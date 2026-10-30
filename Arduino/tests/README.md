# Board-file hardware tests

`RedBoardP4_WiFi_Test` is a self test for the WiFi/BLE board that needs no external wiring. `run_tests.py` flashes it and runs it across several boots (after upload, `ESP.restart()`, RTS hard reset).

```sh
python3 -m pip install pyserial
python3 run_tests.py --port /dev/cu.usbmodem1301               # full suite
python3 run_tests.py --port /dev/cu.usbmodem1301 --no-network  # skip WiFi/BLE (see ../README.md)
python3 run_tests.py --port /dev/cu.usbmodem1301 --no-upload --boot-button
```

Pass `--fqbn` if the board isn't installed as `esp32:esp32:sparkfun_esp32p4_iot_redboard_wifi`. The WiFi/BLE tests need the ESP32-C5 drop-in (`../install.sh` or `../install.ps1`, see ../README.md); with the stock 3.3.12 libs ESP-Hosted asserts. `--soft`/`--hard` set the number of restarts of each kind.

| Test | Checks |
|---|---|
| `chip_revision`, `cpu_freq`, `flash_size`, `psram_*`, `serial_hwcdc` | boards.txt defaults: v3.x silicon at 400 MHz, 16 MB flash, PSRAM, `Serial` on USB Serial/JTAG |
| `boot_io35_idle`, `strap_io36_pullup`, `cam_io13_pullup` | External pullups on the mapped pins |
| `ldo_vo4_sd_pullups` | SD bus pullups on GPIO39-44 read high, so periman turned on LDO VO4 (`BOARD_PERIMAN_IO_LDO_*`) |
| `c5_reset_released` | GPIO20 low after `initVariant()` (C5 running) |
| `i2c_scan`, `max17048_*` | `SDA`/`SCL` mapping; MAX17048 fuel gauge at 0x36 |
| `adc_a0_a5` | `A0`-`A5` read through the ADC |
| `sd_mount`, `sd_rw` | `SD_MMC` slot 0 and power pin; SKIP without a card |
| `wifi_*`, `c5_target`, `ble_*` | ESP-Hosted over SDIO to the C5, WiFi scan, BLE scan |
| `rgb_led`, `boot_button` | Manual: watch the LED / press BOOT when prompted |
| Pin map | `static_assert`s against the KiCad netlist (compile time) |

On macOS, opening the USB Serial/JTAG port resets the P4 once, so the boot counter steps by 2 per run.
