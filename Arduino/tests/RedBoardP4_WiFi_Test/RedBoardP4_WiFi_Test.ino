// Board-file and hardware self test for the SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE).
//
// Needs no external wiring. Results are printed as machine-readable lines:
//   RESULT <name> <PASS|FAIL|SKIP|MANUAL> <detail>
//   DONE pass=<n> fail=<n> skip=<n> boot=<n>
// Serial commands: 'S' start tests (auto-starts after 10s), 'N' start without WiFi/BLE,
// 'R' ESP.restart(), 'B' BOOT button test.
// Driven by run_tests.py, which also checks the C5 comes back across hard and soft resets.

#include <WiFi.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include "esp32-hal-hosted.h"
#include "esp_wifi.h"

#ifndef ARDUINO_SPARKFUN_ESP32P4_IOT_REDBOARD_WIFI
#error "Build with the sparkfun_esp32p4_iot_redboard_wifi board"
#endif

// ---- compile-time pin map checks against the KiCad netlist (H1 header symbol / U1 pads)
static_assert(TX == 37 && RX == 38, "UART0");
static_assert(SDA == 26 && SCL == 27, "I2C");
static_assert(SS == 7 && MOSI == 8 && MISO == 10 && SCK == 9, "SPI");
static_assert(D0 == 38 && D1 == 37 && D2 == 11 && D3 == 0 && D4 == 1 && D5 == 2 && D6 == 3, "D0-D6");
static_assert(D7 == 4 && D8 == 5 && D9 == 6 && D10 == 7 && D11 == 8 && D12 == 10 && D13 == 9, "D7-D13");
static_assert(A0 == 54 && A1 == 53 && A2 == 52 && A3 == 51 && A4 == 50 && A5 == 49, "A0-A5");
static_assert(PIN_RGB_LED == 12 && CAM_IO == 13 && C5_RESET_PIN == 20, "misc");
static_assert(BOARD_SDIO_ESP_HOSTED_CLK == 16 && BOARD_SDIO_ESP_HOSTED_CMD == 17, "SDIO clk/cmd");
static_assert(BOARD_SDIO_ESP_HOSTED_D0 == 15 && BOARD_SDIO_ESP_HOSTED_D1 == 14, "SDIO d0/d1");
static_assert(BOARD_SDIO_ESP_HOSTED_D2 == 19 && BOARD_SDIO_ESP_HOSTED_D3 == 18, "SDIO d2/d3");

static const uint8_t MAX17048_ADDR = 0x36;

RTC_NOINIT_ATTR uint32_t bootCount;
RTC_NOINIT_ATTR uint32_t bootMagic;

static int nPass, nFail, nSkip;

static void result(const char *name, const char *status, const char *fmt = "", ...) {
  char detail[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(detail, sizeof(detail), fmt, ap);
  va_end(ap);
  Serial.printf("RESULT %s %s %s\n", name, status, detail);
  if (!strcmp(status, "PASS")) nPass++;
  else if (!strcmp(status, "FAIL")) nFail++;
  else if (!strcmp(status, "SKIP")) nSkip++;
}

static void check(const char *name, bool ok, const char *fmt = "", ...) {
  char detail[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(detail, sizeof(detail), fmt, ap);
  va_end(ap);
  result(name, ok ? "PASS" : "FAIL", "%s", detail);
}

// ---------------------------------------------------------------- board file
static void testBoardConfig() {
  uint16_t rev = ESP.getChipRevision();
  check("chip_revision", rev >= 300, "v%u.%u (board default is ChipVariant v3.00 or newer)", rev / 100, rev % 100);
  check("cpu_freq", getCpuFrequencyMhz() == 400, "%lu MHz", getCpuFrequencyMhz());
  check("flash_size", ESP.getFlashChipSize() == 16 * 1024 * 1024, "%lu bytes", ESP.getFlashChipSize());
  check("psram_found", psramFound(), "%lu bytes", ESP.getPsramSize());

#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  check("serial_hwcdc", true, "Serial is USB Serial/JTAG (J2)");
#else
  check("serial_hwcdc", false, "USBMode=%d CDCOnBoot=%d", ARDUINO_USB_MODE, ARDUINO_USB_CDC_ON_BOOT);
#endif

  // PSRAM read/write
  const size_t len = 4 * 1024 * 1024;
  uint32_t *buf = (uint32_t *)ps_malloc(len);
  if (!buf) {
    result("psram_rw", "FAIL", "ps_malloc(%u) failed", len);
    return;
  }
  for (size_t i = 0; i < len / 4; i++) buf[i] = i * 2654435761u;
  size_t bad = 0;
  for (size_t i = 0; i < len / 4; i++) bad += buf[i] != i * 2654435761u;
  free(buf);
  check("psram_rw", bad == 0, "4MB pattern, %u mismatches", bad);
}

// ---------------------------------------------------------------- GPIO / pullups
static void testPullups() {
  // External 10k pullups; pinMode(INPUT) disables the internal pulls.
  struct { const char *name; uint8_t pin; } ups[] = {
    {"boot_io35_idle", 35}, {"strap_io36_pullup", 36}, {"cam_io13_pullup", CAM_IO},
  };
  for (auto &u : ups) {
    pinMode(u.pin, INPUT);
    check(u.name, digitalRead(u.pin) == HIGH, "GPIO%u=%d", u.pin, digitalRead(u.pin));
  }

  // The SD bus pullups (R1-R6) go to VDD_SD, which is the on-chip LDO VO4. These read HIGH only if
  // periman powered VO4 when the pins were claimed (BOARD_PERIMAN_IO_LDO_*).
  char detail[64] = "";
  bool ok = true;
  for (uint8_t pin = 39; pin <= 44; pin++) {
    pinMode(pin, INPUT);
    delay(2);
    int v = digitalRead(pin);
    ok &= v == HIGH;
    snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail), "%u=%d ", pin, v);
  }
  check("ldo_vo4_sd_pullups", ok, "%s", detail);

  pinMode(SD_CARD_DETECT, INPUT);
  pinMode(BAT_STATUS, INPUT);
  pinMode(FUEL_GAUGE_ALERT, INPUT);
  result("vo4_inputs", "PASS", "SD_DET(47)=%d BAT_ST(48)=%d ALRT(46)=%d", digitalRead(SD_CARD_DETECT), digitalRead(BAT_STATUS),
         digitalRead(FUEL_GAUGE_ALERT));

  // C5 must be out of reset: GPIO20 LOW (Q4 off)
  check("c5_reset_released", digitalRead(C5_RESET_PIN) == LOW, "GPIO20=%d", digitalRead(C5_RESET_PIN));
}

// ---------------------------------------------------------------- I2C / fuel gauge
static bool readReg16(uint8_t addr, uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return false;
  val = (Wire.read() << 8) | Wire.read();
  return true;
}

static void testI2C() {
  check("wire_begin", Wire.begin(), "SDA=%u SCL=%u", SDA, SCL);
  char found[96] = "";
  int n = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      n++;
      snprintf(found + strlen(found), sizeof(found) - strlen(found), "0x%02X ", a);
    }
  }
  result("i2c_scan", n ? "PASS" : "FAIL", "%d device(s): %s", n, found);

  uint16_t version, vcell, soc;
  if (!readReg16(MAX17048_ADDR, 0x08, version)) {
    result("max17048", "FAIL", "no response at 0x36");
    return;
  }
  check("max17048_version", (version & 0xFFF0) == 0x0010, "VERSION=0x%04X", version);
  if (readReg16(MAX17048_ADDR, 0x02, vcell) && readReg16(MAX17048_ADDR, 0x04, soc)) {
    result("max17048_reading", "PASS", "VCELL=%.3fV SOC=%.1f%%", vcell * 78.125e-6, soc / 256.0);
  }
}

// ---------------------------------------------------------------- ADC
static void testADC() {
  const uint8_t pins[] = {A0, A1, A2, A3, A4, A5};
  char detail[96] = "";
  bool ok = true;
  for (int i = 0; i < 6; i++) {
    uint32_t mv = analogReadMilliVolts(pins[i]);
    ok &= mv <= 3400;
    snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail), "A%d=%lumV ", i, mv);
  }
  check("adc_a0_a5", ok, "%s(floating unless wired)", detail);
}

// ---------------------------------------------------------------- SD card
static void testSD() {
  if (!SD_MMC.begin()) {
    result("sd_mount", "SKIP", "no card / mount failed (SD_DET=%d)", digitalRead(SD_CARD_DETECT));
    return;
  }
  result("sd_mount", "PASS", "type=%d size=%lluMB SD_DET=%d", SD_MMC.cardType(), SD_MMC.cardSize() >> 20, digitalRead(SD_CARD_DETECT));
  const char *path = "/redboard_p4_test.txt";
  File f = SD_MMC.open(path, FILE_WRITE);
  bool ok = f && f.printf("boot %lu\n", bootCount) > 0;
  if (f) f.close();
  f = SD_MMC.open(path);
  String s = f ? f.readString() : String();
  if (f) f.close();
  ok &= s.startsWith("boot ");
  SD_MMC.remove(path);
  check("sd_rw", ok, "read back '%s'", s.c_str());
  SD_MMC.end();
}

// ---------------------------------------------------------------- WiFi (ESP-Hosted, ESP32-C5)
static void testWiFi() {
  uint32_t t0 = millis();
  bool ok = WiFi.mode(WIFI_STA);
  check("wifi_hosted_init", ok, "%lums", millis() - t0);
  if (!ok) return;

  const char *target = hostedGetSlaveTargetName();
  uint32_t hmaj, hmin, hpat, smaj, smin, spat;
  hostedGetHostVersion(&hmaj, &hmin, &hpat);
  hostedGetSlaveVersion(&smaj, &smin, &spat);
  check("c5_target", target && !strcmp(target, "esp32c5"), "slave=%s fw %lu.%lu.%lu host %lu.%lu.%lu", target ? target : "null", smaj, smin, spat,
        hmaj, hmin, hpat);

  String macBefore = WiFi.macAddress();
  uint8_t mac[6] = {0};
  esp_err_t macErr = esp_wifi_get_mac(WIFI_IF_STA, mac);

  t0 = millis();
  int n = WiFi.scanNetworks();
  int best = -127, n5 = 0;
  for (int i = 0; i < n; i++) {
    best = max(best, (int)WiFi.RSSI(i));
    n5 += WiFi.channel(i) > 14;
  }
  check("wifi_scan", n > 0, "%d networks (%d on 5GHz), best RSSI %d dBm, %lums", n, n5, best, millis() - t0);
  result("wifi_5ghz_seen", n5 ? "PASS" : "SKIP", "%d networks on 5GHz", n5);
  WiFi.scanDelete();

  String macAfter = WiFi.macAddress();
  check("wifi_mac", macAfter != "00:00:00:00:00:00", "WiFi.macAddress() %s before scan, %s after; esp_wifi_get_mac %s %02x:%02x:%02x:%02x:%02x:%02x",
        macBefore.c_str(), macAfter.c_str(), esp_err_to_name(macErr), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---------------------------------------------------------------- BLE (NimBLE over ESP-Hosted)
static void testBLE() {
  if (!BLEDevice::init("RedBoardP4-Test")) {
    result("ble_init", "FAIL", "");
    return;
  }
  result("ble_init", "PASS", "addr %s", BLEDevice::getAddress().toString().c_str());
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults *r = scan->start(4, false);
  int n = r ? r->getCount() : 0;
  result("ble_scan", n > 0 ? "PASS" : "FAIL", "%d advertisers in 4s", n);
  scan->clearResults();
}

// ---------------------------------------------------------------- manual checks
static void testRGB() {
  rgbLedWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);
  delay(400);
  rgbLedWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);
  delay(400);
  rgbLedWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);
  delay(400);
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
  result("rgb_led", "MANUAL", "LED_BUILTIN on GPIO12 should have shown red, green, blue");
}

static void testBootButton() {
  Serial.println("PROMPT Press and release the BOOT button within 15 seconds");
  pinMode(35, INPUT);
  uint32_t t0 = millis();
  bool pressed = false, released = false;
  while (millis() - t0 < 15000 && !released) {
    if (!pressed && digitalRead(35) == LOW) pressed = true;
    if (pressed && digitalRead(35) == HIGH) released = true;
    delay(5);
  }
  check("boot_button", released, pressed ? "pressed%s" : "no press seen", released ? " and released" : "");
}

static void runTests(bool network) {
  nPass = nFail = nSkip = 0;
  Serial.printf("BEGIN board=%s boot=%lu reset_reason=%d\n", ARDUINO_BOARD, bootCount, esp_reset_reason());
  testBoardConfig();
  testPullups();
  testI2C();
  testADC();
  testSD();
  testRGB();
  if (network) {
    testWiFi();
    testBLE();
  } else {
    result("wifi_ble", "SKIP", "network tests disabled ('N')");
  }
  Serial.printf("DONE pass=%d fail=%d skip=%d boot=%lu\n", nPass, nFail, nSkip, bootCount);
}

void setup() {
  if (bootMagic != 0x52424434) {
    bootMagic = 0x52424434;
    bootCount = 0;
  }
  bootCount++;
  Serial.begin(115200);

  bool network = true;
  uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    int c = Serial.available() ? Serial.read() : -1;
    if (c == 'S' || c == 'N') {
      network = c == 'S';
      break;
    }
    delay(10);
  }
  runTests(network);
}

void loop() {
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case 'S': runTests(true); break;
    case 'N': runTests(false); break;
    case 'B': testBootButton(); break;
    case 'R':
      Serial.println("RESTARTING");
      Serial.flush();
      delay(100);
      ESP.restart();
      break;
  }
}
