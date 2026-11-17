#pragma once
#define FW_VERSION     "1.0.0"       // subilo en cada cambio: se ve en la web y en la OTA

// Clave para la pagina de actualizacion http://deskbuddy.local/ota
#define OTA_USER       "admin"
#define OTA_PASS       "deskbuddy"
// ---------------------------------------------------------------- Hardware
// ESP32-C3 Super Mini + OLED 1.3" SH1106 (I2C) + sensor tactil TTP223
#define PIN_SDA        8
#define PIN_SCL        9
#define PIN_TOUCH      7
#define TOUCH_ACTIVE   HIGH     // TTP223: OUT = HIGH cuando se toca

// Si tu pantalla es SSD1306 (0.96") en vez de SH1106 (1.3"), descomenta:
// #define USE_SSD1306

// Las Super Mini tienen una antena floja: bajar la potencia TX mejora
// muchisimo la conexion. Si conecta bien sin esto, podes comentarlo.
#define WIFI_LOW_TX_POWER

// ---------------------------------------------------------------- Red
#define AP_PREFIX      "DeskBuddy"   // red propia: DeskBuddy-XXXX
#define AP_PASSWORD    "deskbuddy"   // min 8 caracteres
#define MDNS_NAME      "deskbuddy"   // http://deskbuddy.local
#define WIFI_TIMEOUT   15000         // ms intentando conectar al router

// ---------------------------------------------------------------- Mensajes
#define MSG_HISTORY    12            // mensajes guardados en RAM
#define MSG_MAX_LEN    280
#define FROM_MAX_LEN   16
