# Board-file hardware tests

Self tests for both boards. `run_tests.py` flashes one and runs it across several boots (after upload, `ESP.restart()`, RTS hard reset).

| `--board` | Sketch | Needs |
|---|---|---|
| `wifi` (default) | `RedBoardP4_WiFi_Test` | No external wiring. WiFi/BLE tests need the ESP32-C5 drop-in (`../install.sh` or `../install.ps1`, see ../README.md); with the stock 3.3.12 libs ESP-Hosted asserts. |
| `poe` | `RedBoardP4_PoE_Test` | An Ethernet cable to a network with DHCP and internet access, and the Ethernet patch applied to the core (`../install.sh` / `../install.ps1` do this) |

```sh
python3 -m pip install pyserial
python3 run_tests.py --port /dev/cu.usbmodem1301               # WiFi/BLE board, full suite
python3 run_tests.py --port /dev/cu.usbmodem1301 --no-network  # skip WiFi/BLE (see ../README.md)
python3 run_tests.py --port /dev/cu.usbmodem1301 --no-upload --boot-button
python3 run_tests.py --port /dev/cu.usbmodem1301 --board poe   # PoE board
```

Pass `--fqbn` if the board isn't installed as `esp32:esp32:sparkfun_esp32p4_iot_redboard_wifi` / `_poe`. `--soft`/`--hard` set the number of restarts of each kind.

The sketches also work on their own from the Arduino IDE: upload, open the Serial Monitor at 115200 baud, and type a key and Enter (any case; line endings are ignored). They print a menu at boot and run all tests after 10 s; `?` shows the menu again.

| Key | WiFi/BLE sketch | PoE sketch |
|---|---|---|
| `s` | Run all tests | Run all tests |
| `n` | All tests except WiFi/BLE | All tests except Ethernet |
| `w` / `l` | WiFi only / BLE only | |
| `e` | | Ethernet only (link, DHCP, ping gateway and sparkfun.com) |
| `p` / `g` | | Ping sparkfun.com (DNS, ping, TCP 443) / ping the gateway |
| `a` | | Show link and IP address status |
| `i` | I2C scan and fuel gauge | I2C scan and fuel gauge |
| `d` / `c` | SD card / RGB LED | SD card / RGB LED |
| `b` | BOOT button test | BOOT button test, only before Ethernet starts (restart, then `b` before `s`) |
| `r` | Restart | Restart |
| `?` | Menu | Menu |

## WiFi/BLE board

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

## PoE board

| Test | Checks |
|---|---|
| Board tests | The same board tests as the WiFi/BLE board (defaults, pullups, VO4, I2C/fuel gauge, ADC, SD, RGB LED, BOOT button), run before Ethernet, except `c5_reset_released` (no C5; GPIO20 is the PHY reset). GPIO35 is also RMII TXD1, so `boot_io35_idle` and `boot_button` report SKIP once Ethernet is running. |
| `eth_begin` | `ETH.begin()` with the variant defaults: DP83825I found over MDIO at address 0, RMII clock out on GPIO23 and looped back to GPIO32 (`ETH_RMII_CLK_IN`, needs the patch) |
| `eth_link`, `eth_100mbps_full` | Link up within 10 s, autonegotiated to 100 Mbps full duplex |
| `eth_dhcp` | DHCP lease within 15 s |
| `ping_gateway` | 5 pings to the gateway, all answered |
| `dns_lookup` | `sparkfun.com` resolves |
| `ping_sparkfun` | 5 pings to `sparkfun.com`, all answered. A network that blocks ICMP fails this even with a working link; check `tcp_sparkfun_443` then |
| `tcp_sparkfun_443` | TCP connection to `sparkfun.com:443` |
| Pin map | `static_assert`s against the KiCad netlist, including the RMII/MDIO pins (compile time) |

Tested 2026-10-30 on a PoE board (ESP32-P4 rev v3.2) on a wired office network, full suite: 5 of 5 boots passed (after upload, 2 `ESP.restart()`, 2 RTS resets). All board tests passed (`sd_mount` SKIP without a card), and every boot linked at 100 Mbps full duplex, got a DHCP lease in 1-1.5 s, and got 5/5 ping replies from the gateway (under 1 ms) and from sparkfun.com (2-8 ms). The menu was also checked from the Arduino IDE Serial Monitor. With the same sketch built against an unpatched 3.3.12 core, `eth_begin` fails: `ETH.begin()` returns false and Ethernet never starts.

On macOS, opening the USB Serial/JTAG port resets the P4 once, so the boot counter steps by 2 per run.
