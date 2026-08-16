
# Build the CIA named after the current Makefile version so the on-device
# user can tell builds apart. This script does NOT bump the version — the
# version is advanced exactly once per build by the deploy wrapper (a single
# sed), so it increments by ONE and never double-counts across multiple
# make_cia.sh invocations. Makefile uses "VAR	:=	VALUE" (tab-separated).
MK="$(dirname "$0")/../Makefile"
strip_val() { echo "$1" | sed -E 's/^[^:=]*:=[[:space:]]*//' | tr -d '[:space:]'; }
VER_MAJOR=$(strip_val "$(grep -E '^VER_MAJOR'  "$MK" | head -1)")
VER_MINOR=$(strip_val "$(grep -E '^VER_MINOR'  "$MK" | head -1)")
VER_MICRO=$(strip_val "$(grep -E '^VER_MICRO'  "$MK" | head -1)")
APP_VERSION="${VER_MAJOR}.${VER_MINOR}.${VER_MICRO}"

cp ../d1x-3ds.elf .
bannertool makebanner -i banner.png -a jingle.wav -o banner.bnr
bannertool makesmdh -s "D1X 3DS" -l "Port of DXX-Rebirth to 3DS" -p "El iNdioNicarao" -i icon.png -o icon.icn
makerom -f cia -o "d1x-3ds-${APP_VERSION}.cia" -DAPP_ENCRYPTED=false -rsf d1x-3ds.rsf -target t -exefslogo -elf d1x-3ds.elf -icon icon.icn -banner banner.bnr
