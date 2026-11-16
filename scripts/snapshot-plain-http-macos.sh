#!/usr/bin/env bash
# Shows whether the built Grazer.app loads plain http:// pages: serves a test
# page over http on this Mac's network address, then saves Grazer --snapshot
# PNGs of it and of http://info.cern.ch (a named public host, the kind App
# Transport Security blocks over http) at a wide and a narrow window size, and
# prints the server's access log. Also snapshots the palette over both pages
# and an https page, to compare the Not secure markers with an https page.
#
#   scripts/snapshot-plain-http-macos.sh <output-dir>
set -euo pipefail

output_dir=${1:?usage: snapshot-plain-http-macos.sh <output-dir>}
repo_root=$(cd "$(dirname "$0")/.." && pwd)
grazer="$repo_root/build/Grazer.app/Contents/MacOS/Grazer"
port=8765
wide_size=1600x900
narrow_size=560x900
public_plain_http_url=http://info.cern.ch/
https_url=https://example.com/

address=$(ipconfig getifaddr en0 || ipconfig getifaddr en1)
page_dir=$(mktemp -d)
mkdir -p "$output_dir"
access_log="$output_dir/http-server-access.log"

cat > "$page_dir/index.html" <<'HTML'
<!doctype html><title>Plain http test page</title>
<body style="font: 32px sans-serif; background: #fff8e0; padding: 40px">
Served over plain http by snapshot-plain-http-macos.sh</body>
HTML

python3 -m http.server "$port" --bind "$address" --directory "$page_dir" 2> "$access_log" &
server=$!
trap 'kill $server; rm -rf "$page_dir"' EXIT
sleep 1

local_url="http://$address:$port/"

for size in "$wide_size" "$narrow_size"; do
    "$grazer" --snapshot "$output_dir/lan-$size.png" --size "$size" "$local_url"
    "$grazer" --snapshot "$output_dir/public-$size.png" --size "$size" "$public_plain_http_url"
    "$grazer" --snapshot "$output_dir/https-$size.png" --size "$size" "$https_url"
done

"$grazer" --snapshot "$output_dir/palette-$wide_size.png" --size "$wide_size" --palette \
    "$https_url" "$public_plain_http_url" "$local_url"

echo "--- access log for $local_url"
cat "$access_log"
