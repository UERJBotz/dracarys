#ifdef ESP32
  #include <WiFi.h>
  #include <esp_now.h>

  typedef int esp_now_len_t;
#else
  #include <ESP8266WiFi.h>
  #include <espnow.h>
  #define wifi_mode_t WiFiMode_t
  #define esp_err_t int
  #define ESP_OK 0

  // isso é uma "mentira" sobre o tipo da info de callback
  typedef struct {
      uint8_t src_addr[6]; /* (outros campos ignorados) */
  } esp_now_recv_info_t;
  typedef unsigned char esp_now_len_t;
#endif

// robôs
uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t gesonel[6]   = {0x18, 0xfe, 0x34, 0xe1, 0xb3, 0x3b};

// controles
uint8_t controle_branco[6] = {0x64, 0xe8, 0x33, 0x88, 0x0a, 0xbc};
uint8_t controle_preto_j_verm[6] = {0x60, 0x55, 0xF9, 0x9D, 0x4D, 0xB4};
uint8_t controle_roxo_j_verm[6] = {0x70, 0x04, 0x1D, 0x37, 0x71, 0xC0};

uint8_t pistola_elton[6] = {0x60, 0x55, 0xf9, 0x9f, 0xa5, 0x7c};

// biblioteca em si
typedef void (*recv_cb_t)(const esp_now_recv_info_t* info, const uint8_t* data, esp_now_len_t len);

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

void espnow_setup(recv_cb_t on_recv) {
    init_wifi();
    uint8_t* mac_addr = get_mac_addr();

    Serial.printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    esp_now_register_recv_cb(esp_now_recv_cb_t(on_recv));
}
