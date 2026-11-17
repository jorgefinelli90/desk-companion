#pragma once
#define FW_VERSION     "1.1.1"       // subilo en cada cambio: se ve en la web y en la OTA

// Clave para la pagina de actualizacion http://deskbuddy.local/ota
#define OTA_USER       "admin"
#define OTA_PASS       "deskbuddy"
// ---------------------------------------------------------------- Hardware
// ESP32-C3 Super Mini + OLED 1.3" SH1106 (I2C) + sensor tactil TTP223
#define PIN_SDA        8
#define PIN_SCL        9
#define PIN_TOUCH      7
// El nivel del sensor en reposo se detecta solo al arrancar (no lo toques al enchufar)

// Si tu pantalla es SSD1306 (0.96") en vez de SH1106 (1.3"), descomenta:
// #define USE_SSD1306

// Las Super Mini tienen una antena floja: bajar la potencia TX mejora
// muchisimo la conexion. Si conecta bien sin esto, podes comentarlo.
#define WIFI_LOW_TX_POWER

// ---------------------------------------------------------------- Red
#define AP_PREFIX      "DeskBuddy"   // red propia: DeskBuddy-XXXX
#define AP_PASSWORD    "deskbuddy"   // min 8 caracteres
#define MDNS_NAME      "deskbuddy"   // http://deskbuddy.local
#define WIFI_AP_AFTER  60000         // ms sin router -> abre tambien la red propia para reconfigurar

// ---------------------------------------------------------------- Hora
// Zona horaria (formato POSIX). Argentina = "<-03>3". Espana = "CET-1CEST,M3.5.0,M10.5.0/3"
// Mexico (CDMX) = "CST6". Chile = "<-04>4<-03>,M9.1.6/24,M4.1.6/24"
#define TZ_INFO        "<-03>3"
#define AUTO_CHANGE_MIN 10           // en modo automatico, cambia de cara cada X minutos

// ---------------------------------------------------------------- Mensajes
#define MSG_HISTORY    12            // mensajes guardados en RAM
#define MSG_MAX_LEN    280
#define FROM_MAX_LEN   16
#define MSG_SHOW_MS    15000         // cuanto queda el mensaje en pantalla
#define MSG_FLASH_MS   2000          // flash a brillo maximo al llegar
