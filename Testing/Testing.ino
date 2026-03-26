#include <FS.h>
#include <LittleFS.h>

void setup() {
    Serial.begin(9600);
    while (!Serial) {}
    delay(500);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        while (true) {}
    }
    Serial.println("LittleFS Mount success");

    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("Failed to open root directory");
        while (true) {}
    }

    Serial.println("Listing root directory:");
    File file = root.openNextFile();
    while (file) {
        Serial.print(file.name());
        Serial.print("\tSIZE: ");
        Serial.print(file.size());
        Serial.println(" B");
        file = root.openNextFile();
    }

    Serial.println("Listing complete");
}

void loop() {}