// ============================================================================
//  Desk Companion - firmware propio
//  ESP32-C3 Super Mini + OLED 1.3" SH1106 + sensor tactil TTP223
//
//  - 23 expresiones animadas (se elige una y queda fija)
//  - Mensajeria local: cualquiera en tu red entra a http://deskbuddy.local
//    (o a la IP) y manda un mensaje que aparece en la pantalla
//  - Tactil: tap = reacciona / descarta mensaje
//            doble tap = siguiente expresion
//            mantener 1.5 s = info de red
//            mantener 10 s = borrar WiFi
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

#include "config.h"
#include "face.h"
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
enum Mode { MODE_FACE, MODE_MSG_INTRO, MODE_MSG, MODE_INFO, MODE_SETUP };
Mode mode = MODE_FACE;
uint32_t modeAt = 0;

String devName = "Desk Buddy";
uint8_t brightness = 160;
bool flipped = false;
uint8_t savedExpr = 0;
uint32_t reactionUntil = 0;

String toastText;
uint32_t toastUntil = 0;

bool apMode = false;
String apSsid, staSsid, staPass;
bool staRetrying = false;
uint32_t staRetryAt = 0;

bool otaRunning = false;
size_t otaDone = 0, otaTotal = 0;
int otaShown = -1;

bool touchDown = false;
uint32_t touchAt = 0;

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

// ============================================================================
//  Expresiones
// ============================================================================
void setExpression(uint8_t i, bool toast = true) {
  if (i >= EXPR_COUNT) return;
  savedExpr = i;
  reactionUntil = 0;
  face.set(i);
  prefs.putUChar("expr", i);
  if (toast) showToast(EXPRESSIONS[i].name);
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

uint32_t addMessage(const String& from, const String& text) {
  Msg m{nextMsgId++, from, text, millis(), false};
  msgs.push_back(m);
  while (msgs.size() > MSG_HISTORY) msgs.erase(msgs.begin());
  if (mode != MODE_MSG && mode != MODE_MSG_INTRO) {
    reactionUntil = 0;
    face.set(savedExpr, true);
    setMode(MODE_MSG_INTRO);
  }
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
}

void openNextMessage() {
  int i = firstUnread();
  if (i < 0) { setMode(MODE_FACE); return; }
  currentMsgId = msgs[i].id;
  prepareMessage(msgs[i]);
  setMode(MODE_MSG);
}

void dismissMessage() {
  int i = findMsg(currentMsgId);
  if (i >= 0) msgs[i].read = true;
  openNextMessage();
  if (mode == MODE_FACE) react();     // contento de haberlo leido
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
  uint32_t e = now - modeAt;
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
  if (e > 1700) openNextMessage();
}

void drawMessage(uint32_t now) {
  int i = findMsg(currentMsgId);
  if (i < 0) { openNextMessage(); return; }
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
  int y0 = top, off = 0, overflow = totalH - areaH;
  if (overflow <= 0) {
    y0 = top + (areaH - totalH) / 2;
  } else {
    const uint32_t PAUSE = 2200, PX_MS = 45;
    uint32_t scrollDur = overflow * PX_MS, cycle = PAUSE * 2 + scrollDur;
    uint32_t e = (now - msgShownAt) % cycle;
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
  drawCentered(devName, 12);
  u8g2.drawHLine(0, 16, 128);
  u8g2.setFont(u8g2_font_6x10_tf);
  if (apMode) {
    u8g2.drawUTF8(0, 28, ("Red: " + apSsid).c_str());
    u8g2.drawUTF8(0, 39, "Clave: " AP_PASSWORD);
    u8g2.drawUTF8(0, 50, ("IP: " + WiFi.softAPIP().toString()).c_str());
  } else {
    u8g2.drawUTF8(0, 28, ("WiFi: " + WiFi.SSID()).c_str());
    u8g2.drawUTF8(0, 39, ("IP: " + WiFi.localIP().toString()).c_str());
    u8g2.drawUTF8(0, 50, MDNS_NAME ".local");
  }
  u8g2.drawUTF8(0, 61, ("Mensajes: " + String(msgs.size()) + "   v" FW_VERSION).c_str());
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

void drawResetCountdown(uint32_t held) {
  u8g2.setFont(u8g2_font_helvB08_tf);
  drawCentered("Soltá para cancelar", 20);
  u8g2.setFont(u8g2_font_6x10_tf);
  int left = (10000 - (int)held) / 1000 + 1;
  if (left < 0) left = 0;
  drawCentered("Borrando WiFi en " + String(left), 40);
  u8g2.drawFrame(14, 50, 100, 8);
  u8g2.drawBox(16, 52, constrain((int)((held - 4000) * 96 / 6000), 0, 96), 4);
}

void drawToast(uint32_t now) {
  if (now >= toastUntil) return;
  u8g2.setFont(u8g2_font_6x10_tf);
  int w = textW(toastText) + 10, x = (128 - w) / 2;
  u8g2.setDrawColor(0); u8g2.drawRBox(x - 1, 49, w + 2, 15, 4);
  u8g2.setDrawColor(1); u8g2.drawRFrame(x, 50, w, 14, 4);
  u8g2.drawUTF8(x + 5, 60, toastText.c_str());
}

void renderFrame(uint32_t now) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  uint32_t held = touchDown ? now - touchAt : 0;
  if (held > 4000) {
    drawResetCountdown(held);
  } else {
    switch (mode) {
      case MODE_FACE:      face.draw(now); break;
      case MODE_MSG_INTRO: drawMsgIntro(now); break;
      case MODE_MSG:       drawMessage(now); break;
      case MODE_INFO:      drawInfo(); break;
      case MODE_SETUP:     drawSetup(); break;
    }
    if (mode == MODE_FACE) drawToast(now);
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
    case MODE_SETUP:     setMode(MODE_FACE); break;
    case MODE_FACE:      react(); break;
  }
}

void onDoubleTap() {
  if (mode != MODE_FACE) { onTap(); return; }
  setExpression((savedExpr + 1) % EXPR_COUNT);
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
  static uint8_t taps = 0;
  static uint32_t lastRelease = 0;
  bool raw = digitalRead(PIN_TOUCH) == TOUCH_ACTIVE;

  if (raw && !touchDown) { touchDown = true; touchAt = now; infoFired = false; }
  if (raw && touchDown) {
    uint32_t held = now - touchAt;
    if (held > 1500 && !infoFired && (mode == MODE_FACE || mode == MODE_SETUP)) {
      infoFired = true;
      reactionUntil = 0; face.set(savedExpr, true);
      setMode(MODE_INFO);
    }
  }
  if (!raw && touchDown) {
    touchDown = false;
    uint32_t held = now - touchAt;
    if (held >= 10000) forgetWifi();
    else if (!infoFired && held > 25 && held < 800) { taps++; lastRelease = now; }
  }
  if (taps && !touchDown && now - lastRelease > 280) {
    if (taps >= 2) onDoubleTap(); else onTap();
    taps = 0;
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

void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
  lowerTxPower();
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  startMDNS();
  setMode(MODE_SETUP);
}

void drawStatus(const String& a, const String& b) {
  u8g2.clearBuffer();
  face.draw(millis());
  u8g2.setDrawColor(0); u8g2.drawBox(0, 44, 128, 20); u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCentered(a, 53);
  drawCentered(b, 63);
  u8g2.sendBuffer();
}

void startWiFi() {
  WiFi.persistent(false);
  WiFi.setHostname(MDNS_NAME);
  if (staSsid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSsid.c_str(), staPass.c_str());
    lowerTxPower();
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT) {
      drawStatus("Conectando a", staSsid);
      delay(30);
    }
    if (WiFi.status() == WL_CONNECTED) {
      apMode = false;
      WiFi.setAutoReconnect(true);
      startMDNS();
      setMode(MODE_INFO);           // muestra la IP unos segundos
      return;
    }
    WiFi.disconnect();
  }
  startAP();
}

// En modo AP con una red guardada, reintenta el router cada tanto
// (por si se corto la luz y el router tardo mas en arrancar).
void wifiMaintenance(uint32_t now) {
  if (!apMode || !staSsid.length()) return;
  if (!staRetrying && now - staRetryAt > 90000 && WiFi.softAPgetStationNum() == 0) {
    staRetrying = true; staRetryAt = now;
    WiFi.begin(staSsid.c_str(), staPass.c_str());
  }
  if (staRetrying) {
    if (WiFi.status() == WL_CONNECTED) {
      staRetrying = false; apMode = false;
      dns.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      startMDNS();
      if (mode == MODE_SETUP) setMode(MODE_FACE);
      showToast("WiFi conectado");
    } else if (now - staRetryAt > 12000) {
      staRetrying = false; staRetryAt = now;
      WiFi.disconnect(false);
    }
  }
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
  j += ",\"ap\":" + String(apMode ? "true" : "false");
  j += ",\"ssid\":\"" + jsonEsc(apMode ? apSsid : WiFi.SSID()) + "\"";
  j += ",\"ip\":\"" + (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString() + "\"";
  j += ",\"rssi\":" + String(apMode ? 0 : WiFi.RSSI());
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
    j += "{\"id\":" + String(msgs[i].id) + ",\"from\":\"" + jsonEsc(msgs[i].from) + "\",\"text\":\"" +
         jsonEsc(msgs[i].text) + "\",\"read\":" + (msgs[i].read ? "true" : "false") +
         ",\"ago\":" + String((now - msgs[i].at) / 1000) + "}";
  }
  j += "]}";
  sendJson(j);
}

void handleExpr() {
  String id = server.arg("id");
  int i = exprIndexById(id.c_str());
  if (i < 0 && id.length() && isDigit(id[0])) i = id.toInt();
  if (i < 0 || i >= EXPR_COUNT) { sendJson("{\"ok\":false,\"error\":\"expresion desconocida\"}", 400); return; }
  if (mode == MODE_INFO || mode == MODE_SETUP) setMode(MODE_FACE);
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
  if (apMode) {                         // portal cautivo
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
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/expr", HTTP_ANY, handleExpr);
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

  prefs.begin("deskbuddy", false);
  devName    = prefs.getString("name", "Desk Buddy");
  brightness = prefs.getUChar("bright", 160);
  flipped    = prefs.getBool("flip", false);
  savedExpr  = prefs.getUChar("expr", 0);
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
  startWiFi();
  setupServer();
  face.set(savedExpr);                // se "despierta" con la expresion guardada
  Serial.printf("Desk Buddy v" FW_VERSION " listo. IP: %s\n",
                (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
}

void loop() {
  uint32_t now = millis();
  if (apMode) dns.processNextRequest();
  server.handleClient();
  if (otaRunning) { delay(1); return; }   // durante la OTA no se toca nada mas
  handleTouch(now);
  wifiMaintenance(now);

  if (reactionUntil && now > reactionUntil) { reactionUntil = 0; face.set(savedExpr); }
  if (mode == MODE_INFO && now - modeAt > 7000) setMode(MODE_FACE);
  if (mode == MODE_SETUP && now - modeAt > 45000) setMode(MODE_FACE);

  static uint32_t lastFrame = 0;
  if (!otaRunning && now - lastFrame >= 33) {        // ~30 fps
    lastFrame = now;
    renderFrame(now);
  }
  delay(1);
}
