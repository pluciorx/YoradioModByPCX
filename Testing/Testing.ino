#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "www.tap2get.pl_2ghz";
const char* password = "PhieV5ai";

void tryConnectWithBSSID(const uint8_t* bssid, int channel, unsigned long timeoutMs = 20000) {
    WiFi.disconnect(true, true);
    delay(300);
    WiFi.mode(WIFI_STA);
    // begin with explicit channel + bssid to target that AP instance
    Serial.printf("WiFi.begin(ssid, pwd, channel=%d, bssid=%s)\n", channel, String(WiFi.BSSIDstr()).c_str());
    WiFi.begin(ssid, password, channel, bssid);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected to %s, IP: %s\n", ssid, WiFi.localIP().toString().c_str());
    }
    else {
        Serial.printf("Connect attempt failed, status=%d\n", WiFi.status());
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);

    int n = WiFi.scanNetworks();
    Serial.printf("Scan found %d networks\n", n);
    struct ApEntry { String ss; int rssi; String bss; int ch; int idx; };
    std::vector<ApEntry> matches;

    for (int i = 0; i < n; ++i) {
        String s = WiFi.SSID(i);
        String b = WiFi.BSSIDstr(i);
        int r = WiFi.RSSI(i);
        int ch = WiFi.channel(i);
        Serial.printf("%2d: '%s' (RSSI %d) BSSID=%s ch=%d\n", i + 1, s.c_str(), r, b.c_str(), ch);
        if (s == String(ssid)) matches.push_back({ s, r, b, ch, i });
    }

    if (matches.empty()) {
        Serial.println("No matching SSID found by scan.");
        return;
    }

    // sort by RSSI desc (strongest first)
    std::sort(matches.begin(), matches.end(), [](const ApEntry& a, const ApEntry& b) { return a.rssi > b.rssi; });

    for (auto& m : matches) {
        Serial.printf("Trying BSSID %s (idx=%d) ch=%d rssi=%d\n", m.bss.c_str(), m.idx, m.ch, m.rssi);
        // get raw bssid bytes
        uint8_t bssid[6];
        // WiFi.BSSID(i) returns pointer to bytes
        const uint8_t* raw = WiFi.BSSID(m.idx);
        memcpy(bssid, raw, 6);
        tryConnectWithBSSID(bssid, m.ch, 25000);
        if (WiFi.status() == WL_CONNECTED) break;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("All attempts failed. Check password, router auth mode (WPA3/Enterprise?), MAC filter or captive portal.");
    }
    else {
        // simple HTTP test
        HTTPClient http;
        http.begin("http://asmeteo.pl/");
        int code = http.GET();
        Serial.printf("HTTP status: %d\n", code);
        http.end();
    }
}

void loop() {
    delay(1000);
}