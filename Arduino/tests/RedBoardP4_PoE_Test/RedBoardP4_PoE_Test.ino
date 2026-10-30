// Ethernet self test for the SparkFun IoT RedBoard - ESP32-P4 (PoE).
//
// Needs the Ethernet patch in ../../patches and a cable to a network with DHCP and internet access.
// Brings up the DP83825I over RMII, gets an address by DHCP, pings the gateway and sparkfun.com,
// and opens a TCP connection to sparkfun.com. Results are printed as machine-readable lines:
//   RESULT <name> <PASS|FAIL|SKIP> <detail>
//   DONE pass=<n> fail=<n> skip=<n> boot=<n>
// Serial commands: 'S' start tests (auto-starts after 10s), 'R' ESP.restart().
// Driven by run_tests.py --board poe.

#include <ETH.h>
#include <NetworkClient.h>
#include "ping/ping_sock.h"
#include "lwip/inet.h"

#ifndef ARDUINO_SPARKFUN_ESP32P4_IOT_REDBOARD_POE
#error "Build with the sparkfun_esp32p4_iot_redboard_poe board"
#endif
#ifndef ETH_RMII_CLK_IN
#error "ETH_RMII_CLK_IN missing from the variant"
#endif

// ---- compile-time pin map checks against the KiCad netlist (U1 pads, DP83825I U8)
static_assert(ETH_PHY_ADDR == 0 && ETH_PHY_MDC == 22 && ETH_PHY_MDIO == 21 && ETH_PHY_POWER == 20, "ETH MDIO/reset");
static_assert(ETH_RMII_TX_EN == 33 && ETH_RMII_TX0 == 34 && ETH_RMII_TX1 == 35, "RMII TX");
static_assert(ETH_RMII_RX0 == 29 && ETH_RMII_RX1_EN == 30 && ETH_RMII_CRS_DV == 28, "RMII RX");
static_assert(ETH_RMII_CLK == 23 && ETH_RMII_CLK_IN == 32, "RMII clock out/loopback");

static const char *PING_HOST = "sparkfun.com";
static const int PING_COUNT = 5;

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

static bool ethStarted;

static void runTests() {
  nPass = nFail = nSkip = 0;
  Serial.printf("BEGIN board=%s boot=%lu reset_reason=%d\n", ARDUINO_BOARD, bootCount, esp_reset_reason());

  if (!ethStarted) {
    // Reads the PHY over MDIO and starts the 50MHz RMII clock on GPIO23 (looped back to GPIO32).
    ethStarted = ETH.begin();
    if (!ethStarted) {
      result("eth_begin", "FAIL", "ETH.begin() failed: PHY not found or RMII clock config rejected");
      Serial.printf("DONE pass=%d fail=%d skip=%d boot=%lu\n", nPass, nFail, nSkip, bootCount);
      return;
    }
  }
  result("eth_begin", "PASS", "PHY addr %lu, MAC %s", ETH.phyAddr(), ETH.macAddress().c_str());

  uint32_t ms;
  if (!waitFor([] { return ETH.linkUp(); }, 10000, ms)) {
    result("eth_link", "FAIL", "no link after %lums (cable plugged in?)", ms);
    Serial.printf("DONE pass=%d fail=%d skip=%d boot=%lu\n", nPass, nFail, nSkip, bootCount);
    return;
  }
  result("eth_link", "PASS", "%u Mbps %s duplex, autoneg %s, %lums", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half", ETH.autoNegotiation() ? "on" : "off",
         ms);
  check("eth_100mbps_full", ETH.linkSpeed() == 100 && ETH.fullDuplex(), "%u Mbps %s duplex", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half");

  if (!waitFor([] { return ETH.hasIP(); }, 15000, ms)) {
    result("eth_dhcp", "FAIL", "no DHCP lease after %lums", ms);
    Serial.printf("DONE pass=%d fail=%d skip=%d boot=%lu\n", nPass, nFail, nSkip, bootCount);
    return;
  }
  result("eth_dhcp", "PASS", "ip %s gw %s dns %s, %lums", ETH.localIP().toString().c_str(), ETH.gatewayIP().toString().c_str(),
         ETH.dnsIP().toString().c_str(), ms);

  ping("ping_gateway", "gateway", ETH.gatewayIP());

  IPAddress ip;
  uint32_t t0 = millis();
  if (!Network.hostByName(PING_HOST, ip) || ip == IPAddress()) {
    result("dns_lookup", "FAIL", "%s did not resolve", PING_HOST);
  } else {
    result("dns_lookup", "PASS", "%s -> %s, %lums", PING_HOST, ip.toString().c_str(), millis() - t0);
    ping("ping_sparkfun", PING_HOST, ip);

    NetworkClient client;
    t0 = millis();
    bool ok = client.connect(ip, 443, 5000);
    check("tcp_sparkfun_443", ok, "%s:443 %s, %lums", ip.toString().c_str(), ok ? "connected" : "connect failed", millis() - t0);
    client.stop();
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

  uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    int c = Serial.available() ? Serial.read() : -1;
    if (c == 'S' || c == 'N') break;
    delay(10);
  }
  runTests();
}

void loop() {
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case 'S':
    case 'N': runTests(); break;
    case 'R':
      Serial.println("RESTARTING");
      Serial.flush();
      delay(100);
      ESP.restart();
      break;
  }
}
