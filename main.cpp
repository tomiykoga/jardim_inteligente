#include   <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("Ola! O ESP32 esta funcionando.");
}


void loop() {
    Serial.println("Tempo ligado (s): ");
    Serial.println(millis() / 1000);
    delay(2000);
}
