#include <WiFi.h>
#include <esp_wifi.h>

// ================= CONFIG =================
const char* SSID = "IoT";
const char* PASS = "PhieV5ai";

// ================= HELPERS =================
int rssiToQuality(int rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

const char* authModeName(wifi_auth_mode_t mode) {
    switch (mode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3_PSK";
    default: return "UNKNOWN";
    }
}

// ================= WIFI CONNECT =================
bool connectWiFi() {
    Serial.printf("Connecting to SSID '%s'...\n", SSID);

    WiFi.disconnect(true);
    delay(200);

    WiFi.begin(SSID, PASS);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) { // ~20 sec
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ Connected!");
        Serial.print("IP: "); Serial.println(WiFi.localIP());
        Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
        Serial.print("MAC: "); Serial.println(WiFi.macAddress());
        return true;
    }
    else {
        Serial.println("❌ Failed to connect");
        return false;
    }
}

// ================= SAFE SCAN =================
void scanNetworks() {
    Serial.println("Scanning for WiFi networks...");

    int n = WiFi.scanNetworks();

    if (n <= 0) {
        Serial.println("No networks found");
    }
    else {
        Serial.printf("Found %d networks:\n", n);

        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            String bssid = WiFi.BSSIDstr(i);
            int quality = rssiToQuality(rssi);
            wifi_auth_mode_t auth = WiFi.encryptionType(i);

            Serial.printf("%2d: %-32s RSSI: %4d dBm  Quality: %3d%%  BSSID: %s  Ch: %2d  Auth: %s\n",
                i + 1,
                ssid.c_str(),
                rssi,
                quality,
                bssid.c_str(),
                WiFi.channel(i),
                authModeName(auth));
        }
    }

    WiFi.scanDelete(); // VERY IMPORTANT (heap stability)
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\nWiFi Stable Client");

    // Critical settings for stability
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                 // disable power save (huge fix)
    esp_wifi_set_ps(WIFI_PS_NONE);        // extra safety

    // Initial connect
    if (!connectWiFi()) {
        Serial.println("Initial connection failed, will retry in loop...");
    }
}

// ================= LOOP =================
void loop() {

    // 🔁 Maintain connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n⚠️ WiFi lost, reconnecting...");
        connectWiFi();

        // Optional: scan ONLY when disconnected (safe)
        scanNetworks();
    }

    // Example: print status every 10s
    Serial.print("Connected, RSSI: ");
    Serial.println(WiFi.RSSI());

    delay(10000);
}