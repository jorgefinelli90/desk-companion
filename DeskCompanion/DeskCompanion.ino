// ============================================================================
//  Desk Companion - firmware propio
//  ESP32-C3 Super Mini + OLED 1.3" SH1106 + sensor tactil TTP223
//
//  - 23 expresiones animadas: fija o automatica segun la hora del dia
//  - Mensajeria local: cualquiera en tu red entra a http://deskbuddy.local
//    (o a la IP) y manda un mensaje que aparece en la pantalla
//  - Tactil: tap = siguiente expresion / cierra el mensaje
//            mantener 1.5 s = info de red y hora
//  - Actualizacion OTA desde el navegador: http://deskbuddy.local/ota
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Wire.h>
#include <Update.h>
#include <U8g2lib.h>
#include <vector>
#include <time.h>
#include <esp_system.h>

#include "config.h"
#include "face.h"
#include "schedule.h"
#include "companion.h"
#include "weather.h"
#include "webui.h"

#ifdef USE_SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#else
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
#endif

WebServer server(80);
DNSServer dns;
Preferences prefs;
FaceEngine face;

// ---------------------------------------------------------------- estado
enum Mode { MODE_FACE, MODE_MSG_INTRO, MODE_MSG, MODE_INFO, MODE_SETUP, MODE_CLOCK, MODE_BUDDY, MODE_WEATHER };
Mode mode = MODE_FACE;
uint32_t modeAt = 0;

String devName = "Desk Buddy";
uint8_t brightness = 160;
bool flipped = false;
uint8_t savedExpr = 0;
uint32_t reactionUntil = 0;

String toastText;
uint32_t toastUntil = 0;

// Reloj periodico
bool clockOn = false;
uint16_t clockEvery = 30;      // cada cuantos segundos
uint8_t clockFor = 5;          // cuantos segundos se ve
uint32_t lastClockEnd = 0;

// Clima
bool weatherOn = false;        // pantalla de clima periodica
uint16_t weatherEvery = 60;
uint8_t weatherFor = 5;
bool weatherReact = true;      // gestos y frases segun el clima
float wLat = -34.6037, wLon = -58.3816;   // Buenos Aires por defecto
String wCity = "Buenos Aires";
WeatherData wx;
uint32_t lastWeatherEnd = 0, nextWeatherFetch = 0;
WMood lastAlertMood = WM_NONE;
uint32_t lastAlertAt = 0;

// Apagado nocturno
bool nightOn = false;
uint16_t nightFrom = 23 * 60;  // minutos desde medianoche
uint16_t nightTo = 7 * 60;
bool nightMsgs = false;        // si llega un mensaje de noche, ¿se prende a mostrarlo?
bool screenOff = false;
uint32_t wakeUntil = 0;        // un toque de noche la prende un rato

// Modo companero
bool buddyOn = false;
uint8_t buddyEvery = 20;       // minutos promedio entre interacciones
String owner = "Kharlita";
uint32_t nextBuddyAt = 0;
int buddyPhrase = -1;
std::vector<String> buddyLines;

bool autoMode = true;          // caras segun la hora
uint32_t nextAutoAt = 0;
int lastSlot = -1;

bool apMode = false;
bool online = false;
String apSsid, staSsid, staPass;
uint32_t staBeginAt = 0, offlineSince = 0;
String resetReason;

bool otaRunning = false;
size_t otaDone = 0, otaTotal = 0;
int otaShown = -1;

bool touchDown = false;
uint32_t touchAt = 0;
int touchIdle = LOW;           // nivel en reposo (se detecta al arrancar)

struct Msg {
  uint32_t id;
  String from, text;
  uint32_t at;
  bool read;
};
std::vector<Msg> msgs;
uint32_t nextMsgId = 1;
uint32_t currentMsgId = 0;

std::vector<String> msgLines;
const uint8_t* msgFont = u8g2_font_6x12_tf;
int msgLineH = 12;
bool msgCentered = false;
uint32_t msgShownAt = 0;
uint32_t msgDuration = MSG_SHOW_MS;

// ============================================================================
//  Utilidades de texto
// ============================================================================
static void appendCp(String& out, uint32_t cp) {
  if (cp < 0x80) out += (char)cp;
  else { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
}

// La fuente de la OLED cubre Latin-1 (acentos, n, signos de apertura).
// Los emojis comunes se traducen a texto, el resto se descarta.
String sanitize(const String& in, size_t maxChars) {
  String out;
  const uint8_t* s = (const uint8_t*)in.c_str();
  size_t len = in.length(), i = 0, n = 0;
  while (i < len && n < maxChars) {
    uint8_t c = s[i];
    int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
    if (extra < 0 || i + extra >= len + (extra ? 0 : 1)) { i++; continue; }
    uint32_t cp = extra == 0 ? c : extra == 1 ? (c & 0x1F) : extra == 2 ? (c & 0x0F) : (c & 0x07);
    for (int k = 1; k <= extra; k++) cp = (cp << 6) | (s[i + k] & 0x3F);
    i += extra + 1;

    const char* rep = nullptr;
    switch (cp) {
      case 0x2764: case 0x2665: case 0x1F60D: case 0x1F970: case 0x1F496: case 0x1F495: rep = "<3"; break;
      case 0x1F600: case 0x1F601: case 0x1F603: case 0x1F604: case 0x1F60A: case 0x1F642: case 0x263A: rep = ":)"; break;
      case 0x1F609: rep = ";)"; break;
      case 0x1F602: case 0x1F923: case 0x1F606: rep = "XD"; break;
      case 0x1F622: case 0x1F62D: case 0x1F641: case 0x2639: case 0x1F61E: rep = ":("; break;
      case 0x1F618: case 0x1F617: rep = ":*"; break;
      case 0x1F61B: case 0x1F61C: case 0x1F61D: rep = ":P"; break;
      case 0x1F62E: case 0x1F632: rep = ":O"; break;
      case 0x1F44D: rep = "(y)"; break;
      case 0x2615: rep = "[cafe]"; break;
      case 0x1F44B: rep = "o/"; break;
      case 0x2026: rep = "..."; break;
      case 0x2018: case 0x2019: rep = "'"; break;
      case 0x201C: case 0x201D: rep = "\""; break;
      case 0x2013: case 0x2014: rep = "-"; break;
      case 0x20AC: rep = "EUR"; break;
    }
    if (rep) { out += rep; n += strlen(rep); continue; }
    if (cp == '\n') { out += '\n'; n++; continue; }
    if (cp == '\t') cp = ' ';
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || cp > 0xFF) continue;
    appendCp(out, cp);
    n++;
  }
  out.trim();
  return out;
}

static int utf8Len(char c) {
  uint8_t u = (uint8_t)c;
  return u < 0x80 ? 1 : (u >> 5) == 6 ? 2 : (u >> 4) == 14 ? 3 : (u >> 3) == 30 ? 4 : 1;
}

static int textW(const String& s) { return u8g2.getUTF8Width(s.c_str()); }

// Corta el texto en lineas que entran en maxW (usa la fuente activa)
void wrapText(const String& text, int maxW, std::vector<String>& out) {
  out.clear();
  int p = 0;
  while (p <= (int)text.length()) {
    int nl = text.indexOf('\n', p);
    if (nl < 0) nl = text.length();
    String para = text.substring(p, nl);
    p = nl + 1;
    String line;
    int wp = 0;
    while (wp <= (int)para.length()) {
      int sp = para.indexOf(' ', wp);
      if (sp < 0) sp = para.length();
      String word = para.substring(wp, sp);
      wp = sp + 1;
      if (word.length() == 0) { if (sp >= (int)para.length()) break; continue; }
      String cand = line.length() ? line + " " + word : word;
      if (textW(cand) <= maxW) { line = cand; continue; }
      if (line.length()) { out.push_back(line); line = ""; }
      while (textW(word) > maxW) {            // palabra mas larga que la pantalla
        String part;
        int j = 0;
        while (j < (int)word.length()) {
          int cl = utf8Len(word[j]);
          String next = part + word.substring(j, j + cl);
          if (textW(next) > maxW) break;
          part = next; j += cl;
        }
        if (j == 0) { j = utf8Len(word[0]); part = word.substring(0, j); }
        out.push_back(part);
        word = word.substring(j);
      }
      line = word;
    }
    out.push_back(line);
    if (nl >= (int)text.length()) break;
  }
  while (out.size() > 1 && out.back().length() == 0) out.pop_back();
}

String jsonEsc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': break;
      case '\t': o += ' '; break;
      default:
        if ((uint8_t)c < 0x20) break;
        o += c;
    }
  }
  return o;
}

void showToast(const String& t, uint32_t ms = 1400) { toastText = t; toastUntil = millis() + ms; }
void setMode(Mode m) { mode = m; modeAt = millis(); }

// Tiempo transcurrido sin desbordar: si "t" quedo unos ms despues de "now"
// (pasa cuando el evento llega por la web dentro del mismo ciclo), da 0.
static inline uint32_t since(uint32_t t, uint32_t now) {
  return (int32_t)(now - t) < 0 ? 0 : now - t;
}

// ============================================================================
//  Expresiones
// ============================================================================
void setExpression(uint8_t i, bool toast = true, bool persist = true) {
  if (i >= EXPR_COUNT) return;
  savedExpr = i;
  reactionUntil = 0;
  face.set(i);
  if (persist) prefs.putUChar("expr", i);
  if (toast) showToast(EXPRESSIONS[i].name);
}

void setAutoMode(bool on) {
  autoMode = on;
  prefs.putBool("auto", on);
  lastSlot = -1;                       // fuerza elegir cara de la franja ya
  nextAutoAt = 0;
}

bool timeValid() { return time(nullptr) > 1700000000; }

int currentHour() {
  if (!timeValid()) return -1;
  struct tm t;
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return t.tm_hour;
}

String clockText() {
  if (!timeValid()) return "--:--";
  struct tm t;
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  return b;
}

bool isNightTime() {
  if (!nightOn || !timeValid() || nightFrom == nightTo) return false;
  struct tm t;
  time_t tt = time(nullptr);
  localtime_r(&tt, &t);
  uint16_t m = t.tm_hour * 60 + t.tm_min;
  if (nightFrom < nightTo) return m >= nightFrom && m < nightTo;
  return m >= nightFrom || m < nightTo;          // cruza la medianoche (ej. 23:00 -> 07:00)
}

String hhmm(uint16_t m) {
  char b[6];
  snprintf(b, sizeof(b), "%02d:%02d", m / 60, m % 60);
  return b;
}

int parseHHMM(const String& s) {
  int c = s.indexOf(':');
  if (c < 1) return -1;
  int h = s.substring(0, c).toInt(), m = s.substring(c + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

// Modo automatico: cada AUTO_CHANGE_MIN minutos (o al cambiar de franja)
// elige una cara de la franja horaria actual.
void autoTick(uint32_t now) {
  if (!autoMode || mode != MODE_FACE || reactionUntil) return;
  int h = currentHour();
  if (h < 0) return;                   // todavia sin hora (sin WiFi)
  int slot = scheduleSlot(h);
  if (slot == lastSlot && now < nextAutoAt) return;
  lastSlot = slot;
  nextAutoAt = now + AUTO_CHANGE_MIN * 60000UL;
  WMood m = weatherMood(wx);
  if (weatherReact && m != WM_NONE && h >= 7 && random(100) < 40) {   // el clima tambien le cambia el animo
    int e = exprIndexById(moodExpr(m));
    if (e >= 0) { setExpression(e, false, false); return; }
  }
  setExpression(pickForSlot(slot, savedExpr), false, false);
}

void react() {
  static const char* pool[] = {"joy", "love", "happy", "wink", "starstruck", "excited"};
  int i = exprIndexById(pool[random((long)(sizeof(pool) / sizeof(pool[0])))]);
  if (i < 0 || i == face.index()) i = exprIndexById("joy");
  face.set(i);
  reactionUntil = millis() + 2500;
}

// ============================================================================
//  Mensajes
// ============================================================================
int findMsg(uint32_t id) {
  for (size_t i = 0; i < msgs.size(); i++) if (msgs[i].id == id) return i;
  return -1;
}
int firstUnread() {
  for (size_t i = 0; i < msgs.size(); i++) if (!msgs[i].read) return i;
  return -1;
}
int unreadCount() {
  int n = 0;
  for (auto& m : msgs) if (!m.read) n++;
  return n;
}

void startIntro() {
  reactionUntil = 0;
  face.set(savedExpr, true);
  u8g2.setContrast(255);               // flashazo a brillo maximo
  setMode(MODE_MSG_INTRO);
}

uint32_t addMessage(const String& from, const String& text) {
  Msg m{nextMsgId++, from, text, millis(), false};
  msgs.push_back(m);
  while (msgs.size() > MSG_HISTORY) msgs.erase(msgs.begin());
  bool quiet = isNightTime() && !nightMsgs && since(wakeUntil, millis()) > 0;
  if (!quiet && mode != MODE_MSG && mode != MODE_MSG_INTRO) startIntro();   // de noche espera a la manana
  return m.id;
}

void prepareMessage(const Msg& m) {
  // primero intenta letra grande; si no entra en 3 lineas, letra chica + scroll
  u8g2.setFont(u8g2_font_helvB10_tf);
  wrapText(m.text, 122, msgLines);
  if (msgLines.size() <= 3) {
    msgFont = u8g2_font_helvB10_tf; msgLineH = 15; msgCentered = true;
  } else {
    u8g2.setFont(u8g2_font_6x12_tf);
    wrapText(m.text, 120, msgLines);
    msgFont = u8g2_font_6x12_tf; msgLineH = 12; msgCentered = false;
  }
  msgShownAt = millis();
  // 15 s, o mas si el texto es largo y necesita una pasada completa de scroll
  int overflow = (int)msgLines.size() * msgLineH - (64 - 14) + 3;
  uint32_t scrollTime = overflow > 0 ? SCROLL_PAUSE_MS * 2 + overflow * SCROLL_PX_MS : 0;
  msgDuration = max((uint32_t)MSG_SHOW_MS, scrollTime + 500);
}

void openNextMessage() {
  u8g2.setContrast(brightness);        // fin del flash
  int i = firstUnread();
  if (i < 0) { setMode(MODE_FACE); return; }
  currentMsgId = msgs[i].id;
  prepareMessage(msgs[i]);
  setMode(MODE_MSG);
}

void dismissMessage() {
  int i = findMsg(currentMsgId);
  if (i >= 0) { msgs[i].read = true; msgs[i].text = ""; }   // privacidad: no queda guardado
  if (firstUnread() >= 0) startIntro();   // hay otro: flash y lo muestra
  else { setMode(MODE_FACE); react(); }   // contento de haberlo leido
}

// ============================================================================
//  Pantallas
// ============================================================================
void drawCentered(const String& s, int y) {
  u8g2.drawUTF8((128 - textW(s)) / 2, y, s.c_str());
}

void drawEnvelope(int x, int y, int w, int h) {
  u8g2.drawRFrame(x, y, w, h, 2);
  u8g2.drawLine(x + 1, y + 1, x + w / 2, y + h / 2 + 1);
  u8g2.drawLine(x + w - 2, y + 1, x + w / 2, y + h / 2 + 1);
}

void drawMsgIntro(uint32_t now) {
  uint32_t e = since(modeAt, now);
  if (e < 700 && (e / 117) % 2 == 0) {  // 3 destellos de pantalla completa
    u8g2.drawBox(0, 0, 128, 64);
    return;
  }
  int bounce = (int)(fabsf(sinf(e * 0.008f)) * -6);
  int s = e < 250 ? (int)(e / 250.f * 36) : 36;      // aparece creciendo
  if (s < 4) s = 4;
  int h = s * 2 / 3;
  u8g2.drawBox(64 - s / 2, 10 + bounce, s, h);
  u8g2.setDrawColor(0);
  u8g2.drawLine(64 - s / 2 + 1, 11 + bounce, 64, 10 + bounce + h / 2 + 2);
  u8g2.drawLine(64 + s / 2 - 2, 11 + bounce, 64, 10 + bounce + h / 2 + 2);
  u8g2.setDrawColor(1);
  if ((e / 220) % 2 == 0) {                            // rayitas de "nuevo"
    for (int k = 0; k < 3; k++)
      for (int side = -1; side <= 1; side += 2) {
        float a = 0.35f + k * 0.35f;                     // angulo desde la horizontal
        int x0 = 64 + side * (int)(cosf(a) * 25), y0 = 22 + bounce - (int)(sinf(a) * 14);
        int x1 = 64 + side * (int)(cosf(a) * 31), y1 = 22 + bounce - (int)(sinf(a) * 19);
        u8g2.drawLine(x0, max(y0, 0), x1, max(y1, 0));
      }
  }
  int i = firstUnread();
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCentered("Mensaje de", 48);
  u8g2.setFont(u8g2_font_helvB08_tf);
  if (i >= 0) drawCentered(msgs[i].from, 61);
  if (e > MSG_INTRO_MS) openNextMessage();
}

void drawMessage(uint32_t now) {
  int i = findMsg(currentMsgId);
  if (i < 0) { openNextMessage(); return; }
  uint32_t shown = since(msgShownAt, now);
  if (shown > msgDuration) { dismissMessage(); return; }   // vuelve a las caras
  const Msg& m = msgs[i];

  // cabecera invertida
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  drawEnvelope(2, 2, 11, 8);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawUTF8(17, 10, m.from.c_str());
  int pending = unreadCount() - 1;
  if (pending > 0) {
    String p = "+" + String(pending);
    u8g2.drawStr(125 - u8g2.getStrWidth(p.c_str()), 10, p.c_str());
  }
  u8g2.setDrawColor(1);

  // cuerpo con scroll automatico si no entra
  const int top = 14, areaH = 64 - top;
  u8g2.setFont(msgFont);
  int totalH = msgLines.size() * msgLineH;
  int y0 = top, off = 0, overflow = totalH - areaH + 3;   // +3: que la ultima linea entre entera
  if (overflow <= 0) {
    y0 = top + (areaH - totalH) / 2;
  } else {
    const uint32_t PAUSE = SCROLL_PAUSE_MS, PX_MS = SCROLL_PX_MS;
    uint32_t scrollDur = overflow * PX_MS, cycle = PAUSE * 2 + scrollDur;
    uint32_t e = shown % cycle;
    off = e < PAUSE ? 0 : e < PAUSE + scrollDur ? (e - PAUSE) / PX_MS : overflow;
    int barH = max(6, areaH * areaH / totalH);
    u8g2.drawBox(126, top + (areaH - barH) * off / overflow, 2, barH);
  }
  u8g2.setClipWindow(0, top, 125, 64);
  int asc = u8g2.getAscent();
  for (size_t k = 0; k < msgLines.size(); k++) {
    int y = y0 + k * msgLineH + asc + (msgLineH - asc) / 2 - off;
    if (y < top - msgLineH || y > 64 + msgLineH) continue;
    int x = msgCentered ? (126 - textW(msgLines[k])) / 2 : 2;
    u8g2.drawUTF8(x, y, msgLines[k].c_str());
  }
  u8g2.setMaxClipWindow();
}

void drawInfo() {
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawUTF8(0, 12, devName.c_str());
  String c = clockText();
  u8g2.drawStr(128 - u8g2.getStrWidth(c.c_str()), 12, c.c_str());
  u8g2.drawHLine(0, 16, 128);
  u8g2.setFont(u8g2_font_6x10_tf);
  if (online) {
    u8g2.drawUTF8(0, 28, ("WiFi: " + WiFi.SSID()).c_str());
    u8g2.drawUTF8(0, 39, ("IP: " + WiFi.localIP().toString()).c_str());
    u8g2.drawUTF8(0, 50, MDNS_NAME ".local");
  } else if (apMode) {
    u8g2.drawUTF8(0, 28, ("Red: " + apSsid).c_str());
    u8g2.drawUTF8(0, 39, "Clave: " AP_PASSWORD);
    u8g2.drawUTF8(0, 50, ("IP: " + WiFi.softAPIP().toString()).c_str());
  } else {
    u8g2.drawUTF8(0, 28, staSsid.length() ? ("Buscando " + staSsid).c_str() : "Sin WiFi");
  }
  String last = String(autoMode ? "Auto" : "Fija") + "  v" FW_VERSION;
  if (resetReason.length()) last += " " + resetReason;
  u8g2.drawUTF8(0, 61, last.c_str());
}

void drawSetup() {
  u8g2.setFont(u8g2_font_helvB08_tf);
  drawCentered("Configurar WiFi", 10);
  u8g2.drawHLine(0, 13, 128);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawUTF8(0, 25, "1. Conectate a:");
  u8g2.drawUTF8(6, 35, apSsid.c_str());
  u8g2.drawUTF8(6, 45, "clave: " AP_PASSWORD);
  u8g2.drawUTF8(0, 57, "2. Abrí 192.168.4.1");
}

void drawToast(uint32_t now) {
  if (now >= toastUntil) return;
  u8g2.setFont(u8g2_font_6x10_tf);
  int w = textW(toastText) + 10, x = (128 - w) / 2;
  u8g2.setDrawColor(0); u8g2.drawRBox(x - 1, 49, w + 2, 15, 4);
  u8g2.setDrawColor(1); u8g2.drawRFrame(x, 50, w, 14, 4);
  u8g2.drawUTF8(x + 5, 60, toastText.c_str());
}

// Iconito de WiFi titilando arriba al centro mientras busca el router
void drawWifiHint(uint32_t now) {
  if (online || !staSsid.length() || (now / 600) % 2) return;
  for (int r = 2; r <= 6; r += 2)
    u8g2.drawCircle(64, 8, r, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  u8g2.drawPixel(64, 8);
}

// ---------------------------------------------------------------- reloj
static const char* DIAS[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
static const char* MESES[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};

void drawClock(uint32_t now) {
  struct tm t;
  time_t tt = time(nullptr);
  localtime_r(&tt, &t);
  char hh[3], mm[3];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  u8g2.setFont(u8g2_font_logisoso32_tn);
  int wd = u8g2.getStrWidth("00"), wc = u8g2.getStrWidth(":");
  int x = (128 - (wd * 2 + wc + 4)) / 2;
  u8g2.drawStr(x, 40, hh);
  if ((now / 500) % 2 == 0) u8g2.drawStr(x + wd + 2, 38, ":");
  u8g2.drawStr(x + wd + wc + 4, 40, mm);
  u8g2.setFont(u8g2_font_6x10_tf);
  String d = String(DIAS[t.tm_wday]) + " " + String(t.tm_mday) + " " + MESES[t.tm_mon];
  drawCentered(d, 58);
}

// ---------------------------------------------------------------- clima
String tempText(float t);
String fillPhrase(const char* tpl);
void showBuddy(const char* exprId, const String& text);

void drawWeather(uint32_t now) {
  if (!wx.ok) { setMode(MODE_FACE); return; }
  drawWeatherIcon(u8g2, wx.code, wx.day, 28, 22, now);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  String t = String((int)lroundf(wx.temp));
  int w = u8g2.getStrWidth(t.c_str());
  int x = 92 - (w + 8) / 2;
  u8g2.drawStr(x, 34, t.c_str());
  u8g2.drawCircle(x + w + 5, 13, 3);                       // simbolo de grados
  u8g2.drawCircle(x + w + 5, 13, 2);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCentered(weatherDesc(wx.code), 51);
  String l = tempText(wx.tmin) + " / " + tempText(wx.tmax);
  if (wx.rainProb >= 20) l += "  lluvia " + String(wx.rainProb) + "%";
  drawCentered(l, 63);
}

void updateWeather(uint32_t now) {
  if (!online || (!weatherOn && !weatherReact)) return;
  if (nextWeatherFetch && since(nextWeatherFetch, now) == 0) return;
  bool ok = fetchWeather(wLat, wLon, wx);
  if (!ok && wx.ok && since(wx.at, millis()) > 3 * 3600000UL) wx.ok = false;   // datos viejos: no los muestra
  nextWeatherFetch = millis() + (ok ? WEATHER_REFRESH_MIN * 60000UL : 120000UL);
  Serial.printf("Clima: %s %.1f° cod %d lluvia %d%%\n", ok ? "ok" : wx.error.c_str(), wx.temp, wx.code, wx.rainProb);
}

// Si el clima se pone "notable" (lluvia, tormenta, frio, calor), avisa una vez
void weatherAlert(uint32_t now) {
  if (!weatherReact || !wx.ok || mode != MODE_FACE || reactionUntil || screenOff) return;
  WMood m = weatherMood(wx);
  if (m == WM_NONE || m == WM_SUNNY) { lastAlertMood = m; return; }
  if (m == lastAlertMood && since(lastAlertAt, now) < WEATHER_ALERT_GAP_MIN * 60000UL) return;
  if (currentHour() >= 0 && (currentHour() < 7 || currentHour() >= 23)) return;   // de noche no molesta
  lastAlertMood = m;
  lastAlertAt = now;
  showBuddy(moodExpr(m), fillPhrase(moodPhrase(m)));
}

// ---------------------------------------------------------------- companero
String buddyText(int i) {
  String s = PHRASES[i].text;
  s.replace("{n}", owner.c_str());
  return s;
}

String tempText(float t) { return String((int)lroundf(t)) + "°"; }

String fillPhrase(const char* tpl) {
  String s = tpl;
  s.replace("{n}", owner.c_str());
  s.replace("{t}", tempText(wx.temp).c_str());
  s.replace("{p}", String(wx.rainProb).c_str());
  return s;
}

void showBuddy(const char* exprId, const String& text) {
  int e = exprIndexById(exprId);
  if (e >= 0) face.set(e);
  u8g2.setFont(u8g2_font_helvB10_tf);
  wrapText(text, 112, buddyLines);
  setMode(MODE_BUDDY);
}

void startBuddy(uint32_t now) {
  WMood m = weatherMood(wx);
  if (weatherReact && m != WM_NONE && random(100) < 35) {     // a veces habla del clima
    showBuddy(moodExpr(m), fillPhrase(moodPhrase(m)));
    return;
  }
  int i = pickPhrase(currentHour(), buddyPhrase);
  if (i < 0) return;
  buddyPhrase = i;
  showBuddy(PHRASES[i].expr, buddyText(i));
}

void scheduleBuddy(uint32_t now) {
  uint32_t avg = (uint32_t)buddyEvery * 60000UL;          // al azar entre 50% y 150% del promedio
  nextBuddyAt = now + avg / 2 + (uint32_t)random((long)avg + 1);
}

// 2 s la cara, despues el globo con la frase 5 s, y vuelve
void drawBuddy(uint32_t now) {
  uint32_t e = since(modeAt, now);
  if (e < BUDDY_FACE_MS) { face.draw(now); return; }
  if (e > BUDDY_FACE_MS + BUDDY_TEXT_MS) {
    face.set(savedExpr);
    setMode(MODE_FACE);
    face.draw(now);
    return;
  }
  int n = buddyLines.size(), lh = 15, h = n * lh + 8;
  if (h > 54) h = 54;
  int y = (54 - h) / 2;
  u8g2.drawRFrame(2, y, 124, h, 6);
  u8g2.drawRFrame(3, y + 1, 122, h - 2, 5);
  int ty = y + h - 1;                                     // colita del globo
  u8g2.setDrawColor(0); u8g2.drawBox(52, ty - 1, 14, 3); u8g2.setDrawColor(1);
  u8g2.drawLine(52, ty, 58, 63); u8g2.drawLine(53, ty, 59, 63);
  u8g2.drawLine(66, ty, 58, 63); u8g2.drawLine(65, ty, 59, 63);
  u8g2.setFont(u8g2_font_helvB10_tf);
  int asc = u8g2.getAscent();
  for (int k = 0; k < n && k < 3; k++)
    u8g2.drawUTF8((128 - textW(buddyLines[k])) / 2, y + 4 + k * lh + asc + 1, buddyLines[k].c_str());
}

void renderFrame(uint32_t now) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  {
    switch (mode) {
      case MODE_FACE:      face.draw(now); break;
      case MODE_MSG_INTRO: drawMsgIntro(now); break;
      case MODE_MSG:       drawMessage(now); break;
      case MODE_INFO:      drawInfo(); break;
      case MODE_SETUP:     drawSetup(); break;
      case MODE_CLOCK:     drawClock(now); break;
      case MODE_BUDDY:     drawBuddy(now); break;
      case MODE_WEATHER:   drawWeather(now); break;
    }
    if (mode == MODE_FACE) drawWifiHint(now);   // sin textos debajo de las caras
  }
  u8g2.sendBuffer();
}

// ============================================================================
//  Tactil
// ============================================================================
void onTap() {
  switch (mode) {
    case MODE_MSG:       dismissMessage(); break;
    case MODE_MSG_INTRO: openNextMessage(); break;
    case MODE_INFO:
    case MODE_SETUP:
    case MODE_CLOCK:     lastClockEnd = millis(); setMode(MODE_FACE); break;
    case MODE_WEATHER:   lastWeatherEnd = millis(); setMode(MODE_FACE); break;
    case MODE_BUDDY:     face.set(savedExpr); setMode(MODE_FACE); break;
    case MODE_FACE:
      // pasa a la siguiente cara; en modo automatico queda hasta el proximo cambio
      setExpression((savedExpr + 1) % EXPR_COUNT, true, !autoMode);
      if (autoMode) nextAutoAt = millis() + AUTO_CHANGE_MIN * 60000UL;
      break;
  }
}

void forgetWifi() {
  prefs.remove("ssid");
  prefs.remove("pass");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tf);
  drawCentered("WiFi borrado", 30);
  drawCentered("Reiniciando...", 46);
  u8g2.sendBuffer();
  delay(1200);
  ESP.restart();
}

void handleTouch(uint32_t now) {
  static bool infoFired = false;
  static uint32_t lastRelease = 0;
  bool raw = digitalRead(PIN_TOUCH) != touchIdle;

  if (raw && !touchDown) {
    if (now - lastRelease < 40) return;          // antirrebote
    touchDown = true; touchAt = now; infoFired = false;
  } else if (raw && touchDown) {
    uint32_t held = now - touchAt;
    if (held > 1500 && !infoFired && (mode == MODE_FACE || mode == MODE_SETUP)) {
      infoFired = true;
      reactionUntil = 0; face.set(savedExpr, true);
      setMode(MODE_INFO);
    }
    if (held > 20000) {                          // sensor trabado o en modo "toggle": recalibra
      touchIdle = digitalRead(PIN_TOUCH);
      touchDown = false;
    }
  } else if (!raw && touchDown) {
    touchDown = false;
    lastRelease = now;
    if (!infoFired && now - touchAt > 20) {
      if (screenOff) wakeUntil = now + NIGHT_WAKE_MS;     // de noche: solo prende la pantalla un rato
      else {
        if (isNightTime()) wakeUntil = now + NIGHT_WAKE_MS; // y cada toque estira ese rato
        onTap();
      }
    }
  }
}

// ============================================================================
//  WiFi
// ============================================================================
void lowerTxPower() {
#ifdef WIFI_LOW_TX_POWER
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
}

void startMDNS() {
  MDNS.end();
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
}

void startAP(bool showSetup) {
  if (apMode) return;
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  lowerTxPower();
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
  dns.start(53, "*", WiFi.softAPIP());
  startMDNS();
  if (showSetup) setMode(MODE_SETUP);
}

void staBegin() {
  WiFi.disconnect();
  WiFi.begin(staSsid.c_str(), staPass.c_str());
  lowerTxPower();
  staBeginAt = millis();
}

void onOnline() {
  online = true;
  if (apMode) {                        // ya no hace falta la red propia
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    lowerTxPower();
    apMode = false;
    if (mode == MODE_SETUP) setMode(MODE_FACE);
  }
  startMDNS();
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  showToast("WiFi OK");
  Serial.printf("WiFi conectado: %s\n", WiFi.localIP().toString().c_str());
}

// Nunca bloquea: las caras se ven desde el arranque y el WiFi se conecta solo.
void startWiFi() {
  WiFi.persistent(false);
  WiFi.setHostname(MDNS_NAME);
  WiFi.setAutoReconnect(true);
  offlineSince = millis();
  if (staSsid.length()) {
    WiFi.mode(WIFI_STA);
    lowerTxPower();                    // ANTES de conectar: evita reinicios por pico de consumo
    staBegin();
  } else {
    startAP(true);                     // primera vez: pantalla de configuracion
  }
}

void wifiLoop(uint32_t now) {
  if (!staSsid.length()) return;
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !online) onOnline();
  if (!up && online) { online = false; offlineSince = now; }
  if (up) return;
  bool apBusy = apMode && WiFi.softAPgetStationNum() > 0;   // no molestar si alguien configura
  if (!apBusy && since(staBeginAt, now) > 15000) staBegin();
  if (!apMode && since(offlineSince, now) > WIFI_AP_AFTER) startAP(false);
}

// ============================================================================
//  Servidor web / API
// ============================================================================
void sendJson(const String& s, int code = 200) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", s);
}

void handleState() {
  String j;
  j.reserve(3500);
  j += "{\"name\":\"" + jsonEsc(devName) + "\"";
  j += ",\"expr\":\"" + String(EXPRESSIONS[savedExpr].id) + "\"";
  j += ",\"ap\":" + String(!online && apMode ? "true" : "false");
  j += ",\"online\":" + String(online ? "true" : "false");
  j += ",\"ssid\":\"" + jsonEsc(online ? WiFi.SSID() : (apMode ? apSsid : staSsid)) + "\"";
  j += ",\"ip\":\"" + (online ? WiFi.localIP() : WiFi.softAPIP()).toString() + "\"";
  j += ",\"rssi\":" + String(online ? WiFi.RSSI() : 0);
  j += ",\"auto\":" + String(autoMode ? "true" : "false");
  j += ",\"time\":\"" + clockText() + "\"";
  j += ",\"clock\":" + String(clockOn ? "true" : "false");
  j += ",\"clockEvery\":" + String(clockEvery);
  j += ",\"clockFor\":" + String(clockFor);
  j += ",\"buddy\":" + String(buddyOn ? "true" : "false");
  j += ",\"buddyEvery\":" + String(buddyEvery);
  j += ",\"owner\":\"" + jsonEsc(owner) + "\"";
  j += ",\"night\":" + String(nightOn ? "true" : "false");
  j += ",\"nightFrom\":\"" + hhmm(nightFrom) + "\"";
  j += ",\"nightTo\":\"" + hhmm(nightTo) + "\"";
  j += ",\"nightMsgs\":" + String(nightMsgs ? "true" : "false");
  j += ",\"screenOff\":" + String(screenOff ? "true" : "false");
  j += ",\"weather\":" + String(weatherOn ? "true" : "false");
  j += ",\"weatherEvery\":" + String(weatherEvery);
  j += ",\"weatherFor\":" + String(weatherFor);
  j += ",\"weatherReact\":" + String(weatherReact ? "true" : "false");
  j += ",\"city\":\"" + jsonEsc(wCity) + "\"";
  j += ",\"wx\":{\"ok\":" + String(wx.ok ? "true" : "false");
  if (wx.ok) {
    j += ",\"temp\":" + String(wx.temp, 1) + ",\"min\":" + String(wx.tmin, 0) + ",\"max\":" + String(wx.tmax, 0) +
         ",\"rain\":" + String(wx.rainProb) + ",\"desc\":\"" + weatherDesc(wx.code) + "\"" +
         ",\"ago\":" + String(since(wx.at, millis()) / 60000);
  }
  j += ",\"error\":\"" + jsonEsc(wx.error) + "\"}";
  j += ",\"bright\":" + String(brightness);
  j += ",\"flip\":" + String(flipped ? "true" : "false");
  j += ",\"unread\":" + String(unreadCount());
  j += ",\"version\":\"" FW_VERSION "\"";
  j += ",\"exprs\":[";
  for (uint8_t i = 0; i < EXPR_COUNT; i++) {
    if (i) j += ',';
    j += "{\"id\":\"" + String(EXPRESSIONS[i].id) + "\",\"name\":\"" + String(EXPRESSIONS[i].name) +
         "\",\"emoji\":\"" + String(EXPRESSIONS[i].emoji) + "\"}";
  }
  j += "],\"msgs\":[";
  uint32_t now = millis();
  for (size_t i = 0; i < msgs.size(); i++) {
    if (i) j += ',';
    j += "{\"id\":" + String(msgs[i].id) + ",\"from\":\"" + jsonEsc(msgs[i].from) + "\",\"read\":" + (msgs[i].read ? "true" : "false") +
         ",\"ago\":" + String((now - msgs[i].at) / 1000) + "}";
  }
  j += "]}";
  sendJson(j);
}

void handleAuto() {
  bool on = server.arg("on") != "0";
  setAutoMode(on);
  if (on) showToast("Modo automatico");
  sendJson(String("{\"ok\":true,\"auto\":") + (on ? "true" : "false") + "}");
}

void handleExpr() {
  String id = server.arg("id");
  int i = exprIndexById(id.c_str());
  if (i < 0 && id.length() && isDigit(id[0])) i = id.toInt();
  if (i < 0 || i >= EXPR_COUNT) { sendJson("{\"ok\":false,\"error\":\"expresion desconocida\"}", 400); return; }
  if (mode == MODE_INFO || mode == MODE_SETUP) setMode(MODE_FACE);
  if (autoMode) setAutoMode(false);    // elegida a mano: queda fija
  setExpression(i);
  sendJson("{\"ok\":true}");
}

void handleMsg() {
  String text = sanitize(server.arg("text"), MSG_MAX_LEN);
  String from = sanitize(server.arg("from"), FROM_MAX_LEN);
  from.replace("\n", " ");
  if (!text.length()) { sendJson("{\"ok\":false,\"error\":\"mensaje vacio\"}", 400); return; }
  if (!from.length()) from = "Anónimo";
  int e = exprIndexById(server.arg("expr").c_str());
  if (e >= 0) { savedExpr = e; prefs.putUChar("expr", e); }
  uint32_t id = addMessage(from, text);
  sendJson("{\"ok\":true,\"id\":" + String(id) + "}");
}

void handleSettings() {
  if (server.hasArg("name")) {
    String n = sanitize(server.arg("name"), 20);
    n.replace("\n", " ");
    if (n.length()) { devName = n; prefs.putString("name", n); }
  }
  if (server.hasArg("bright")) {
    brightness = constrain((int)server.arg("bright").toInt(), 1, 255);
    u8g2.setContrast(brightness);
    prefs.putUChar("bright", brightness);
  }
  if (server.hasArg("flip")) {
    flipped = server.arg("flip").toInt() != 0;
    u8g2.setFlipMode(flipped);
    prefs.putBool("flip", flipped);
  }
  if (server.hasArg("clock")) {
    clockOn = server.arg("clock").toInt() != 0;
    prefs.putBool("clk", clockOn);
    lastClockEnd = millis();
  }
  if (server.hasArg("clockEvery")) {
    clockEvery = constrain((int)server.arg("clockEvery").toInt(), 5, 3600);
    prefs.putUShort("clkEv", clockEvery);
  }
  if (server.hasArg("clockFor")) {
    clockFor = constrain((int)server.arg("clockFor").toInt(), 1, 60);
    prefs.putUChar("clkFor", clockFor);
  }
  if (server.hasArg("buddy")) {
    buddyOn = server.arg("buddy").toInt() != 0;
    prefs.putBool("buddy", buddyOn);
  }
  if (server.hasArg("buddyEvery")) {
    buddyEvery = constrain((int)server.arg("buddyEvery").toInt(), 1, 240);
    prefs.putUChar("budEv", buddyEvery);
  }
  if (server.hasArg("owner")) {
    String o = sanitize(server.arg("owner"), 16);
    o.replace("\n", " ");
    if (o.length()) { owner = o; prefs.putString("owner", o); }
  }
  if (server.hasArg("night")) {
    nightOn = server.arg("night").toInt() != 0;
    prefs.putBool("night", nightOn);
  }
  if (server.hasArg("nightFrom")) {
    int m = parseHHMM(server.arg("nightFrom"));
    if (m >= 0) { nightFrom = m; prefs.putUShort("nFrom", nightFrom); }
  }
  if (server.hasArg("nightTo")) {
    int m = parseHHMM(server.arg("nightTo"));
    if (m >= 0) { nightTo = m; prefs.putUShort("nTo", nightTo); }
  }
  if (server.hasArg("nightMsgs")) {
    nightMsgs = server.arg("nightMsgs").toInt() != 0;
    prefs.putBool("nMsg", nightMsgs);
  }
  if (server.hasArg("weather")) {
    weatherOn = server.arg("weather").toInt() != 0;
    prefs.putBool("wOn", weatherOn);
    lastWeatherEnd = millis();
  }
  if (server.hasArg("weatherEvery")) {
    weatherEvery = constrain((int)server.arg("weatherEvery").toInt(), 5, 3600);
    prefs.putUShort("wEv", weatherEvery);
  }
  if (server.hasArg("weatherFor")) {
    weatherFor = constrain((int)server.arg("weatherFor").toInt(), 1, 60);
    prefs.putUChar("wFor", weatherFor);
  }
  if (server.hasArg("weatherReact")) {
    weatherReact = server.arg("weatherReact").toInt() != 0;
    prefs.putBool("wReact", weatherReact);
  }
  if (server.hasArg("lat") && server.hasArg("lon")) {
    float la = atof(server.arg("lat").c_str()), lo = atof(server.arg("lon").c_str());
    if (la >= -90 && la <= 90 && lo >= -180 && lo <= 180 && (la != 0 || lo != 0)) {
      wLat = la; wLon = lo;
      prefs.putFloat("lat", wLat); prefs.putFloat("lon", wLon);
      String c = sanitize(server.arg("city"), 30);
      c.replace("\n", " ");
      wCity = c.length() ? c : String("?");
      prefs.putString("city", wCity);
      wx.ok = false;
      nextWeatherFetch = 0;            // descarga ya con la nueva ubicacion
    }
  }
  if (weatherOn || weatherReact) { if (!wx.ok) nextWeatherFetch = 0; }
  wakeUntil = 0;
  scheduleBuddy(millis());
  sendJson("{\"ok\":true}");
}

void handleWeatherTest() {
  if (!wx.ok) { sendJson("{\"ok\":false,\"error\":\"todavia no hay datos del clima\"}", 400); return; }
  if (mode == MODE_FACE || mode == MODE_CLOCK || mode == MODE_INFO || mode == MODE_BUDDY) {
    WMood m = weatherMood(wx);
    if (server.arg("show") == "1" || m == WM_NONE) { lastWeatherEnd = millis(); setMode(MODE_WEATHER); }
    else showBuddy(moodExpr(m), fillPhrase(moodPhrase(m)));
  }
  sendJson("{\"ok\":true}");
}

void handleBuddyTest() {
  if (mode == MODE_FACE || mode == MODE_CLOCK || mode == MODE_INFO) startBuddy(millis());
  sendJson("{\"ok\":true}");
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  std::vector<String> seen;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (auto& x : seen) if (x == s) dup = true;
    if (dup) continue;
    seen.push_back(s);
    if (j.length() > 1) j += ',';
    j += "{\"ssid\":\"" + jsonEsc(s) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"lock\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  j += "]";
  WiFi.scanDelete();
  sendJson(j);
}

void handleWifi() {
  String s = server.arg("ssid");
  if (!s.length()) { sendJson("{\"ok\":false}", 400); return; }
  prefs.putString("ssid", s);
  prefs.putString("pass", server.arg("pass"));
  sendJson("{\"ok\":true}");
  delay(600);
  ESP.restart();
}

void handleForget() {
  sendJson("{\"ok\":true}");
  delay(300);
  forgetWifi();
}

void handleClear() {
  msgs.clear();
  if (mode == MODE_MSG || mode == MODE_MSG_INTRO) setMode(MODE_FACE);
  sendJson("{\"ok\":true}");
}

void handleNotFound() {
  if (apMode && !online) {              // portal cautivo
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "404");
  }
}

// ---------------------------------------------------------------- OTA
void drawOta(const char* title, int pct, const char* sub) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tf);
  drawCentered(title, 18);
  if (pct >= 0) {
    u8g2.drawRFrame(10, 28, 108, 12, 3);
    u8g2.drawRBox(12, 30, max(4, pct * 104 / 100), 8, 2);
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered(String(pct) + "%", 52);
  }
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCentered(sub, 63);
  u8g2.sendBuffer();
}

void handleOtaPage() {
  if (!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication();
  server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
}

void handleOtaDone() {
  if (!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication();
  bool ok = otaRunning && !Update.hasError();
  if (!ok) {
    String err = otaRunning ? String(Update.errorString()) : String("no se recibio el archivo");
    sendJson("{\"ok\":false,\"error\":\"" + jsonEsc(err) + "\"}", 500);
    drawOta("Error OTA", -1, err.c_str());
    delay(2500);
    otaRunning = false;
    return;
  }
  sendJson("{\"ok\":true}");
  drawOta("Actualizado!", 100, "Reiniciando...");
  delay(1200);
  ESP.restart();
}

void handleOtaUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaRunning = false;
    if (!server.authenticate(OTA_USER, OTA_PASS)) return;
    otaRunning = true;
    otaDone = 0;
    otaTotal = server.clientContentLength();
    otaShown = -1;
    drawOta("Actualizando", 0, "No lo desenchufes");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (!otaRunning) {
    return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    otaDone += up.currentSize;
    int pct = otaTotal ? (int)(otaDone * 100 / otaTotal) : 0;
    if (pct > 99) pct = 99;
    if (pct >= otaShown + 4) { otaShown = pct; drawOta("Actualizando", pct, "No lo desenchufes"); }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaRunning = false;
  }
}

void setupServer() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");   // que el navegador no guarde la pagina vieja
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/expr", HTTP_ANY, handleExpr);
  server.on("/api/auto", HTTP_ANY, handleAuto);
  server.on("/api/buddy", HTTP_ANY, handleBuddyTest);
  server.on("/api/weather", HTTP_ANY, handleWeatherTest);
  server.on("/api/msg", HTTP_ANY, handleMsg);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/forget", HTTP_POST, handleForget);
  server.on("/api/clear", HTTP_POST, handleClear);
  server.on("/ota", HTTP_GET, handleOtaPage);
  server.on("/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleNotFound);
  server.begin();
}

// ============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT);
  randomSeed(esp_random());
  int highs = 0;                       // nivel del sensor en reposo (sirve para TTP223 activo alto o bajo)
  for (int k = 0; k < 20; k++) { highs += digitalRead(PIN_TOUCH); delay(2); }
  touchIdle = highs > 10 ? HIGH : LOW;
  if (esp_reset_reason() == ESP_RST_BROWNOUT) resetReason = "!Volt";

  prefs.begin("deskbuddy", false);
  devName    = prefs.getString("name", "Desk Buddy");
  brightness = prefs.getUChar("bright", 160);
  flipped    = prefs.getBool("flip", false);
  savedExpr  = prefs.getUChar("expr", 0);
  autoMode   = prefs.getBool("auto", true);
  clockOn    = prefs.getBool("clk", false);
  clockEvery = prefs.getUShort("clkEv", 30);
  clockFor   = prefs.getUChar("clkFor", 5);
  buddyOn    = prefs.getBool("buddy", false);
  buddyEvery = prefs.getUChar("budEv", 20);
  owner      = prefs.getString("owner", "Kharlita");
  nightOn    = prefs.getBool("night", false);
  nightFrom  = prefs.getUShort("nFrom", 23 * 60);
  nightTo    = prefs.getUShort("nTo", 7 * 60);
  nightMsgs  = prefs.getBool("nMsg", false);
  weatherOn    = prefs.getBool("wOn", false);
  weatherEvery = prefs.getUShort("wEv", 60);
  weatherFor   = prefs.getUChar("wFor", 5);
  weatherReact = prefs.getBool("wReact", true);
  wLat  = prefs.getFloat("lat", wLat);
  wLon  = prefs.getFloat("lon", wLon);
  wCity = prefs.getString("city", wCity.c_str());
  if (savedExpr >= EXPR_COUNT) savedExpr = 0;
  staSsid = prefs.getString("ssid", "");
  staPass = prefs.getString("pass", "");

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s-%02X%02X", AP_PREFIX, mac[4], mac[5]);
  apSsid = buf;

  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setContrast(brightness);
  u8g2.setFlipMode(flipped);

  face.begin(&u8g2, exprIndexById("sleeping"));
  face.set(savedExpr);                // se "despierta" con la expresion guardada
  startWiFi();
  setupServer();
  scheduleBuddy(millis());
  Serial.printf("Desk Buddy v" FW_VERSION " listo (touch reposo=%d)\n", touchIdle);
}

void nightTick(uint32_t now) {
  bool showingMsg = mode == MODE_MSG || mode == MODE_MSG_INTRO;
  bool off = isNightTime() && !showingMsg && since(wakeUntil, now) > 0;
  if (off == screenOff) return;
  screenOff = off;
  u8g2.setPowerSave(off);
  if (off) {
    if (mode != MODE_FACE) setMode(MODE_FACE);
  } else if (firstUnread() >= 0 && !showingMsg) {
    startIntro();                      // se prende con mensajes pendientes: los muestra
  }
}

void periodicScreens(uint32_t now) {
  if (mode == MODE_CLOCK && since(modeAt, now) > clockFor * 1000UL) {
    lastClockEnd = now;
    setMode(MODE_FACE);
  }
  if (mode == MODE_WEATHER && since(modeAt, now) > weatherFor * 1000UL) {
    lastWeatherEnd = now;
    if (clockOn && since(lastClockEnd, now) + 3000 > clockEvery * 1000UL) lastClockEnd = now;  // que no se pisen
    setMode(MODE_FACE);
  }
  if (mode != MODE_FACE || reactionUntil || screenOff) return;
  if (buddyOn && nextBuddyAt && since(nextBuddyAt, now) > 0) {
    scheduleBuddy(now);
    startBuddy(now);
    return;
  }
  if (clockOn && timeValid() && since(lastClockEnd, now) > clockEvery * 1000UL) {
    setMode(MODE_CLOCK);
    if (weatherOn && since(lastWeatherEnd, now) + 3000 > weatherEvery * 1000UL) lastWeatherEnd = now + clockFor * 1000UL;
    return;
  }
  if (weatherOn && wx.ok && since(lastWeatherEnd, now) > weatherEvery * 1000UL) setMode(MODE_WEATHER);
}

void loop() {
  if (apMode) dns.processNextRequest();
  server.handleClient();
  if (otaRunning) { delay(1); return; }   // durante la OTA no se toca nada mas
  uint32_t now = millis();                // despues de atender la web (ver since())
  handleTouch(now);
  wifiLoop(now);
  autoTick(now);
  nightTick(now);
  updateWeather(now);
  weatherAlert(now);
  periodicScreens(now);

  if (reactionUntil && since(reactionUntil, now) > 0) { reactionUntil = 0; face.set(savedExpr); }
  if (mode == MODE_INFO && since(modeAt, now) > 7000) setMode(MODE_FACE);
  if (mode == MODE_SETUP && since(modeAt, now) > 45000) setMode(MODE_FACE);

  static uint32_t lastFrame = 0;
  now = millis();
  if (!otaRunning && !screenOff && now - lastFrame >= 33) {   // ~30 fps
    lastFrame = now;
    renderFrame(now);
  }
  delay(1);
}
