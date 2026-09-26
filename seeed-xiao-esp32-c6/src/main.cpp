// Compost Computer Unplugged - a small gallery server for the Seeed XIAO ESP32S3 Sense
//
// Flash (filesystem first, then firmware):
//   pio run --target uploadfs --environment seeed_xiao_esp32s3
//   pio run --target upload   --environment seeed_xiao_esp32s3
//   pio device monitor --environment seeed_xiao_esp32s3 --baud 115200
//
// Routes:
//   GET  /                 the grid (index.html), one tile per piece in /g
//   GET  /upload.html      add a piece, remove pieces
//   GET  /api/list         JSON: every piece in /g plus free space
//   POST /upload           multipart form: name + file (.jpg or .html)
//   POST /p5               form: name + code, wrapped into a page that loads /p5.min.js
//   POST /delete           form: slug
//   GET  /g/<slug>.<ext>   the piece itself
//   GET  /p5.min.js        shared p5 library, served gzipped from /p5.min.js.gz
//
// WiFi: joins the first reachable network in include/secrets.h. If none is around it becomes an open
// network called "compost" and keeps scanning; when a known network appears it moves over. If the
// connection drops it waits a minute for it to return, then tries the whole list again.
//
// A piece is one file in /g named after its slug: <slug>.jpg, <slug>.html or <slug>.p5.html.
// The folder listing is the only database.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "secrets.h"  // KNOWN_NETWORKS, the WiFi networks to try in order (copy include/secrets.example.h)

const char* HOSTNAME = "compost";  // reachable as http://compost.local
const char* AP_SSID = "compost";   // open network the board creates when none of the known WiFi is around
const unsigned long WIFI_TIMEOUT_MS = 10000;  // per network

const char* GALLERY_DIR = "/g";
const char* TMP_PATH = "/upload.tmp";
const size_t MAX_UPLOAD = 1500 * 1024;  // one photo or html piece
const size_t MAX_CODE = 64 * 1024;     // one pasted p5 sketch
const size_t MIN_FREE = 64 * 1024;     // headroom kept on the filesystem

WebServer server(80);

// ---- upload state (the server handles one request at a time) ----
File uploadFile;
String uploadExt;
String uploadName;
size_t uploadBytes = 0;
enum UploadError { UP_OK, UP_TOO_BIG, UP_BAD_TYPE, UP_FULL, UP_WRITE };
UploadError uploadError = UP_OK;

// ---- helpers ----

// "Trash Pick 2!" -> "trash-pick-2". Only a-z, 0-9 and single dashes survive.
String slugify(const String& in) {
  String out;
  bool dash = false;
  for (size_t i = 0; i < in.length() && out.length() < 40; i++) {
    char c = tolower((unsigned char)in[i]);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out += c;
      dash = false;
    } else if (!dash && out.length() > 0) {
      out += '-';
      dash = true;
    }
  }
  while (out.endsWith("-")) out.remove(out.length() - 1);
  return out;
}

// "photo.JPEG" -> "jpg", "sketch.htm" -> "html", anything else -> ""
String extOf(const String& filename) {
  int dot = filename.lastIndexOf('.');
  if (dot < 0) return "";
  String e = filename.substring(dot + 1);
  e.toLowerCase();
  if (e == "jpg" || e == "jpeg") return "jpg";
  if (e == "html" || e == "htm") return "html";
  return "";
}

String baseName(const String& path) {
  int slash = path.lastIndexOf('/');
  return slash < 0 ? path : path.substring(slash + 1);
}

String pathFor(const String& slug, const String& ext) {
  return String(GALLERY_DIR) + "/" + slug + "." + ext;
}

// A slug owns at most one file, whatever its format.
void removeSlug(const String& slug) {
  const char* exts[] = {"jpg", "html", "p5.html"};
  for (const char* e : exts) {
    String p = pathFor(slug, e);
    if (LittleFS.exists(p)) LittleFS.remove(p);
  }
}

size_t freeBytes() {
  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  return used > total ? 0 : total - used;
}

void redirectHome() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void streamOr404(const String& path, const char* type) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain", "not found");
    return;
  }
  server.streamFile(f, type);
  f.close();
}

// ---- routes ----

// GET /p5.min.js: the one shared copy, stored gzipped. Every sketch tile loads it, so let browsers keep it a day.
void handleP5Lib() {
  File f = LittleFS.open("/p5.min.js.gz", "r");
  if (!f) {
    server.send(404, "text/plain", "not found");
    return;
  }
  server.sendHeader("Cache-Control", "max-age=86400");
  server.streamFile(f, "application/javascript");  // adds Content-Encoding: gzip for a .gz file
  f.close();
}

void handleRoot() {
  streamOr404("/index.html", "text/html");
}

// GET /api/list
void handleList() {
  String json = "{\"free\":" + String(freeBytes()) + ",\"total\":" + String(LittleFS.totalBytes()) + ",\"items\":[";
  File dir = LittleFS.open(GALLERY_DIR);
  bool first = true;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) continue;
    String name = baseName(f.name());
    String type, slug;
    if (name.endsWith(".p5.html")) {
      type = "p5";
      slug = name.substring(0, name.length() - 8);
    } else if (name.endsWith(".html")) {
      type = "html";
      slug = name.substring(0, name.length() - 5);
    } else if (name.endsWith(".jpg")) {
      type = "jpg";
      slug = name.substring(0, name.length() - 4);
    } else {
      continue;
    }
    if (!first) json += ",";
    first = false;
    json += "{\"slug\":\"" + slug + "\",\"type\":\"" + type + "\",\"url\":\"" + String(GALLERY_DIR) + "/" + name +
            "\",\"size\":" + String(f.size()) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// POST /upload, called for every chunk of the file part
void handleUploadChunk() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadError = UP_OK;
    uploadBytes = 0;
    uploadName = up.filename;
    uploadExt = extOf(up.filename);
    if (uploadExt == "") {
      uploadError = UP_BAD_TYPE;
      return;
    }
    if (freeBytes() < MIN_FREE) {
      uploadError = UP_FULL;
      return;
    }
    if (LittleFS.exists(TMP_PATH)) LittleFS.remove(TMP_PATH);
    uploadFile = LittleFS.open(TMP_PATH, "w");
    if (!uploadFile) uploadError = UP_WRITE;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadError != UP_OK) return;
    uploadBytes += up.currentSize;
    if (uploadBytes > MAX_UPLOAD) {
      uploadError = UP_TOO_BIG;
      uploadFile.close();
      LittleFS.remove(TMP_PATH);
      return;
    }
    if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
      uploadError = UP_WRITE;
      uploadFile.close();
      LittleFS.remove(TMP_PATH);
      return;
    }
    // every 64 KB or so, make sure the filesystem is not filling up under us
    if ((uploadBytes / up.currentSize) % 45 == 0 && freeBytes() < MIN_FREE) {
      uploadError = UP_FULL;
      uploadFile.close();
      LittleFS.remove(TMP_PATH);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    LittleFS.remove(TMP_PATH);
    uploadError = UP_WRITE;
  }
}

// POST /upload, called once the whole form has arrived (form fields are readable here)
void handleUploadDone() {
  switch (uploadError) {
    case UP_TOO_BIG: server.send(413, "text/plain", "That file is too big. Keep it under 1500 KB."); return;
    case UP_BAD_TYPE: server.send(415, "text/plain", "Only .jpg or .html files, please."); return;
    case UP_FULL: server.send(507, "text/plain", "The compost is full. Remove something first."); return;
    case UP_WRITE: server.send(500, "text/plain", "Could not save the file. Try again."); return;
    default: break;
  }
  if (uploadBytes == 0 || !LittleFS.exists(TMP_PATH)) {
    LittleFS.remove(TMP_PATH);
    server.send(400, "text/plain", "No file received.");
    return;
  }

  String slug = slugify(server.arg("name"));
  if (slug == "") slug = slugify(uploadName.substring(0, uploadName.lastIndexOf('.')));
  if (slug == "") slug = "untitled";

  removeSlug(slug);
  if (!LittleFS.rename(TMP_PATH, pathFor(slug, uploadExt))) {
    LittleFS.remove(TMP_PATH);
    server.send(500, "text/plain", "Could not save the file. Try again.");
    return;
  }
  Serial.printf("added /g/%s.%s (%u bytes)\n", slug.c_str(), uploadExt.c_str(), uploadBytes);
  redirectHome();
}

// POST /p5: name + code, wrapped into a page that loads the shared p5 library
void handleP5() {
  String slug = slugify(server.arg("name"));
  String code = server.arg("code");
  if (slug == "") {
    server.send(400, "text/plain", "Give the sketch a name.");
    return;
  }
  if (code.length() == 0) {
    server.send(400, "text/plain", "Paste some sketch code.");
    return;
  }
  if (code.length() > MAX_CODE) {
    server.send(413, "text/plain", "That sketch is too big. Keep it under 64 KB.");
    return;
  }
  if (freeBytes() < code.length() + 1024 + MIN_FREE) {
    server.send(507, "text/plain", "The compost is full. Remove something first.");
    return;
  }
  code.replace("</script", "<\\/script");  // keep a stray closing tag from ending the page early

  removeSlug(slug);
  File f = LittleFS.open(pathFor(slug, "p5.html"), "w");
  if (!f) {
    server.send(500, "text/plain", "Could not save the sketch. Try again.");
    return;
  }
  f.print("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
          "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
          "<title>");
  f.print(slug);
  f.print("</title><script src=\"/p5.min.js\"></script>"
          "<style>html,body{margin:0;padding:0;background:#2b2b2b;overflow:hidden}canvas{display:block;max-width:100%;height:auto!important;margin:0 auto}</style>"
          "</head><body><script>\n");
  f.print(code);
  f.print("\n</script></body></html>\n");
  f.close();
  Serial.printf("added /g/%s.p5.html (%u bytes)\n", slug.c_str(), code.length());
  redirectHome();
}

// POST /delete: slug
void handleDelete() {
  String slug = slugify(server.arg("slug"));
  if (slug == "") {
    server.send(400, "text/plain", "no slug");
    return;
  }
  removeSlug(slug);
  Serial.printf("removed %s\n", slug.c_str());
  server.send(200, "text/plain", "ok");
}

// Print why a join attempt failed (reason 15 or 202 is usually a wrong password, 201 no such network).
void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf(" [disconnected, reason %d]", info.wifi_sta_disconnected.reason);
}

// List what is on air, so a wrong name, a 5GHz-only network or an odd character is easy to spot.
void printVisibleNetworks() {
  int n = WiFi.scanNetworks();
  Serial.printf("Networks visible on 2.4GHz: %d\n", n);
  for (int i = 0; i < n; i++) {
    String name = WiFi.SSID(i);
    Serial.printf("  %-32s ch %2d  %d dBm", name.c_str(), WiFi.channel(i), WiFi.RSSI(i));
    bool ascii = true;
    for (size_t k = 0; k < name.length(); k++) {
      unsigned char c = name[k];
      if (c < 32 || c > 126) ascii = false;
    }
    if (!ascii) {
      Serial.print("  bytes:");
      for (size_t k = 0; k < name.length(); k++) Serial.printf(" %02x", (unsigned char)name[k]);
    }
    Serial.println();
  }
  WiFi.scanDelete();
}

// ---- WiFi: stay on the best known network, or serve as our own network until one shows up ----

const int KNOWN_COUNT = sizeof(KNOWN_NETWORKS) / sizeof(KNOWN_NETWORKS[0]);
const unsigned long NET_CHECK_MS = 30000;      // how often loop() looks at the connection
const unsigned long BETTER_SCAN_MS = 120000;   // on a lower-ranked network: how often to look for a better one
const unsigned long OWN_SCAN_MS = 60000;       // serving our own network: how often to look for a known one
const unsigned long OFFLINE_GRACE_MS = 60000;  // how long the core may retry a lost network before we try them all

enum NetMode { NET_NONE, NET_KNOWN, NET_OWN };
NetMode netMode = NET_NONE;
int netIndex = -1;  // which KNOWN_NETWORKS entry we are on, while netMode == NET_KNOWN
unsigned long lastNetCheck = 0, lastScanStart = 0, offlineSince = 0;
bool scanRunning = false;

// Announce compost.local again; needed after every new connection.
void announceName() {
  MDNS.end();
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("http://%s.local\n", HOSTNAME);
  }
}

// Try one known network for up to WIFI_TIMEOUT_MS; true once it gives us an address.
bool tryNetwork(int i) {
  const KnownNetwork& net = KNOWN_NETWORKS[i];
  Serial.printf("Trying %s", net.ssid);
  WiFi.begin(net.ssid, net.password);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    netMode = NET_KNOWN;
    netIndex = i;
    offlineSince = 0;
    Serial.printf("Joined %s, IP: ", net.ssid);
    Serial.println(WiFi.localIP());
    announceName();
    return true;
  }
  Serial.printf("  not joined (status %d)\n", WiFi.status());
  WiFi.disconnect(false);
  delay(200);
  return false;
}

// Try every known network in order; true once one of them works.
bool joinKnownNetwork() {
  for (int i = 0; i < KNOWN_COUNT; i++) {
    if (tryNetwork(i)) return true;
  }
  return false;
}

// No known WiFi around: become the network, so visitors can still join "compost".
void startOwnNetwork() {
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP_STA);  // AP for visitors, STA kept alive so we can still scan for known networks
  WiFi.softAP(AP_SSID);
  netMode = NET_OWN;
  netIndex = -1;
  lastScanStart = millis();
  Serial.printf("Started own network \"%s\", IP: ", AP_SSID);
  Serial.println(WiFi.softAPIP());
  announceName();
}

// Back on a known network: stop serving our own.
void stopOwnNetwork() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.println("Own network stopped");
}

// Start a scan in the background; maintainWifi() picks up the result.
void startScan() {
  WiFi.scanNetworks(true);
  scanRunning = true;
  lastScanStart = millis();
}

// Best-ranked known network (lowest index below `limit`) among the scan results, or -1.
int bestVisibleKnown(int found, int limit) {
  int best = -1;
  for (int r = 0; r < found; r++) {
    String seen = WiFi.SSID(r);
    for (int i = 0; i < limit; i++) {
      if (seen == KNOWN_NETWORKS[i].ssid && (best < 0 || i < best)) best = i;
    }
  }
  return best;
}

// A better network is around: move to it, or go back to what we had if that fails.
void moveTo(int best, int limit) {
  int previous = netIndex;
  bool wasOwn = (netMode == NET_OWN);
  for (int i = best; i < limit; i++) {
    if (tryNetwork(i)) {
      if (wasOwn) stopOwnNetwork();
      return;
    }
  }
  if (!wasOwn && previous >= 0) tryNetwork(previous);
}

// Called from loop(): notice a lost connection, look for better networks, leave own-network mode when possible.
void maintainWifi() {
  unsigned long now = millis();

  if (scanRunning) {
    int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) return;
    scanRunning = false;
    int limit = (netMode == NET_KNOWN) ? netIndex : KNOWN_COUNT;  // on a known network only higher-ranked ones count
    int best = (found > 0) ? bestVisibleKnown(found, limit) : -1;
    WiFi.scanDelete();
    if (best >= 0) {
      Serial.printf("%s is around, moving to it\n", KNOWN_NETWORKS[best].ssid);
      moveTo(best, limit);
    } else {
      Serial.printf("scan: %d networks, nothing better than %s\n", found,
                    netMode == NET_KNOWN ? KNOWN_NETWORKS[netIndex].ssid : "our own network");
    }
    return;
  }

  if (now - lastNetCheck < NET_CHECK_MS) return;
  lastNetCheck = now;

  if (netMode == NET_KNOWN) {
    if (WiFi.status() == WL_CONNECTED) {
      offlineSince = 0;
      if (netIndex > 0 && now - lastScanStart >= BETTER_SCAN_MS) startScan();
    } else if (offlineSince == 0) {
      offlineSince = now;
      Serial.println("WiFi lost, waiting for it to come back");
    } else if (now - offlineSince >= OFFLINE_GRACE_MS) {
      Serial.println("Still offline, trying every known network");
      if (!joinKnownNetwork()) startOwnNetwork();
    }
  } else if (netMode == NET_OWN) {
    if (now - lastScanStart >= OWN_SCAN_MS) startScan();
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) Serial.println("LittleFS mount failed");
  if (!LittleFS.exists(GALLERY_DIR)) LittleFS.mkdir(GALLERY_DIR);
  if (LittleFS.exists(TMP_PATH)) LittleFS.remove(TMP_PATH);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  printVisibleNetworks();
  if (!joinKnownNetwork()) startOwnNetwork();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/list", HTTP_GET, handleList);
  server.on("/upload", HTTP_POST, handleUploadDone, handleUploadChunk);
  server.on("/p5", HTTP_POST, handleP5);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/p5.min.js", HTTP_GET, handleP5Lib);
  server.serveStatic("/", LittleFS, "/");  // /g/<piece>, /upload.html
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();

  Serial.printf("gallery: %u bytes free of %u\n", freeBytes(), LittleFS.totalBytes());
}

void loop() {
  server.handleClient();
  maintainWifi();
}
