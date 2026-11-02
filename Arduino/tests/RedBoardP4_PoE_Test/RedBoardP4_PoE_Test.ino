// Board-file and hardware self test for the SparkFun IoT RedBoard - ESP32-P4 (PoE).
//
// Runs the same board tests as the WiFi/BLE sketch (defaults, pullups, I2C/fuel gauge, ADC, SD,
// RGB LED), then the Ethernet tests: brings up the DP83825I over RMII, gets an address by DHCP,
// pings the gateway and sparkfun.com, and opens a TCP connection to sparkfun.com.
// Needs the Ethernet patch (../install.sh applies it) and a cable to a network with DHCP and
// internet access. GPIO35 is both BOOT and RMII TXD1, so the BOOT pin checks only run before
// Ethernet starts. Results are printed as machine-readable lines:
//   RESULT <name> <PASS|FAIL|SKIP> <detail>
//   DONE pass=<n> fail=<n> skip=<n> boot=<n>
// Serial commands (single keys, any case; '?' prints the menu): see printMenu().
// The tests auto-start 10s after boot. Driven by run_tests.py --board poe, which sends 'S', 'N', 'R' and 'B'.

#include <ETH.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <NetworkClient.h>
#include "ping/ping_sock.h"
#include "lwip/inet.h"

#ifndef ARDUINO_SPARKFUN_ESP32P4_IOT_REDBOARD_POE
#error "Build with the sparkfun_esp32p4_iot_redboard_poe board"
#endif
#ifndef ETH_RMII_CLK_IN
#error "ETH_RMII_CLK_IN missing from the variant"
#endif

// ---- compile-time pin map checks against the KiCad netlist (H1 header symbol / U1 pads, DP83825I U8)
static_assert(TX == 37 && RX == 38, "UART0");
static_assert(SDA == 26 && SCL == 27, "I2C");
static_assert(SS == 7 && MOSI == 8 && MISO == 10 && SCK == 9, "SPI");
static_assert(D0 == 38 && D1 == 37 && D2 == 11 && D3 == 0 && D4 == 1 && D5 == 2 && D6 == 3, "D0-D6");
static_assert(D7 == 4 && D8 == 5 && D9 == 6 && D10 == 7 && D11 == 8 && D12 == 10 && D13 == 9, "D7-D13");
static_assert(A0 == 54 && A1 == 53 && A2 == 52 && A3 == 51 && A4 == 50 && A5 == 49, "A0-A5");
static_assert(PIN_RGB_LED == 12 && CAM_IO == 13, "misc");
static_assert(ETH_PHY_ADDR == 0 && ETH_PHY_MDC == 22 && ETH_PHY_MDIO == 21 && ETH_PHY_POWER == 20, "ETH MDIO/reset");
static_assert(ETH_RMII_TX_EN == 33 && ETH_RMII_TX0 == 34 && ETH_RMII_TX1 == 35, "RMII TX");
static_assert(ETH_RMII_RX0 == 29 && ETH_RMII_RX1_EN == 30 && ETH_RMII_CRS_DV == 28, "RMII RX");
static_assert(ETH_RMII_CLK == 23 && ETH_RMII_CLK_IN == 32, "RMII clock out/loopback");

static const char *PING_HOST = "sparkfun.com";
static const int PING_COUNT = 5;
static const uint8_t MAX17048_ADDR = 0x36;

RTC_NOINIT_ATTR uint32_t bootCount;
RTC_NOINIT_ATTR uint32_t bootMagic;

static int nPass, nFail, nSkip;
static bool ethStarted;  // once set, GPIO35 belongs to the EMAC (RMII TXD1)

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
  // GPIO35 is RMII TXD1 once Ethernet is running, so it is only read before ETH.begin().
  if (ethStarted) {
    result("boot_io35_idle", "SKIP", "GPIO35 is RMII TXD1 while Ethernet runs (restart to test)");
  } else {
    pinMode(35, INPUT);
    check("boot_io35_idle", digitalRead(35) == HIGH, "GPIO35=%d", digitalRead(35));
  }
  struct { const char *name; uint8_t pin; } ups[] = {
    {"strap_io36_pullup", 36}, {"cam_io13_pullup", CAM_IO},
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
  if (ethStarted) {
    result("boot_button", "SKIP", "GPIO35 is RMII TXD1 while Ethernet runs (restart, then press b before s)");
    return;
  }
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

static void testBoard() {
  testBoardConfig();
  testPullups();
  testI2C();
  testADC();
  testSD();
  testRGB();
}

// ---------------------------------------------------------------- ping (lwIP esp_ping)
struct PingStats {
  uint32_t sent, received, minMs, maxMs, sumMs;
  SemaphoreHandle_t done;
};

static void onPingSuccess(esp_ping_handle_t hdl, void *args) {
  PingStats *st = (PingStats *)args;
  uint32_t ms;
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &ms, sizeof(ms));
  st->minMs = min(st->minMs, ms);
  st->maxMs = max(st->maxMs, ms);
  st->sumMs += ms;
}

static void onPingTimeout(esp_ping_handle_t hdl, void *args) {}

static void onPingEnd(esp_ping_handle_t hdl, void *args) {
  PingStats *st = (PingStats *)args;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &st->sent, sizeof(st->sent));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &st->received, sizeof(st->received));
  xSemaphoreGive(st->done);
}

// Pings ip PING_COUNT times; PASS if every request gets a reply.
static void ping(const char *name, const char *label, IPAddress ip) {
  PingStats st = {0, 0, UINT32_MAX, 0, 0, xSemaphoreCreateBinary()};
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.count = PING_COUNT;
  cfg.interval_ms = 500;
  IP_ADDR4(&cfg.target_addr, ip[0], ip[1], ip[2], ip[3]);
  esp_ping_callbacks_t cbs = {&st, onPingSuccess, onPingTimeout, onPingEnd};
  esp_ping_handle_t hdl;
  esp_err_t err = esp_ping_new_session(&cfg, &cbs, &hdl);
  if (err != ESP_OK) {
    result(name, "FAIL", "esp_ping_new_session: %s", esp_err_to_name(err));
    vSemaphoreDelete(st.done);
    return;
  }
  esp_ping_start(hdl);
  bool ended = xSemaphoreTake(st.done, pdMS_TO_TICKS(PING_COUNT * 1500 + 2000)) == pdTRUE;
  esp_ping_stop(hdl);
  esp_ping_delete_session(hdl);
  vSemaphoreDelete(st.done);
  if (!ended) {
    result(name, "FAIL", "%s (%s): ping session did not end", label, ip.toString().c_str());
  } else if (st.received == 0) {
    result(name, "FAIL", "%s (%s): 0/%lu replies", label, ip.toString().c_str(), st.sent);
  } else {
    check(name, st.received == st.sent, "%s (%s): %lu/%lu replies, rtt min/avg/max %lu/%lu/%lu ms", label, ip.toString().c_str(), st.received,
          st.sent, st.minMs, st.sumMs / st.received, st.maxMs);
  }
}

// ---------------------------------------------------------------- Ethernet
static bool waitFor(bool (*cond)(), uint32_t timeoutMs, uint32_t &elapsed) {
  uint32_t t0 = millis();
  while (!cond() && millis() - t0 < timeoutMs) delay(50);
  elapsed = millis() - t0;
  return cond();
}

#define DONE() Serial.printf("DONE pass=%d fail=%d skip=%d boot=%lu\n", nPass, nFail, nSkip, bootCount)

// Starts Ethernet if needed and waits for link and a DHCP lease, reporting each step. True once online.
static bool bringUp() {
  if (!ethStarted) {
    // Reads the PHY over MDIO and starts the 50MHz RMII clock on GPIO23 (looped back to GPIO32).
    ethStarted = ETH.begin();
    if (!ethStarted) {
      result("eth_begin", "FAIL", "ETH.begin() failed: PHY not found or RMII clock config rejected");
      return false;
    }
  }
  result("eth_begin", "PASS", "PHY addr %lu, MAC %s", ETH.phyAddr(), ETH.macAddress().c_str());

  uint32_t ms;
  if (!waitFor([] { return ETH.linkUp(); }, 10000, ms)) {
    result("eth_link", "FAIL", "no link after %lums (cable plugged in?)", ms);
    return false;
  }
  result("eth_link", "PASS", "%u Mbps %s duplex, autoneg %s, %lums", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half", ETH.autoNegotiation() ? "on" : "off",
         ms);
  check("eth_100mbps_full", ETH.linkSpeed() == 100 && ETH.fullDuplex(), "%u Mbps %s duplex", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half");

  if (!waitFor([] { return ETH.hasIP(); }, 15000, ms)) {
    result("eth_dhcp", "FAIL", "no DHCP lease after %lums", ms);
    return false;
  }
  result("eth_dhcp", "PASS", "ip %s gw %s dns %s, %lums", ETH.localIP().toString().c_str(), ETH.gatewayIP().toString().c_str(),
         ETH.dnsIP().toString().c_str(), ms);
  return true;
}

// For single commands: bring Ethernet up only if it isn't already.
static bool online() {
  return (ethStarted && ETH.linkUp() && ETH.hasIP()) || bringUp();
}

static void testGateway() {
  ping("ping_gateway", "gateway", ETH.gatewayIP());
}

static void testSparkfun() {
  IPAddress ip;
  uint32_t t0 = millis();
  if (!Network.hostByName(PING_HOST, ip) || ip == IPAddress()) {
    result("dns_lookup", "FAIL", "%s did not resolve", PING_HOST);
    return;
  }
  result("dns_lookup", "PASS", "%s -> %s, %lums", PING_HOST, ip.toString().c_str(), millis() - t0);
  ping("ping_sparkfun", PING_HOST, ip);

  NetworkClient client;
  t0 = millis();
  bool ok = client.connect(ip, 443, 5000);
  check("tcp_sparkfun_443", ok, "%s:443 %s, %lums", ip.toString().c_str(), ok ? "connected" : "connect failed", millis() - t0);
  client.stop();
}

static void testEthernet() {
  if (bringUp()) {
    testGateway();
    testSparkfun();
  }
}

// Board tests first: GPIO35 can only be checked before Ethernet takes it.
static void runTests(bool network) {
  nPass = nFail = nSkip = 0;
  Serial.printf("BEGIN board=%s boot=%lu reset_reason=%d\n", ARDUINO_BOARD, bootCount, esp_reset_reason());
  testBoard();
  if (network) {
    testEthernet();
  } else {
    result("ethernet", "SKIP", "network tests disabled ('N')");
  }
  DONE();
}

// Runs one group of tests on its own, with BEGIN/DONE around it. Network tests bring Ethernet up first.
static void runOne(const char *what, void (*test)(), bool needsNetwork = false) {
  nPass = nFail = nSkip = 0;
  Serial.printf("BEGIN %s board=%s boot=%lu\n", what, ARDUINO_BOARD, bootCount);
  if (!needsNetwork || online()) test();
  DONE();
}

static void printStatus() {
  if (!ethStarted) {
    Serial.println("Ethernet not started (press s)");
    return;
  }
  Serial.printf("MAC %s, link %s", ETH.macAddress().c_str(), ETH.linkUp() ? "up" : "down");
  if (ETH.linkUp()) Serial.printf(" %u Mbps %s duplex", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half");
  Serial.println();
  if (ETH.hasIP()) {
    Serial.printf("IP %s, mask %s, gateway %s, DNS %s\n", ETH.localIP().toString().c_str(), ETH.subnetMask().toString().c_str(),
                  ETH.gatewayIP().toString().c_str(), ETH.dnsIP().toString().c_str());
  } else {
    Serial.println("No IP address");
  }
}

static void printMenu() {
  Serial.println();
  Serial.println("RedBoard ESP32-P4 (PoE) test - press a key:");
  Serial.println("  s  run all tests");
  Serial.println("  n  run all tests except Ethernet");
  Serial.println("  e  Ethernet only (link, DHCP, ping gateway and sparkfun.com)");
  Serial.println("  p  ping sparkfun.com (DNS, ping, TCP 443)");
  Serial.println("  g  ping the gateway");
  Serial.println("  a  show link and IP address status");
  Serial.println("  i  I2C scan and fuel gauge");
  Serial.println("  d  SD card");
  Serial.println("  c  RGB LED colour cycle");
  Serial.println("  b  BOOT button (press it when prompted; only before Ethernet starts)");
  Serial.println("  r  restart");
  Serial.println("  ?  this menu");
}

// Reads one command key; ignores line endings so the IDE Serial Monitor can send "s\n".
static int readCommand() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c != '\r' && c != '\n' && c != ' ') return tolower(c);
  }
  return -1;
}

void setup() {
  if (bootMagic != 0x52424434) {
    bootMagic = 0x52424434;
    bootCount = 0;
  }
  bootCount++;
  Serial.begin(115200);

  printMenu();
  Serial.println("Running all tests in 10 s (press s to start now, n to skip Ethernet)");
  bool network = true;
  uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    int c = readCommand();
    if (c == 's' || c == 'n') {
      network = c == 's';
      break;
    }
    delay(10);
  }
  runTests(network);
  Serial.println("Press ? for the menu");
}

void loop() {
  int c = readCommand();
  if (c < 0) return;
  switch (c) {
    case 's': runTests(true); break;
    case 'n': runTests(false); break;
    case 'e': runOne("ethernet", testEthernet); break;
    case 'p': runOne("sparkfun", testSparkfun, true); break;
    case 'g': runOne("gateway", testGateway, true); break;
    case 'a': printStatus(); break;
    case 'i': runOne("i2c", testI2C); break;
    case 'd': runOne("sd", testSD); break;
    case 'c': runOne("rgb", testRGB); break;
    case 'b': testBootButton(); break;
    case 'r':
      Serial.println("RESTARTING");
      Serial.flush();
      delay(100);
      ESP.restart();
      break;
    case '?':
    case 'h': printMenu(); return;
    default: Serial.printf("Unknown command '%c' - press ? for the menu\n", c); return;
  }
  Serial.println("Press ? for the menu");
}
