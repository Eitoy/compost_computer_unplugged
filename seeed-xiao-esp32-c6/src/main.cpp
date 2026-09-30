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

////
// Partition svg mode gives ~1.5MB for app and ~2.5MB for LittleFS 

////


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// const char* ssid = "gigacube-646281";
// const char* password = "4qM8yr2m53mr4538";

// const char* ssid = "Guestapartamente";
// const char* ssid = "Guestapartamente_5G";
// const char* password = "0123456789";

// const char* ssid = "The Legacy Bucharest cowork MAIN";
const char* ssid = "The Legacy | Bucharest";
const char* password = "kindness";

// const char* ssid = "Coliberator_02";
// const char* password = "coliberator";

WebServer server(80);

String htmlContent = 
R"rawliteral(


<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1, viewport-fit=cover"
    />
    <title>tea ceremony</title>
    <style>
      html {
        background: #2e2c2c;
        background-image: url('/noise.svg');
        background-blend-mode: exclusion;
      }
      body {
        margin: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      button {
        font: 14px monospace;
        margin: 10px;
      }
      #wrap {
        overflow-x: auto;
        max-width: 100%;
        background: #23242381;
      }
      #plan {
        position: relative;
        width: max-content;
        font: var(--fs, 18px) / var(--lh, 21px) monospace;
      }
      #plan pre {
        margin: 0;
        font: inherit;
        line-height: inherit;
      }
      .room {
        position: absolute;
        overflow: hidden;
        cursor: pointer;
        left: calc(var(--c) * 1ch - 1.9%);
        top: calc(var(--r) * var(--lh));
        width: calc(var(--w) * 1ch);
        height: calc(var(--h) * var(--lh));
      }
      #plan pre span {
        display: inline-block;
        vertical-align: top;
        height: var(--lh);
        overflow: hidden;
        white-space: pre;
      }
      .room iframe {
        position: absolute;
        left: -8%;
        top: -8%;
        width: 110%;
        height: 110%;
        margin: 0;
        padding: 0;
        border: 0;
        display: block;
        pointer-events: none;
        filter: contrast(0.4);
        mix-blend-mode: color-dodge;
      }
    </style>
  </head>
  <body>
    <button id="rnd">regenerate</button>
    <div id="wrap"><div id="plan"></div></div>

    <script>
      const $ = (s) => document.querySelector(s);
      
      //ADD YOUR HTML HEREEEEEEE
  const ROOM_PAGES = [
  ["/frame1.html", "/frame2.html"], //Shinji
  ["/skyline.p5.html"], //David
  ["/stereogram.html"], //Mariana
  ["/n002.png", "/n004.html", "/n005.png"], //Orsi
  ["/horia-stefania.p5.html"], //extra?
];
      const POPUP = "popup=yes, scrollbar=no, resize=no, width=320,height=320";

      const hash = (s) => {
        let h = 1779033703 ^ s.length;
        for (let i = 0; i < s.length; i++) {
          h = Math.imul(h ^ s.charCodeAt(i), 3432918353);
          h = (h << 13) | (h >>> 19);
        }
        return h >>> 0;
      };
      const mk = (a) => () => {
        a |= 0;
        a = (a + 0x6d2b79f5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
      };


      const E = [];
      for (let i = 0; i < 5; i++)
        for (let j = i + 1; j < 5; j++) E.push([i, j]);
      const perms = (a) =>
        a.length < 2
          ? [a]
          : a.flatMap((x, i) =>
              perms([...a.slice(0, i), ...a.slice(i + 1)]).map((p) => [
                x,
                ...p,
              ]),
            );
      const P5 = perms([0, 1, 2, 3, 4]);
      const bits = (n) => n.toString(2).replace(/0/g, "").length;
      const IDX = {};
      E.forEach(([a, b], k) => (IDX[a * 5 + b] = k));
      const edgesOf = (m) => E.filter((_, k) => (m >> k) & 1);

      function connected(m) {
        const adj = [[], [], [], [], []];
        edgesOf(m).forEach(([a, b]) => {
          adj[a].push(b);
          adj[b].push(a);
        });
        const s = new Set([0]),
          q = [0];
        while (q.length)
          adj[q.pop()].forEach((w) => {
            if (!s.has(w)) {
              s.add(w);
              q.push(w);
            }
          });
        return s.size == 5;
      }
      function canon(m) {
        let best = 1e9;
        for (const p of P5) {
          let n = 0;
          edgesOf(m).forEach(([a, b]) => {
            let x = p[a],
              y = p[b];
            if (x > y) [x, y] = [y, x];
            n |= 1 << IDX[x * 5 + y];
          });
          if (n < best) best = n;
        }
        return best;
      }
      const FORMS = (() => {
        const s = new Set();
        for (let m = 1; m < 1024; m++) if (connected(m)) s.add(canon(m));
        return [...s].sort((a, b) => bits(a) - bits(b) || a - b);
      })();

      function place(edges, R) {
        const res = [],
          asg = [],
          used = Array(9).fill(0);
        const cost = () => {
          let c = R() * 0.4;
          const occ = new Set(asg);
          edges.forEach(([a, b]) => {
            const A = asg[a],
              B = asg[b],
              ra = (A / 3) | 0,
              ca = A % 3,
              rb = (B / 3) | 0,
              cb = B % 3;
            c += Math.abs(ra - rb) + Math.abs(ca - cb);
            if (ca != cb) {
              const s = ca < cb ? 1 : -1;
              for (let x = ca + s; x != cb; x += s)
                if (occ.has(ra * 3 + x)) c += 6;
            }
            if (ra != rb) {
              const s = ra < rb ? 1 : -1;
              for (let y = ra + s; y != rb; y += s)
                if (occ.has(y * 3 + cb)) c += 6;
            }
          });
          return c;
        };
        (function go(k) {
          if (k == 5) {
            res.push([cost(), [...asg]]);
            return;
          }
          for (let s = 0; s < 9; s++)
            if (!used[s]) {
              used[s] = 1;
              asg[k] = s;
              go(k + 1);
              used[s] = 0;
            }
        })(0);
        res.sort((a, b) => a[0] - b[0]);
        return res[(R() * Math.min(3, res.length)) | 0][1];
      }

      const COL = {
        room: "#737739",
        door: "#786742",
        corridor: "#786742",
        corrWall: "#737739",
        border: "#737739",
        path: "#ccc",
        rock: "#737739",
        tree: "#737739",
        water: "#7786b3",
        shore: "#778689",
      };

      function build(fi, seed) {
        const R = mk(hash(seed + ":" + fi)),
          edges = edgesOf(FORMS[fi]),
          asg = place(edges, R);
        const grass = () =>
          `hsl(${85 + R() * 50},${40 + R() * 25}%,${28 + R() * 22}%)`;
        const adj = [[], [], [], [], []];
        edges.forEach(([a, b]) => {
          adj[a].push(b);
          adj[b].push(a);
        });
        const minD = Math.min(...adj.map((a) => a.length));
        const cand = [0, 1, 2, 3, 4].filter((n) => adj[n].length == minD);
        const e0 = cand[(R() * cand.length) | 0];
        const dist = Array(5).fill(9);
        dist[e0] = 0;
        const q = [e0];
        while (q.length) {
          const v = q.shift();
          adj[v].forEach((w) => {
            if (dist[w] == 9) {
              dist[w] = dist[v] + 1;
              q.push(w);
            }
          });
        }
        const order = [0, 1, 2, 3, 4].sort(
            (a, b) => dist[a] - dist[b] || R() - 0.5,
          ),
          role = [];
        order.forEach((n, i) => (role[n] = i));

        const rs = asg.map((s) => (s / 3) | 0),
          cs = asg.map((s) => s % 3);
        const r0 = Math.min(...rs),
          c0 = Math.min(...cs);
        const SW = 18,
          SH = 9,
          DR = 3,
          DC = 6;
        const rows = (Math.max(...rs) - r0 + 1) * SH + 2 * DR;
        const cols = (Math.max(...cs) - c0 + 1) * SW + 2 * DC;
        const pick = (a) => a[(R() * a.length) | 0];
        const g = Array.from({ length: rows }, () => Array(cols).fill(" "));
        const C = Array.from({ length: rows }, () => Array(cols).fill(null));
        const cor = new Set();
        const put = (y, x, ch, c) => {
          g[y][x] = ch;
          C[y][x] = c;
        };
        const box = (n) => [
          (rs[n] - r0) * SH + 1 + DR,
          (cs[n] - c0) * SW + 1 + DC,
        ];
        const ctr = (n) => {
          const [t, l] = box(n);
          return [t + 3, l + 7];
        };
        const inBox = (y, x) =>
          [0, 1, 2, 3, 4].some((n) => {
            const [t, l] = box(n);
            return y >= t && y <= t + 6 && x >= l && x <= l + 15;
          });
        const inside = (y, x) => y > 0 && x > 0 && y < rows - 1 && x < cols - 1;
        const cell = (y, x) => {
          if (inside(y, x)) {
            put(y, x, "▓", COL.corridor);
            cor.add(y + "," + x);
          }
        };

        edges.forEach(([a, b]) => {
          const [ya, xa] = ctr(a),
            [yb, xb] = ctr(b),
            wide = R() < 0.35;
          for (let x = Math.min(xa, xb); x <= Math.max(xa, xb); x++) {
            cell(ya, x);
            if (wide) cell(ya + 1, x);
          }
          for (let y = Math.min(ya, yb); y <= Math.max(ya, yb); y++) {
            cell(y, xb);
            if (wide) cell(y, xb + 1);
          }
        });

        // corridor walls
        [...cor].forEach((z) => {
          const [y, x] = z.split(",").map(Number);
          for (let dy = -1; dy <= 1; dy++)
            for (let dx = -1; dx <= 1; dx++)
              if (inside(y + dy, x + dx) && g[y + dy][x + dx] == " ")
                put(y + dy, x + dx, "-", COL.corrWall);
        });

        // rooms
        const pages = [];
        for (let n = 0; n < 5; n++) {
          const [t, l] = box(n);
          for (let y = t; y <= t + 6; y++)
            for (let x = l; x <= l + 15; x++) {
              const edge = y == t || y == t + 6 || x == l || x == l + 15;
              if (!edge) put(y, x, " ", null);
              else if (cor.has(y + "," + x)) put(y, x, "▚", COL.door);
              else put(y, x, "░", COL.room);
            }
          pages[role[n]] = {
            r: t + 1,
            c: l + 2,
            w: 14,
            h: 5,
             art: pick(ROOM_PAGES[role[n]]),  
          };
        }

        // terrain
        const ponds = Array.from({ length: 3 }, () => [
          R() * rows,
          R() * cols,
          4 + R() * 5,
        ]);
        const groves = Array.from({ length: 7 }, () => [
          R() * rows,
          R() * cols,
          4 + R() * 6,
        ]);
        for (let y = 1; y < rows - 1; y++)
          for (let x = 1; x < cols - 1; x++) {
            if (g[y][x] != " " || inBox(y, x)) continue;
            const pd = Math.min(
              ...ponds.map((p) => Math.hypot((y - p[0]) * 2, x - p[1]) / p[2]),
            );
            const gv = groves.some(
              (p) => Math.hypot((y - p[0]) * 2, x - p[1]) < p[2],
            );
            const w = Math.sin(x * 0.35) + Math.cos(y * 0.6 + x * 0.1);
            const r = R();
            if (pd < 0.7) put(y, x, ".", COL.water);
            else if (pd < 1) put(y, x, "¦", COL.shore);
            else if (gv && r < 0.4) {
              const ch = pick([".", ".", ".", "▚"]);
              put(y, x, ch, ch == "▚" ? COL.tree : grass());
            } else if (r < 0.08) put(y, x, "░", COL.rock);
            else if (r < 0.22) {
              if (w > 0.1) put(y, x, "^", grass());
              else if (w > -0.1) put(y, x, ".", grass());
              else put(y, x, "░", COL.rock);
            }
          }

        // border + gate
        const gc = cols - DC - 4 - ((R() * (cols - 2 * DC - 8)) | 0);
        for (let x = 0; x < cols; x++) {
          put(0, x, "-", COL.border);
          put(rows - 1, x, "-", COL.border);
        }
        for (let y = 0; y < rows; y++) {
          put(y, 0, "|", COL.border);
          put(y, cols - 1, "|", COL.border);
        }
        [
          [0, 0],
          [0, cols - 1],
          [rows - 1, 0],
          [rows - 1, cols - 1],
        ].forEach(([y, x]) => put(y, x, "+", COL.border));
        for (let x = gc - 1; x <= gc + 1; x++) put(rows - 1, x, " ", null);
        put(rows - 1, gc - 2, "|", COL.border);
        put(rows - 1, gc + 2, "|", COL.border);

        const [et, el] = box(e0),
          ex = el + 7,
          py = rows - 3;
        const pth = (y, x) => {
          if (
            inside(y, x) &&
            !inBox(y, x) &&
            !cor.has(y + "," + x) &&
            g[y][x] != "░"
          )
            put(y, x, "-", COL.path);
        };
        for (let x = Math.min(gc, ex); x <= Math.max(gc, ex); x++)
          if ((x + gc) % 2 == 0) pth(py, x);
        for (let y = rows - 2; y > py; y--) if (y % 2) pth(y, gc);
        for (let y = et + 7; y <= py; y++) if ((y + et) % 2 == 0) pth(y, ex);
        put(et + 6, ex, "+", COL.door);

        return { g, C, pages, cols };
      }

      function toHTML(g, C) {
        return g
          .map((row, y) => {
            let out = "",
              run = "",
              cur = null;
            const flush = () => {
              if (run)
                out += `<span style="width:${run.length}ch${cur ? ";color:" + cur : ""}">${run}</span>`;
              run = "";
            };
            row.forEach((ch, x) => {
              const c = ch == " " ? null : C[y][x];
              if (c !== cur) {
                flush();
                cur = c;
              }
              run += ch;
            });
            flush();
            return out;
          })
          .join("\n");
      }

      function fit(cols) {
        const fs = Math.max(8, Math.min(20, (innerWidth - 32) / (cols * 0.61)));
        const p = $("#plan");
        p.style.setProperty("--fs", fs + "px");
        p.style.setProperty("--lh", fs * 1.3 + "px");
      }

      let lastCols = 0;
      function render() {
        const fi = (Math.random() * FORMS.length) | 0,
          seed = Math.random().toString(36).slice(2, 7);
        const B = build(fi, seed),
          P = $("#plan");
        lastCols = B.cols;
        P.innerHTML = `<pre>${toHTML(B.g, B.C)}</pre>`;
        B.pages.forEach((p) => {
          const d = document.createElement("div");
          d.className = "room";
          d.style.cssText = `--c:${p.c};--r:${p.r};--w:${p.w};--h:${p.h}`;
          d.innerHTML = `<iframe scrolling="no" frameborder="0" src="${p.art}"></iframe>`;
          d.firstChild.onload = (e) => {
            try {
              const st = e.target.contentDocument.createElement("style");
              st.textContent =
                "html,body{margin:0;padding:0;overflow:hidden}table,thead,tbody,tfoot,tr,th,td{border:0!important;outline:0!important;border-collapse:collapse!important;border-spacing:0!important;padding:0!important;margin:0!important}";
              e.target.contentDocument.head.append(st);
            } catch (_) {}
          };
          d.onclick = () => window.open(p.art, "_blank", POPUP);
          P.append(d);
        });
        fit(B.cols);
      }

      $("#rnd").onclick = render;
      addEventListener("resize", () => fit(lastCols));
      render();
    </script>
  </body>
</html>

)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlContent);
}

void handleSvg() {
  File file = LittleFS.open("/noise.svg", "r");
  if (!file) {
    server.send(404, "text/plain", "SVG file not found");
    return;
  }
  server.streamFile(file, "image/svg+xml");
  file.close();
}


// htmls st

void handleImage() {
  File file = LittleFS.open("/st_img1.png", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}

void handleImage2() {
  File file = LittleFS.open("/st_img2.png", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
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

void handleHTML2() {
  File file = LittleFS.open("/frame2.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}


// orsi images

void handleImageOrs2() {
  File file = LittleFS.open("/n002.png", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}

void handleImageOrs3() {
  File file = LittleFS.open("/n005.png", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}
// orsi htmls

void handleHTMLors1() {
  File file = LittleFS.open("/n004.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleHTMdavid() {
  File file = LittleFS.open("/skyline.p5.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleHTMmariana() {
  File file = LittleFS.open("/stereogram.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleHTMstefania() {
  File file = LittleFS.open("/horia-stefania.p5.html", "r");
  if (!file) {
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// p5 lib

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
  server.on("/noise.svg", handleSvg);
  server.on("/n004.html", handleHTMLors1);
  server.on("/n002.png", handleImageOrs2);
  server.on("/n005.png", handleImageOrs3);
  server.on("/st_img1.png", handleImage);
  server.on("/st_img2.png", handleImage2);
  server.on("/frame1.html", handleHTML1);
  server.on("/frame2.html", handleHTML2);
  server.on("/skyline.p5.html", handleHTMdavid);
  server.on("/stereogram.html", handleHTMmariana);
  server.on("/horia-stefania.p5.html", handleHTMstefania);
  server.on("/p5.min.js", handleJsP5);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}