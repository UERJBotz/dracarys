#ifdef ESP32
  #include <WiFi.h>
  #include <esp_now.h>
#else
  #include <ESP8266WiFi.h>
  #include <espnow.h>
  #define esp_err_t int
  #define wifi_mode_t WiFiMode_t
  #define ESP_OK 0
#endif

uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t gesonel[6]   = {0x18, 0xfe, 0x34, 0xe1, 0xb3, 0x3b};
uint8_t controle[6]  = {0x64, 0xe8, 0x33, 0x88, 0x0a, 0xbc};

typedef struct packet {
    uint8_t id;
    uint8_t len;
    char vels[25];
} Packet;

void init_wifi(wifi_mode_t mode=WIFI_STA) {
    WiFi.mode(mode);
  #ifdef ESP32
    WiFi.STA.begin();
  #else
    WiFi.begin();
  #endif
    esp_err_t err = esp_now_init();
    assert (err == ESP_OK);
}

uint8_t* get_mac_addr() {
    static uint8_t mac_addr[6];
    return WiFi.macAddress(mac_addr);
}
