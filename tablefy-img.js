#!/usr/bin/env node


const fs = require("fs");
const path = require("path");
const Jimp = require("jimp");

function printUsage() {
  console.error(
    "usage: pixeltable.js [-h] [-m] file\n\n" +
      "Convert an image to a HTML table\n\n" +
      "positional arguments:\n" +
      "  file           filename of the image to convert\n\n" +
      "options:\n" +
      "  -h, --help     show this help message and exit\n" +
      "  -m, --min      create minified HTML code"
  );
}

function parseArgs(argv) {
  let min = false;
  let file = null;

  for (const arg of argv) {
    if (arg === "-h" || arg === "--help") {
      printUsage();
      process.exit(0);
    } else if (arg === "-m" || arg === "--min") {
      min = true;
    } else {
      file = arg;
    }
  }

  if (!file) {
    printUsage();
    process.exit(1);
  }

  return { min, file };
}

function triplet(r, g, b) {
  return (((r << 16) | (g << 8) | b) >>> 0).toString(16).padStart(6, "0");
}

async function main() {
  const { min, file } = parseArgs(process.argv.slice(2));

  const parsed = path.parse(file);
  const outPath = path.join(parsed.dir, parsed.name + ".html");

  const newline = min ? "" : "\n";
  const tab = min ? "" : "  ";

  const image = await Jimp.read(file);
  const width = image.bitmap.width;
  const height = image.bitmap.height;

  const colors = [];
  for (let y = 0; y < height; y++) {
    const row = [];
    for (let x = 0; x < width; x++) {
const rgba = Jimp.intToRGBA(image.getPixelColor(x, y));
row.push(triplet(rgba.r, rgba.g, rgba.b));
    }
    colors.push(row);
  }

  const out = [];
  out.push("<html>");
  out.push(tab + "<head>" + newline);
  out.push(tab.repeat(2) + '<style type="text/css">' + newline);
  out.push(tab.repeat(3) + "table.pixeltable" + newline);
  out.push(tab.repeat(3) + "{" + newline);
  out.push(tab.repeat(3) + "border-spacing: 0;" + newline);
  out.push(tab.repeat(3) + "border-collapse: collapse;" + newline);
  out.push(tab.repeat(3) + "width: " + width + "px;" + newline);
  out.push(tab.repeat(3) + "height: " + height + "px;" + newline);
  out.push(tab.repeat(2) + "}" + newline);
  out.push(tab.repeat(2) + ".pixeltable td" + newline);
  out.push(tab.repeat(2) + "{" + newline);
  out.push(tab.repeat(3) + "width: 1px;" + newline);
  out.push(tab.repeat(3) + "height: 1px;" + newline);
  out.push(tab.repeat(3) + "border: 0;" + newline);
  out.push(tab.repeat(3) + "padding: 0;" + newline);
  out.push(tab.repeat(2) + "}" + newline);
  out.push(tab.repeat(2) + "</style>" + newline);
  out.push(tab + "</head>" + newline);
  out.push(tab + '<body bgcolor="#ffffff">' + newline);
  out.push(tab.repeat(2) + '<table class="pixeltable">' + newline);

  for (let y = 0; y < height; y++) {
    out.push(tab.repeat(3) + "<tr>" + newline);
    for (let x = 0; x < width; x++) {
      out.push(tab.repeat(3) + '<td bgcolor="#' + colors[y][x] + '"></td>' + newline);
    }
    out.push(tab.repeat(3) + "</tr>" + newline);
  }

  out.push(tab.repeat(2) + "</table>" + newline);
  out.push(tab + "</body>" + newline);
  out.push("</html>" + newline);

  fs.writeFileSync(outPath, out.join(""));
  console.log("Wrote " + outPath);
}

main().catch((err) => {
  console.error("Error: " + err.message);
  process.exit(1);
});