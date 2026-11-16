#!/bin/sh
# Builds your game into a .wuhb:   sh build.sh
# Edit these first. They are what the Wii U Menu shows.
NAME="My DOS Game"
SHORT_NAME="My DOS Game"
AUTHOR="Your Name"
OUTPUT="MyDOSGame.wuhb"

cd "$(dirname "$0")" || exit 1

# The tool: in tools/, then on the PATH, then in a devkitPro install
if [ -f tools/wuhbtool ]; then
	chmod +x tools/wuhbtool 2>/dev/null
	TOOL=tools/wuhbtool
elif command -v wuhbtool >/dev/null 2>&1; then
	TOOL=wuhbtool
elif [ -x "${DEVKITPRO:-/opt/devkitpro}/tools/bin/wuhbtool" ]; then
	TOOL="${DEVKITPRO:-/opt/devkitpro}/tools/bin/wuhbtool"
else
	echo "wuhbtool not found. Put it in the tools folder (see tools/README.txt)." >&2
	exit 1
fi

if [ ! -f dosbox-wiiu.rpx ]; then
	echo "dosbox-wiiu.rpx not found: it belongs in this folder, next to build.sh." >&2
	exit 1
fi

if [ ! -f content/autoboot.conf ]; then
	echo "content/autoboot.conf not found: it is the conf your game starts with." >&2
	exit 1
fi

"$TOOL" dosbox-wiiu.rpx "$OUTPUT" \
	--content=content \
	--name="$NAME" \
	--short-name="$SHORT_NAME" \
	--author="$AUTHOR" \
	--icon=meta/icon.png \
	--tv-image=meta/tv_splash.png \
	--drc-image=meta/drc_splash.png || exit 1

echo
echo "Built $OUTPUT. Copy it to wiiu/apps/ on your SD card."
