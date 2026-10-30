# Arduino board support

Board variants for [arduino-esp32](https://github.com/espressif/arduino-esp32) (tested against 3.3.12), laid out to drop into that repo.

| Board | FQBN board id | Variant |
|---|---|---|
| SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE) | `sparkfun_esp32p4_iot_redboard_wifi` | `variants/sparkfun_esp32p4_iot_redboard_wifi` |
| SparkFun IoT RedBoard - ESP32-P4 (PoE) | `sparkfun_esp32p4_iot_redboard_poe` | `variants/sparkfun_esp32p4_iot_redboard_poe` |

| Path | |
|---|---|
| `boards.txt` | Entries to append to the core's `boards.txt` (the `menu.*` declarations come from the core file) |
| `variants/` | The two board variants |
| `patches/0001-ETH-esp32p4-rmii-clock-loopback.patch` | Ethernet library patch, required for Ethernet on the PoE board (see [Board notes](#board-notes)) |
| `drop-in/` | ESP32-P4 library files rebuilt for an ESP32-C5 co-processor, required for WiFi/BLE on the WiFi/BLE board (see [WiFi/BLE libraries](#wifible-libraries-esp32-c5-drop-in)); `drop-in/Ethernet/` holds the Ethernet library files with the patch applied |
| `install.sh`, `install.ps1` | Install the board files, `drop-in/` and the Ethernet patch into a 3.3.12 core: macOS/Linux and Windows |
| `build/` | Inputs for rebuilding `drop-in/`, and the matching C5 firmware (see [Rebuilding the drop-in](#rebuilding-the-drop-in)) |
| `tests/` | Hardware self test (see [`tests/README.md`](tests/README.md)) |

Pin mapping was taken from the KiCad netlists in `Hardware/`.

## Defaults

ESP32-P4 chip v3.x (400 MHz), 16 MB flash, PSRAM enabled, `Serial` on the USB-C Serial/JTAG port (J2); the USB-C OTG HS port (J11) is free for TinyUSB.

## WiFi/BLE libraries (ESP32-C5 drop-in)

The stock 3.3.12 ESP32-P4 libraries are built for an ESP32-C6 WiFi/BLE co-processor (`CONFIG_SLAVE_IDF_TARGET_ESP32C6`). The WiFi/BLE board uses an ESP32-C5, so with the stock libraries a sketch aborts as soon as WiFi or BLE starts:

```
E (5080) transport: Identified slave [esp32c5] != Expected [esp32c6]
assert failed: verify_host_config_for_slave transport_drv.c:694 (0!=0)
```

`drop-in/` fixes this by replacing 5 files in the stock libraries with ones built for a C5. It is a stopgap until arduino-esp32 supports the C5 directly.

The install below also patches the core's Ethernet library (`ETH.h`, `ETH.cpp`), which the PoE board needs: the stock library can't use the board's looped-back RMII clock, so `ETH.begin()` fails (see [PoE - RMII clock](#board-notes)). One install sets up both boards.

### Requirements

- **arduino-esp32 3.3.12** exactly, installed with Boards Manager (*esp32 by Espressif Systems*). The drop-in files and the patched Ethernet files must match the core version; don't use them with any other release.
- **ESP32-P4 chip revision v3.x** (the board's default *ChipVariant: v3.00 or newer*).

### Install

Close the Arduino IDE, then from this folder:

```sh
./install.sh                                              # macOS / Linux
```

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1    # Windows
```

Restart the IDE and select *SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE)* or *SparkFun IoT RedBoard - ESP32-P4 (PoE)*.

The options below choose where the C5 drop-in goes. The Ethernet patch is applied the same way in every install mode.

| `install.sh` | `install.ps1` | |
|---|---|---|
| *(none)* | *(none)* | **This board only (recommended).** Copies the stock `tools/esp32p4-libs/3.3.12` tree to `<core>/tools/esp32p4-c5-libs`, overwrites the drop-in files there, and adds a `boards.txt` line that points only this board at it. Every other board, including ESP32-P4 boards with a C6, keeps the stock libraries. |
| `--all-p4` | `-AllP4` | **All ESP32-P4 boards.** Renames the stock tree to `3.3.12.stock` and overwrites the drop-in files in a copy. WiFi/BLE then stops working on ESP32-P4 boards with a C6 (for example the ESP32-P4 Function EV Board) until you uninstall. |
| `--uninstall` | `-Uninstall` | Undoes either one: restores the stock libraries and the stock Ethernet library files, and removes the board files the script added |
| `--arduino-dir DIR` | `-ArduinoDir DIR` | Arduino data folder. Default: `arduino-cli`'s `directories.data`, otherwise `~/Library/Arduino15`, `~/.arduino15` or `%LOCALAPPDATA%\Arduino15` |

The scripts:

- stop before changing anything if core 3.3.12 or its ESP32-P4 libraries are missing, if a drop-in file doesn't match `drop-in/SHA256SUMS`, or if the installed libraries' component versions differ from `drop-in/versions.txt`.
- add `boards.txt` and both variants from this folder if the core doesn't have the `sparkfun_esp32p4_iot_redboard_wifi` board yet.
- apply the Ethernet patch, in either mode: they copy `drop-in/Ethernet/ETH.h` and `ETH.cpp` into `<core>/libraries/Ethernet/src` and keep the stock files in `<core>/tools/redboard-p4-eth-stock`, which uninstall restores. A file that is neither stock 3.3.12 nor already patched is left alone with a warning (the PoE board's Ethernet won't start). Boards that don't define `ETH_RMII_CLK_IN` behave exactly as before.
- put everything they add to `boards.txt` between `# BEGIN/END SparkFun RedBoard ESP32-P4 C5 drop-in` lines. A rerun replaces that block, and uninstall removes it, leaving `boards.txt` exactly as it was.
- can be mixed: either script can uninstall what the other one installed.

Board Manager replaces the core on an update or reinstall, which removes all of this, including the Ethernet patch. Run the script again afterwards.

### Manual install

`<core>` is `~/Library/Arduino15/packages/esp32/hardware/esp32/3.3.12` (macOS), `~/.arduino15/packages/esp32/hardware/esp32/3.3.12` (Linux) or `%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\3.3.12` (Windows).

1. Append `boards.txt` to `<core>/boards.txt` and copy `variants/*` into `<core>/variants/`, if the core doesn't have the board yet.
2. Copy the stock libraries, then the drop-in files over them:

   ```sh
   T=~/Library/Arduino15/packages/esp32/tools/esp32p4-libs/3.3.12   # Linux: ~/.arduino15/...
   cp -R "$T" "<core>/tools/esp32p4-c5-libs"
   cp -R drop-in/lib drop-in/qio_qspi drop-in/dio_qspi drop-in/sdkconfig "<core>/tools/esp32p4-c5-libs/"
   ```

3. Add this line to `<core>/boards.txt`, after the `sparkfun_esp32p4_iot_redboard_wifi` entries:

   ```
   sparkfun_esp32p4_iot_redboard_wifi.tools.esp32-arduino-libs.path={runtime.platform.path}/tools/esp32p4-c5-libs
   ```

4. For Ethernet on the PoE board, apply the patch to the core: `cd "<core>" && patch -p1 < /path/to/patches/0001-ETH-esp32p4-rmii-clock-loopback.patch`, or copy `drop-in/Ethernet/ETH.h` and `ETH.cpp` into `<core>/libraries/Ethernet/src`.

### Check the install

Compile any sketch for the WiFi/BLE board and look at the `sdkconfig` in the build folder:

```sh
arduino-cli compile -b esp32:esp32:sparkfun_esp32p4_iot_redboard_wifi --build-path build-out MySketch
grep CONFIG_IDF_SLAVE_TARGET build-out/sdkconfig
# CONFIG_IDF_SLAVE_TARGET="esp32c5"
```

On the board, a WiFi scan should list networks, including 5 GHz ones.

For the PoE board, check that the core's Ethernet library is patched:

```sh
grep -c ETH_RMII_CLK_IN "<core>/libraries/Ethernet/src/ETH.h" "<core>/libraries/Ethernet/src/ETH.cpp"
# both counts non-zero; 0 means the stock files are still in place
```

The full hardware tests for both boards are in [`tests/`](tests/README.md).

### What the drop-in changes

The C5 setting reaches only two components. Diffing the stock 3.3.12 libraries against a C5 build showed every other `.a` file differs only in build paths and timestamps.

| File in `tools/esp32p4-libs/3.3.12/` | Why |
|---|---|
| `lib/libespressif__esp_hosted.a` | Host-side co-processor driver; this is where the C6 check asserts |
| `lib/libespressif__esp_wifi_remote.a` | Remote WiFi API; its structs and defaults depend on the slave's capabilities |
| `qio_qspi/include/sdkconfig.h`, `dio_qspi/include/sdkconfig.h` | The esp_wifi_remote headers (`injected/esp_wifi.h`, `esp_wifi_types_*.h`, `esp_wifi_default_config.h`) read `CONFIG_SLAVE_*` when the sketch compiles, so they must agree with the `.a` files |
| `sdkconfig` | Reference only: the core copies it into the build folder. Replaced so it doesn't still say `esp32c6` |

The sdkconfig changes from stock are `SLAVE_IDF_TARGET` / `ESP_HOSTED_CP_TARGET` going from C6 to C5, plus `SLAVE_SOC_WIFI_SUPPORT_5G`, `SLAVE_SOC_WIFI_NAN_SUPPORT` and `SLAVE_SOC_WIFI_MAC_VERSION_NUM=3`. `flags/defines` doesn't change.

The files were built with every managed component pinned to the stock 3.3.12 versions (esp_hosted 2.12.13, esp_wifi_remote 1.6.4; `drop-in/versions.txt` matches the stock `versions.txt`). Against stock, that build has an identical `include/` tree, the same set of `.a` files, and the same `flags/` apart from link order in `flags/ld_libs` (the drop-in keeps the stock `ld_libs`).

### Tested

2026-10-30, stock 3.3.12 libraries with the drop-in files replaced, on a WiFi/BLE board (ESP32-P4 rev v3.2, C5 slave firmware 2.13.0): 14 of 14 boots passed (after upload, 6 `ESP.restart()`, 7 RTS resets). `c5_target` reported `slave=esp32c5 fw 2.13.0 host 2.12.13`, WiFi scans found 85-92 networks (45-51 on 5 GHz) and BLE scans found 61-77 advertisers.

Both install scripts were tested on macOS (`install.ps1` with PowerShell 7.6) against a copy of an Arduino data folder: install, reinstall, all-P4, switching modes, uninstall, a corrupt drop-in file, a missing core, and the Ethernet patch on a stock, modified, fully or partly pre-patched library. A PoE board flashed from a core installed with `install.sh` passed the Ethernet test. `install.ps1` has not yet been run on Windows.

### C5 firmware

The boards run ESP-Hosted slave firmware 2.13.0 on the C5, which works with the 2.12.13 host in the drop-in. The C5 needs 2.12.2 or newer for the P4 to detect it as an `esp32c5`. `build/esp32c5-v2.12.13.bin` is the slave image from the drop-in build, matching the host, for use with ESP-Hosted's OTA update (`ESP_HostedOTA` library). Updating the C5 is optional and untested.

## Rebuilding the drop-in

[esp32-arduino-lib-builder](https://github.com/espressif/esp32-arduino-lib-builder), pinned to the commits the stock 3.3.12 libraries were built from, with two changes from `build/`:

- `defconfig.esp32p4.diff`: `CONFIG_SLAVE_IDF_TARGET_ESP32C6=y` → `CONFIG_SLAVE_IDF_TARGET_ESP32C5=y`, the only configuration change.
- `idf_component.pinned.yml`: replaces `main/idf_component.yml` and pins every managed component to the stock 3.3.12 version. Without it the component manager resolves newer releases (esp_hosted 2.13.1, esp_wifi_remote 1.6.5, ...), whose `.a` files don't match the stock headers.

| | |
|---|---|
| lib-builder | `6671d0b` |
| ESP-IDF | v5.5.5 `b774170ff46` |
| arduino-esp32 component | `e7c1e8cd4` |
| TinyUSB | `88c7f0b81` (lib-builder's TinyUSB patch only applies at this commit) |

```sh
B=/path/to/this/Arduino/build
git clone https://github.com/espressif/esp32-arduino-lib-builder.git && cd esp32-arduino-lib-builder
git checkout 6671d0b
git apply "$B/defconfig.esp32p4.diff"
cp "$B/idf_component.pinned.yml" main/idf_component.yml
jq 'del(.targets[] | select(.chip_variant=="esp32p4_es"))' configs/builds.json > b.json && mv b.json configs/builds.json

export IDF_COMMIT=b774170ff46 IDF_BRANCH=release/v5.5
git clone https://github.com/hathach/tinyusb.git components/arduino_tinyusb/tinyusb
git -C components/arduino_tinyusb/tinyusb checkout 88c7f0b81
for p in patches/tinyusb/*.diff; do git -C components/arduino_tinyusb/tinyusb apply "$PWD/$p"; done
./tools/install-arduino.sh && git -C components/arduino checkout e7c1e8cd4
source ./tools/install-esp-idf.sh

./build.sh -s -t esp32p4      # output: out/tools/esp32-arduino-libs/esp32p4
```

Then copy the 5 files from `out/tools/esp32-arduino-libs/esp32p4` into `drop-in/`, copy `out/tools/esp32-arduino-libs/versions.txt` to `drop-in/versions.txt`, and regenerate the checksums:

```sh
cd drop-in && shasum -a 256 lib/*.a */include/sdkconfig.h sdkconfig Ethernet/ETH.h Ethernet/ETH.cpp > SHA256SUMS
```

Check that `drop-in/versions.txt` lists the same component versions as the stock `tools/esp32p4-libs/3.3.12/versions.txt`; the install scripts refuse to install otherwise.

Notes:

- On macOS, install `gnu-sed`, `gawk` and `coreutils` with Homebrew. Put the `gnu-sed` and `gawk` gnubin folders on `PATH`, but not coreutils': `copy-libs.sh` calls macOS `stat -f%z`, which GNU `stat` reads as filesystem info, and the output `lib/` folder comes out empty.
- On macOS, don't build under `/tmp`. It is a symlink to `/private/tmp`, and IDF's toolchain path check fails there with `@.../esp-idf/tools/cmake/cflags: linker input file not found`.

## Board notes

**WiFi/BLE - ESP32-C5 reset polarity.** GPIO20 drives the C5 EN pin through an NPN (Q4), so HIGH holds the C5 in reset. The prebuilt ESP-Hosted library treats its reset GPIO as an active-high enable (`CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH`, compile time only). The variant therefore points `BOARD_SDIO_ESP_HOSTED_RESET` at GPIO21 (not connected), and `variant.cpp` pulses GPIO20 in `initVariant()` to reset the C5 on every host boot. A library build with `CONFIG_ESP_HOSTED_RESET_GPIO_ACTIVE_LOW` and reset on GPIO20 would make this workaround unnecessary.

**WiFi/BLE - brownout at first WiFi start.** In an earlier test session the first two boots reset with `E BOD: Brownout detector was triggered` as WiFi started; the following 13 boots did not, and none of the 14 drop-in boots did. One rail (U2, AP2112K-3.3, 600 mA) feeds the P4, C5, flash and PSRAM. Probing 3.3V while the C5 starts its radio from a fresh state (C5 NVS erased, so it runs a full RF calibration) is worth doing before production.

`WiFi.macAddress()` returns `00:00:00:00:00:00` immediately after `WiFi.mode(WIFI_STA)` over ESP-Hosted and the real MAC once the station is running.

**PoE - RMII clock.** The DP83825I is strapped for RMII slave mode at PHY address 0. The P4 generates 50 MHz on GPIO23 (`EMAC_CLK_OUT`), looped back externally to GPIO32. The Ethernet library in arduino-esp32 3.3.12 (and on master) hard-codes `clock_config_out_in` to -1, so `ETH.begin()` fails without the patch (confirmed on hardware: `ETH.begin()` returns false). The patch adds `ETH_RMII_CLK_IN`, which the PoE variant sets to 32. The install scripts apply it. With it, the PoE test in [`tests/`](tests/README.md) links at 100 Mbps full duplex, gets a DHCP lease and pings sparkfun.com.

If the patch changes, regenerate the patched files and their checksums (the scripts check the core's files against the stock 3.3.12 hashes in `ETH_STOCK_SHA256` / `$EthFiles` before replacing them):

```sh
C=~/Library/Arduino15/packages/esp32/hardware/esp32/3.3.12; T=$(mktemp -d)
mkdir -p "$T/libraries/Ethernet/src" && cp "$C"/libraries/Ethernet/src/ETH.{h,cpp} "$T/libraries/Ethernet/src/"
(cd "$T" && patch -p1 < "$OLDPWD/patches/0001-ETH-esp32p4-rmii-clock-loopback.patch")
cp "$T"/libraries/Ethernet/src/ETH.{h,cpp} drop-in/Ethernet/
cd drop-in && shasum -a 256 lib/*.a */include/sdkconfig.h sdkconfig Ethernet/ETH.h Ethernet/ETH.cpp > SHA256SUMS
```

**PoE - BOOT button.** GPIO35 is both the BOOT strap and RMII TXD1. Pressing BOOT while Ethernet is running corrupts transmitted frames.

## Distributing to customers (plan, not done yet)

Until arduino-esp32 supports the C5, customers could download this folder as a zip and run the install script. Things to settle first:

- **Zip folder name.** GitHub's *Download ZIP* wraps everything in `<repo>-main/`; the instructions must say to run the script from the folder that contains it.
- **Locked to core 3.3.12.** State it prominently and tag the release `3.3.12`. A core update or reinstall removes the libraries, the Ethernet patch and the board files, and the drop-in won't install on a newer core (the version check refuses). Each new core release that still lacks C5 support needs a rebuild and a new tag.
- **Licensing.** These are Espressif binaries (ESP-IDF: Apache-2.0; arduino-esp32 and its libraries: LGPL-2.1). Redistribution is allowed; include the license files and a note that the build is unmodified apart from the two files in `build/`, and that `drop-in/Ethernet/` is the arduino-esp32 Ethernet library (LGPL-2.1) with `patches/0001-ETH-esp32p4-rmii-clock-loopback.patch` applied.
- **Windows.** Run `install.ps1` on a real Windows machine (Windows PowerShell 5.1, `%LOCALAPPDATA%`, long paths) before shipping.

The long-term fix is upstream: ask Espressif to have arduino-esp32 check the co-processor type when ESP-Hosted connects (the C5 reports it; firmware 2.12.2 or newer) instead of hard-coding the C6. The drop-in is the reference for that request: it shows the change touches only esp_hosted, esp_wifi_remote and `sdkconfig.h`.
