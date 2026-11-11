#!/usr/bin/env bash
# Regenerates Assets/Icon/{Icon.png,Icon.ico} from Assets/Icon/Source.png (the
# cow-spotted planet on white). Needs ImageMagick 7. The outputs are committed;
# run this only when the source art changes.
set -euo pipefail

cd "$(dirname "$0")/../Assets/Icon"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Silhouette: flood the near-white background from a corner and from inside the
# ring's openings (white the corner can't reach); everything else is the shape.
# The cream spots stay put: their blue channel is well under the threshold.
magick Source.png -channel RGB -separate -evaluate-sequence min -threshold 97.5% \
    -type truecolor -fill red \
    -draw "color 0,0 floodfill" -draw "color 1113,327 floodfill" \
    -draw "color 401,636 floodfill" -draw "color 472,702 floodfill" \
    -draw "color 1294,318 floodfill" \
    -fill black +opaque red -fill white -opaque red -negate \
    -alpha off -colorspace gray "$tmp/mask.png"
magick "$tmp/mask.png" -morphology Close Disk:2 -morphology Erode Disk:1 -blur 0x1.2 -level 45%,55% "$tmp/body.png"
magick "$tmp/mask.png" -morphology Dilate Disk:9 -blur 0x1.5 -level 45%,55% "$tmp/outline.png"

# The planet keeps its spots and sits on a white outline so the black spots on
# its edge don't run into the black tile.
crop=$(magick "$tmp/outline.png" -format "%@" info:)
magick Source.png "$tmp/body.png" -alpha off -compose CopyOpacity -composite "$tmp/planet.png"
magick -size 1536x1024 xc:white "$tmp/outline.png" -alpha off -compose CopyOpacity -composite \
    "$tmp/planet.png" -compose Over -composite -crop "$crop" +repage "$tmp/glyph.png"

# The bare white silhouette, for sizes too small to show the spots.
magick -size 1536x1024 xc:white "$tmp/body.png" -alpha off -compose CopyOpacity -composite \
    -crop "$crop" +repage "$tmp/silhouette.png"

# tile <canvas> <tile size> <corner radius> <glyph width> <out> [glyph]
tile() {
    local inset=$((($1 - $2) / 2)) last=$(($2 - 1))
    magick -size "$1x$1" xc:none \
        \( -size "$2x$2" gradient:'#262626-#050505' \
           \( -size "$2x$2" xc:black -fill white -draw "roundrectangle 0,0 $last,$last $3,$3" \) \
           -alpha off -compose CopyOpacity -composite \) \
        -geometry "+$inset+$inset" -compose Over -composite \
        \( "${6:-$tmp/glyph.png}" -resize "$4x" \) -gravity center -compose Over -composite \
        "$5"
}

# macOS: Apple's icon grid, an 824 tile on a 1024 canvas, which leaves room for
# the shadow the system draws.
tile 1024 824 185 700 Icon.png

# Windows draws the .ico as-is with no grid of its own, so the tile runs nearly
# to the edge and the planet fills more of it. Below 64 px the spots are noise,
# so those frames carry the plain white silhouette instead. eacp_set_app_icon's
# IMAGE path would downscale one picture to every size; this .ico goes in
# through its ICO argument.
tile 1024 1000 150 900 "$tmp/windows.png"
tile 1024 1000 150 920 "$tmp/small.png" "$tmp/silhouette.png"
frames=()
for size in 16 24 32 48; do
    magick "$tmp/small.png" -resize "${size}x$size" "$tmp/f$size.png"
    frames+=("$tmp/f$size.png")
done
for size in 64 128 256; do
    magick "$tmp/windows.png" -resize "${size}x$size" "$tmp/f$size.png"
    frames+=("$tmp/f$size.png")
done
magick "${frames[@]}" Icon.ico
