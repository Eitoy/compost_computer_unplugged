// https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/#3
// https://docs.platformio.org/en/latest/boards/espressif32/seeed_xiao_esp32c3.html
// https://wiki.seeedstudio.com/xiao_esp32c6_with_platform_io/
// Some tutorials used

// should use this to upload code to the board
/*
pio run --target upload --environment seeed_xiao_esp32_c6
*/

// command with cleaning
/* 
pio run --target clean --environment seeed_xiao_esp32_c6
pio run --target uploadfs --environment seeed_xiao_esp32_c6
pio run --target upload --environment seeed_xiao_esp32_c6
*/

// ///// check the content
/*
ls -la data/
du -sh data/
*/



///// open the serial monitor 
/*
pio device monitor --environment seeed_xiao_esp32_c6 --baud 115200
*/


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// const char* ssid = "gigacube-646281";
// const char* password = "4qM8yr2m53mr4538";

// const char* ssid = "Guestapartamente";
// const char* password = "0123456789";

// const char* ssid = "The Legacy Bucharest cowork MAIN";
const char* ssid = "The Legacy | Bucharest";
const char* password = "kindness";

WebServer server(80);

String htmlContent = 
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>XIAO Server</title>
    <script src="/p5.min.js"></script>
</head>
<body style='text-align: center; padding-top: 50px;'>
<div style='display: inline-block; padding: 20px; border: 20px solid #ada11a; border-radius: 10px; background-color: #dddbca;'>
<h1>Xiao Server</h1>
<p id="demo">Hello world!</p>
<p>sml</p>
<img src="/trashpick_lg.jpg" style="max-width: 300px;">
<p>lrg</p>
<img src="/trashpick_lg.jpg" style="max-width: 500px;">
</div>
<div>
    <iframe src="/frame1.html" style="border: solid 1px #ccc;"></iframe>
</div>

<button type="button" onclick="textChange()">Click Me!</button>

<script>
function textChange() {
    document.getElementById("demo").innerHTML = "Hello JavaScript!";
}
</script>

</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlContent);
}

void handleImage() {
  File file = LittleFS.open("/trashpick.jpg", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleImage2() {
  File file = LittleFS.open("/trashpick_lg.jpg", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleHTML1() {
  File file = LittleFS.open("/frame1.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleJsP5() {
  File file = LittleFS.open("/p5.min.js", "r");
  if (!file) {
    server.send(404, "text/plain", "JavaScript file not found");
    return;
  }
  server.streamFile(file, "application/javascript");
  file.close();
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address is: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/trashpick.jpg", handleImage);
  server.on("/trashpick_lg.jpg", handleImage2);
  server.on("/frame1.html", handleHTML1);
  server.on("/p5.min.js", handleJsP5);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}