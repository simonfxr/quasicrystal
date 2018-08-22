#!/usr/bin/env bash

set -o pipefail

prefix="$1"
out="$2"
jpg_convert_args="$3"
framerate=30
every=1
[[ $4 ]] && every="$4"

sep() {
    local IFS="$1"
    printf -- '%s' "${*:2}"
}

((eff_fr_ms = framerate * 1000 / every))
eff_fr="$((eff_fr_ms / 1000)).$((eff_fr_ms % 1000))"

readarray -t bmps < <(ls -1 "$prefix"*.bmp | sort | sed -n "1p;0~${every}p" )

jpgs=( )
conv_list=( )

for x in "${bmps[@]}"; do
    jpg="${x%.bmp}.jpg"
    jpgs+=( "$jpg" )
    [[ -e $jpg && $jpg -nt $x ]] && continue
    conv_list+=( "$x" "$jpg" )
done

sep $'\n' "${conv_list[@]}" |
    xargs -r -d'\n' -P2 -n2 -- convert $jpg_convert_args || exit $?

mov_tmp=$(mktemp -u --suffix .mp4)
trap 'rm -f "$mov_tmp" 2>/dev/null' EXIT

cat "${jpgs[@]}" |
    ffmpeg -y -framerate "$eff_fr" -f image2pipe -vcodec mjpeg \
                    -i - -vcodec libx264 "$mov_tmp" || exit $?

ffmpeg -y -i "$mov_tmp" "$out"
