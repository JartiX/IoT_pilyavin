#include "Config.h"
#include "WIFI.h"
#include "Server.h"
#include "MQTT.h"

void setup() {
    Serial.begin(9600);
    init_WIFI(WIFI_START_MODE_CLIENT);
    delay(500);
    Serial.println("Our id is:");
    Serial.println(id());
    init_server();
    init_MQTT();
    mqtt_cli.subscribe("esp8266/cmd");
}

void loop() {
    server.handleClient();
    mqtt_cli.loop();
}