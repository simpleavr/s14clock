#!/usr/bin/env bash
# Local web flasher for testing builds with ESP Web Tools (same tool as install.html).
#
#   ./webflash.sh             build, stage and serve on http://localhost:8000
#   ./webflash.sh --no-build  serve the last build without rebuilding
#   PORT=9000 ./webflash.sh   use another port
#
# Open the URL in Chrome or Edge (Web Serial is allowed on localhost).
# Under WSL2, the Windows browser reaches this server through localhost forwarding.
# Everything is staged in .pio/webflash/, which git ignores.
set -euo pipefail

cd "$(dirname "$0")"
PORT="${PORT:-8000}"
ENV=lolin_s2_mini
BUILD=".pio/build/$ENV"
OUT=".pio/webflash"
PIO="$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

if [[ "${1:-}" != "--no-build" ]]; then
	"$PIO" run -e "$ENV"
fi

for f in bootloader.bin partitions.bin firmware.bin; do
	[[ -f "$BUILD/$f" ]] || { echo "missing $BUILD/$f, run without --no-build" >&2; exit 1; }
done

rm -rf "$OUT"
mkdir -p "$OUT"
cp "$BUILD"/{bootloader,partitions,firmware}.bin "$BOOT_APP0" "$OUT/"

FW_VER="$(sed -n 's/^char _version\[[0-9]*\] = "\([^"]*\)".*/\1/p' s14clock_firmware/s14clock_firmware.ino | sed 's/ *$//')"
STAMP="$(date '+%Y-%m-%d %H:%M:%S')"
GIT="$(git rev-parse --short HEAD 2>/dev/null || echo nogit)$(git diff --quiet 2>/dev/null || echo -dirty)"
LABEL="$FW_VER local build $STAMP ($GIT)"

cat > "$OUT/manifest.json" <<EOF
{
  "name": "S14Clock local build",
  "version": "$LABEL",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S2",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
EOF

cat > "$OUT/index.html" <<EOF
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>S14Clock Local Flash</title>
<script type="module" src="https://unpkg.com/tasmota-esp-web-tools@8.1.2/dist/web/install-button.js?module"></script>
<style>
  body { font-family: sans-serif; max-width: 640px; margin: 2em auto; padding: 0 16px; background: #fff; color: #222; }
  code { background: #eee; padding: 1px 4px; }
  @media (prefers-color-scheme: dark) { body { background: #1e1e1e; color: #ddd; } code { background: #333; } }
</style>
</head>
<body>
<h1>S14Clock local flash</h1>
<p><b>$LABEL</b></p>
<ol>
  <li>Bootloader mode: hold <b>0</b>, plug in USB, release <b>0</b>
      (or with USB connected: hold <b>0</b>, tap <b>RST</b>, release <b>0</b>).</li>
  <li>Click <b>Connect</b>, pick the ESP32-S2 port, then <b>Install</b>.
      Tick erase only if you want to wipe settings and WiFi credentials.</li>
  <li>Press <b>RST</b> when done.</li>
</ol>
<esp-web-install-button manifest="manifest.json"></esp-web-install-button>
<p>For OTA testing, upload <a href="firmware.bin" download>firmware.bin</a> at <code>http://&lt;clock-ip&gt;/update</code>.</p>
</body>
</html>
EOF

echo
echo "Staged: $LABEL"
echo "Open http://localhost:$PORT/ in Chrome or Edge (Ctrl+C to stop)"
echo

# Serve with caching off, so a rebuilt firmware.bin is always fetched fresh.
cd "$OUT"
exec python3 - "$PORT" <<'PY'
import sys, http.server
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()
http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
