#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>

ESP8266WiFiMulti wifiMulti;
WiFiClient wifiClient;

String id() {
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH-2], HEX);
    macID = macID + String(mac[WL_MAC_ADDR_LENGTH-1], HEX);
    return macID;
}
void start_AP_mode() {
    IPAddress AP_IP(192, 168, 4, 1);

    Serial.println("Attempt to start WIFI AP");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(SSID_AP + "_" + id(), PASSWORD_AP);

    Serial.println("WiFi AP is up, see" + SSID_AP + " " + id());

}
void start_client_mode() {
    Serial.print("Trying to set client to ssid");
    Serial.println(SSID_CLI);
    wifiMulti.addAP(SSID_CLI.c_str(), PASSWORD_CLI.c_str());
    while(wifiMulti.run() != WL_CONNECTED) {

    }

    Serial.println("Succesful connected to router");
}
void client_mode() {

}
void init_WIFI(bool mode) {
    if (mode == WIFI_START_MODE_CLIENT) {
        start_client_mode();
        String ap = WiFi.localIP().toString();
        Serial.println("My IP address IS:" + ap);
    } else {
        start_AP_mode();
    }
}