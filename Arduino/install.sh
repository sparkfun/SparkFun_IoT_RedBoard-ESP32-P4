#!/usr/bin/env bash
# Install the ESP32-C5 drop-in (drop-in/) into an arduino-esp32 3.3.12 core.
# macOS and Linux. Windows: install.ps1.
#
#   ./install.sh                     this board only (default)
#   ./install.sh --all-p4            patch the stock libs for every ESP32-P4 board
#   ./install.sh --uninstall         undo either
#   ./install.sh --arduino-dir DIR   Arduino data folder, if not the default
#
# Close the Arduino IDE first.

set -euo pipefail

CORE_VERSION=3.3.12
BOARD=sparkfun_esp32p4_iot_redboard_wifi
LIBS_DIR_NAME=esp32p4-c5-libs
BEGIN_MARK="# BEGIN SparkFun RedBoard ESP32-P4 C5 drop-in"
END_MARK="# END SparkFun RedBoard ESP32-P4 C5 drop-in"
DROP_IN_FILES=(
    lib/libespressif__esp_hosted.a
    lib/libespressif__esp_wifi_remote.a
    qio_qspi/include/sdkconfig.h
    dio_qspi/include/sdkconfig.h
    sdkconfig
)

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DROP_IN="$HERE/drop-in"
BOARD_FILES="$HERE"   # boards.txt and variants/

die()  { echo "error: $*" >&2; exit 1; }
info() { echo "  $*"; }

usage() { sed -n '2,11p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

mode=board
arduino_dir=""
while [ $# -gt 0 ]; do
    case "$1" in
        --all-p4)      mode=all ;;
        --uninstall)   mode=uninstall ;;
        --arduino-dir) [ $# -ge 2 ] || usage 1; arduino_dir="$2"; shift ;;
        -h|--help)     usage 0 ;;
        *)             echo "unknown option: $1" >&2; usage 1 ;;
    esac
    shift
done

if [ -z "$arduino_dir" ]; then
    if command -v arduino-cli >/dev/null 2>&1; then
        arduino_dir="$(arduino-cli config get directories.data 2>/dev/null || true)"
    fi
    if [ -z "$arduino_dir" ] || [ ! -d "$arduino_dir" ]; then
        case "$(uname -s)" in
            Darwin) arduino_dir="$HOME/Library/Arduino15" ;;
            *)      arduino_dir="$HOME/.arduino15" ;;
        esac
    fi
fi

CORE="$arduino_dir/packages/esp32/hardware/esp32/$CORE_VERSION"
STOCK="$arduino_dir/packages/esp32/tools/esp32p4-libs/$CORE_VERSION"
STOCK_BACKUP="$STOCK.stock"
BOARD_LIBS="$CORE/tools/$LIBS_DIR_NAME"
BOARDS_TXT="$CORE/boards.txt"

echo "Arduino data folder: $arduino_dir"
[ -d "$CORE" ]  || die "arduino-esp32 $CORE_VERSION not found at $CORE. Install it with Boards Manager (esp32 by Espressif Systems, version $CORE_VERSION)."
[ -d "$STOCK" ] || die "ESP32-P4 libraries not found at $STOCK. Reinstall arduino-esp32 $CORE_VERSION."
[ -f "$BOARDS_TXT" ] || die "$BOARDS_TXT not found."

if command -v sha256sum >/dev/null 2>&1; then
    sha256() { sha256sum "$1" | cut -d' ' -f1; }
else
    sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }
fi

# Remove our marked block from boards.txt.
strip_block() {
    local tmp="$BOARDS_TXT.tmp"
    awk -v b="$BEGIN_MARK" -v e="$END_MARK" '$0==b{skip=1;next} $0==e{skip=0;next} !skip' "$BOARDS_TXT" > "$tmp"
    mv "$tmp" "$BOARDS_TXT"
}

has_block() { grep -qxF "$BEGIN_MARK" "$BOARDS_TXT"; }
block_has_board_files() {
    awk -v b="$BEGIN_MARK" -v e="$END_MARK" -v n="$BOARD.name=" \
        '$0==b{in_=1;next} $0==e{in_=0} in_ && index($0,n)==1{found=1} END{exit !found}' "$BOARDS_TXT"
}

uninstall() {
    if has_block; then
        if block_has_board_files; then
            rm -rf "$CORE/variants/sparkfun_esp32p4_iot_redboard_wifi" "$CORE/variants/sparkfun_esp32p4_iot_redboard_poe"
            info "removed RedBoard ESP32-P4 board files"
        fi
        strip_block
        info "removed boards.txt entries"
    fi
    if [ -d "$BOARD_LIBS" ]; then
        rm -rf "$BOARD_LIBS"
        info "removed $BOARD_LIBS"
    fi
    if [ -d "$STOCK_BACKUP" ]; then
        rm -rf "$STOCK"
        mv "$STOCK_BACKUP" "$STOCK"
        info "restored stock ESP32-P4 libraries"
    fi
}

if [ "$mode" = uninstall ]; then
    echo "Uninstalling"
    uninstall
    echo "Done. Restart the Arduino IDE."
    exit 0
fi

# Check the drop-in files.
[ -d "$DROP_IN" ] || die "$DROP_IN not found; run this script from its own folder in the download."
( cd "$DROP_IN" && while read -r sum file; do
    [ -f "$file" ] || die "drop-in/$file is missing"
    [ "$(sha256 "$file")" = "$sum" ] || die "drop-in/$file is corrupt (checksum mismatch)"
  done < SHA256SUMS )

# The source must be the stock libs (or their backup if --all-p4 was run before).
src="$STOCK"
[ -d "$STOCK_BACKUP" ] && src="$STOCK_BACKUP"
if ! diff -q <(grep '__' "$src/versions.txt" | sort) <(grep '__' "$DROP_IN/versions.txt" | sort) >/dev/null; then
    die "the ESP32-P4 libraries at $src don't match the versions this drop-in was built for. Reinstall arduino-esp32 $CORE_VERSION."
fi

echo "Installing (mode: $([ "$mode" = all ] && echo 'all ESP32-P4 boards' || echo 'this board only'))"
uninstall   # start from a clean state; restores $STOCK from the backup if there was one
src="$STOCK"

# Board files, if the core doesn't have this board yet.
block=""
if ! grep -q "^$BOARD\.name=" "$BOARDS_TXT"; then
    [ -f "$BOARD_FILES/boards.txt" ] && [ -d "$BOARD_FILES/variants/$BOARD" ] \
        || die "the core has no $BOARD board and no board files were found in $BOARD_FILES"
    cp -R "$BOARD_FILES/variants/sparkfun_esp32p4_iot_redboard_wifi" "$BOARD_FILES/variants/sparkfun_esp32p4_iot_redboard_poe" "$CORE/variants/"
    block="$(cat "$BOARD_FILES/boards.txt")"
    info "added RedBoard ESP32-P4 board files"
fi

if [ "$mode" = all ]; then
    mv "$STOCK" "$STOCK_BACKUP"
    cp -R "$STOCK_BACKUP" "$STOCK"
    target="$STOCK"
    info "backed up stock ESP32-P4 libraries to $STOCK_BACKUP"
else
    cp -R "$src" "$BOARD_LIBS"
    target="$BOARD_LIBS"
    block="$block${block:+$'\n'}$BOARD.tools.esp32-arduino-libs.path={runtime.platform.path}/tools/$LIBS_DIR_NAME"
fi

for f in "${DROP_IN_FILES[@]}"; do
    cp "$DROP_IN/$f" "$target/$f"
    [ "$(sha256 "$target/$f")" = "$(sha256 "$DROP_IN/$f")" ] || die "copy of $f failed"
done
info "installed drop-in into $target"

if [ -n "$block" ]; then
    [ -n "$(tail -c1 "$BOARDS_TXT")" ] && echo >> "$BOARDS_TXT"
    printf '%s\n%s\n%s\n' "$BEGIN_MARK" "$block" "$END_MARK" >> "$BOARDS_TXT"
    info "updated boards.txt"
fi

echo "Done. Restart the Arduino IDE and select 'SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE)'."
echo "Updating or reinstalling the esp32 core removes this; run the script again afterwards."
