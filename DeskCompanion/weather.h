#pragma once
// ============================================================================
//  Clima (Open-Meteo, sin cuenta ni API key)
//  - descarga temperatura actual, codigo de clima, prob. de lluvia proximas
//    horas y min/max del dia
//  - dibuja iconos (sol, luna, nubes, lluvia, tormenta, nieve, niebla)
//  - calcula el "humor" del clima para que el buddy reaccione
// ============================================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>

struct WeatherData {
  bool ok = false;
  float temp = 0, tmin = 0, tmax = 0;
  int code = -1;
  bool day = true;
  int rainProb = 0;        // maxima prob. de lluvia en las proximas horas (%)
  uint32_t at = 0;         // millis de la ultima actualizacion
  String error;
};

enum WCat { W_CLEAR, W_PCLOUD, W_CLOUD, W_FOG, W_DRIZZLE, W_RAIN, W_SNOW, W_STORM };

static WCat weatherCat(int c) {
  if (c <= 1) return W_CLEAR;
  if (c == 2) return W_PCLOUD;
  if (c == 3) return W_CLOUD;
  if (c == 45 || c == 48) return W_FOG;
  if (c >= 51 && c <= 57) return W_DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return W_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return W_SNOW;
  if (c >= 95) return W_STORM;
  return W_CLOUD;
}

static const char* weatherDesc(int c) {
  switch (weatherCat(c)) {
    case W_CLEAR:   return "Despejado";
    case W_PCLOUD:  return "Algo nublado";
    case W_CLOUD:   return "Nublado";
    case W_FOG:     return "Niebla";
    case W_DRIZZLE: return "Llovizna";
    case W_RAIN:    return "Lluvia";
    case W_SNOW:    return "Nieve";
    case W_STORM:   return "Tormenta";
  }
  return "";
}

// "Humor" del clima, de mas a menos importante
enum WMood { WM_NONE, WM_SUNNY, WM_HOT, WM_COLD, WM_RAIN_SOON, WM_RAIN, WM_STORM };

static WMood weatherMood(const WeatherData& w) {
  if (!w.ok) return WM_NONE;
  WCat c = weatherCat(w.code);
  if (c == W_STORM) return WM_STORM;
  if (c == W_RAIN || c == W_DRIZZLE || c == W_SNOW) return WM_RAIN;
  if (w.rainProb >= RAIN_PROB_ALERT) return WM_RAIN_SOON;
  if (w.temp <= COLD_TEMP) return WM_COLD;
  if (w.temp >= HOT_TEMP) return WM_HOT;
  if (c == W_CLEAR && w.day) return WM_SUNNY;
  return WM_NONE;
}

static const char* moodExpr(WMood m) {
  switch (m) {
    case WM_STORM:     return "scared";
    case WM_RAIN:      return "rainy";
    case WM_RAIN_SOON: return "rainy";
    case WM_COLD:      return "cold";
    case WM_HOT:       return "hot";
    case WM_SUNNY:     return "happy";
    default:           return nullptr;
  }
}

// Frase para el globito ({n} = nombre, {t} = temperatura, {p} = prob. de lluvia)
static const char* moodPhrase(WMood m) {
  static const char* storm[] = {"¡Tormenta! Quedate adentro, {n}", "Uy, truenos... ¡qué miedo, {n}!"};
  static const char* rain[]  = {"Está lloviendo, {n}...", "Día de lluvia, {n}. ¿Un tecito?"};
  static const char* soon[]  = {"Va a llover, {n}. ¡Llevá paraguas!", "{p}% de lluvia en un rato, {n}"};
  static const char* cold[]  = {"¡Qué frío! Abrigate, {n}", "Hace {t} afuera, {n}. ¡Brrr!"};
  static const char* hot[]   = {"¡Qué calor, {n}! Tomá agua", "Hace {t}, {n}. ¡Me derrito!"};
  static const char* sunny[] = {"¡Qué lindo día, {n}!", "Salió el sol, {n} :)"};
  int r = random(2);
  switch (m) {
    case WM_STORM:     return storm[r];
    case WM_RAIN:      return rain[r];
    case WM_RAIN_SOON: return soon[r];
    case WM_COLD:      return cold[r];
    case WM_HOT:       return hot[r];
    case WM_SUNNY:     return sunny[r];
    default:           return nullptr;
  }
}

// ---------------------------------------------------------------- parseo
// Busca "key": dentro de la seccion que empieza en "section" y devuelve el numero
static bool jsonNum(const String& s, const char* section, const char* key, float& out) {
  int a = s.indexOf(section);
  if (a < 0) return false;
  int b = s.indexOf('}', a);
  String k = String("\"") + key + "\":";
  int p = s.indexOf(k, a);
  if (p < 0 || (b >= 0 && p > b)) return false;
  out = atof(s.c_str() + p + k.length());
  return true;
}

// Devuelve el maximo de un array numerico "key":[..] dentro de "section"
static bool jsonArrMax(const String& s, const char* section, const char* key, float& out, int maxItems) {
  int a = s.indexOf(section);
  if (a < 0) return false;
  String k = String("\"") + key + "\":[";
  int p = s.indexOf(k, a);
  if (p < 0) return false;
  p += k.length();
  int e = s.indexOf(']', p);
  if (e < 0) return false;
  bool any = false;
  int n = 0;
  while (p < e && n < maxItems) {
    while (p < e && (s[p] == ' ' || s[p] == ',')) p++;
    if (p >= e) break;
    if (s.startsWith("null", p)) { p += 4; n++; continue; }
    float v = atof(s.c_str() + p);
    if (!any || v > out) out = v;
    any = true;
    n++;
    while (p < e && s[p] != ',') p++;
  }
  return any;
}

static bool parseWeather(const String& body, WeatherData& w) {
  float t, c, d, mx = 0, mn = 0, rp = 0;
  if (!jsonNum(body, "\"current\":{", "temperature_2m", t)) return false;
  if (!jsonNum(body, "\"current\":{", "weather_code", c)) return false;
  if (!jsonNum(body, "\"current\":{", "is_day", d)) d = 1;
  jsonArrMax(body, "\"hourly\":{", "precipitation_probability", rp, 4);
  jsonArrMax(body, "\"daily\":{", "temperature_2m_max", mx, 1);
  float neg = 0;
  if (jsonArrMax(body, "\"daily\":{", "temperature_2m_min", neg, 1)) mn = neg;
  w.temp = t; w.code = (int)c; w.day = d > 0.5f; w.rainProb = (int)rp;
  w.tmax = mx; w.tmin = mn;
  w.ok = true;
  return true;
}

// Lee la respuesta HTTP completa de un cliente ya conectado
static String readHttp(Client& cli, int& status) {
  String resp;
  uint32_t t0 = millis();
  while ((cli.connected() || cli.available()) && millis() - t0 < 6000) {
    while (cli.available()) { resp += (char)cli.read(); t0 = millis(); }
    delay(2);
  }
  cli.stop();
  status = 0;
  if (resp.startsWith("HTTP/")) status = resp.substring(resp.indexOf(' ') + 1).toInt();
  int b = resp.indexOf("\r\n\r\n");
  return b >= 0 ? resp.substring(b + 4) : String();
}

// Descarga el clima (bloquea ~0.5-2 s). Primero HTTP; si falla, HTTPS.
static bool fetchWeather(float lat, float lon, WeatherData& w) {
  const char* host = "api.open-meteo.com";
  char path[300];
  snprintf(path, sizeof(path),
           "/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code,is_day"
           "&hourly=precipitation_probability&forecast_hours=4"
           "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
           lat, lon);
  String req = String("GET ") + path + " HTTP/1.0\r\nHost: " + host +
               "\r\nUser-Agent: DeskBuddy\r\nConnection: close\r\n\r\n";
  int status = 0;
  String body;
  {
    WiFiClient cli;
    if (cli.connect(host, 80, 4000)) {
      cli.print(req);
      body = readHttp(cli, status);
    }
  }
  if (status != 200) {
    WiFiClientSecure sec;
    sec.setInsecure();                 // solo datos del clima publicos
    sec.setTimeout(6);
    if (sec.connect(host, 443)) {
      sec.print(req);
      body = readHttp(sec, status);
    }
  }
  if (status != 200) { w.error = status ? "HTTP " + String(status) : "sin conexion"; return false; }
  if (!parseWeather(body, w)) { w.error = "respuesta invalida"; return false; }
  w.error = "";
  w.at = millis();
  return true;
}

// ---------------------------------------------------------------- iconos
static void wiCloud(U8G2& g, int cx, int cy, int s) {
  // borde negro para que se separe del sol, despues la nube blanca
  for (int pass = 0; pass < 2; pass++) {
    int o = pass == 0 ? 2 : 0;
    g.setDrawColor(pass == 0 ? 0 : 1);
    g.drawDisc(cx - s / 2, cy, s * 45 / 100 + o);
    g.drawDisc(cx + s / 10, cy - s * 3 / 10, s * 6 / 10 + o);
    g.drawDisc(cx + s * 6 / 10, cy, s * 4 / 10 + o);
    g.drawBox(cx - s / 2 - o, cy, s * 11 / 10 + 2 * o, s * 45 / 100 + o);
  }
  g.setDrawColor(1);
}

static void wiSun(U8G2& g, int cx, int cy, int r, uint32_t now) {
  g.drawDisc(cx, cy, r / 2);
  float a0 = now * 0.0008f;
  for (int k = 0; k < 8; k++) {
    float a = a0 + k * PI / 4;
    g.drawLine(cx + cosf(a) * r * 0.7f, cy + sinf(a) * r * 0.7f, cx + cosf(a) * r, cy + sinf(a) * r);
  }
}

static void wiMoon(U8G2& g, int cx, int cy, int r) {
  g.drawDisc(cx, cy, r * 6 / 10);
  g.setDrawColor(0);
  g.drawDisc(cx + r * 35 / 100, cy - r * 2 / 10, r * 55 / 100);
  g.setDrawColor(1);
}

static void drawWeatherIcon(U8G2& g, int code, bool day, int cx, int cy, uint32_t now) {
  WCat c = weatherCat(code);
  int s = 18;
  switch (c) {
    case W_CLEAR:
      if (day) wiSun(g, cx, cy, 16, now); else wiMoon(g, cx, cy, 16);
      break;
    case W_PCLOUD:
      if (day) wiSun(g, cx - 6, cy - 6, 12, now); else wiMoon(g, cx - 6, cy - 6, 13);
      wiCloud(g, cx + 2, cy + 4, 14);
      break;
    case W_CLOUD:
      wiCloud(g, cx, cy, s);
      break;
    case W_FOG:
      for (int k = 0; k < 4; k++) {
        int o = (k % 2) * 4;
        g.drawHLine(cx - 14 + o, cy - 8 + k * 6, 24);
        g.drawHLine(cx - 14 + o, cy - 7 + k * 6, 24);
      }
      break;
    case W_DRIZZLE:
    case W_RAIN: {
      wiCloud(g, cx, cy - 6, 16);
      int off = (now / 120) % 6;
      for (int k = 0; k < 4; k++) {
        int x = cx - 10 + k * 7, y = cy + 6 + ((off + k * 2) % 6);
        if (c == W_DRIZZLE) g.drawDisc(x, y + 2, 1);
        else { g.drawLine(x, y, x - 2, y + 5); g.drawLine(x + 1, y, x - 1, y + 5); }
      }
      break;
    }
    case W_SNOW: {
      wiCloud(g, cx, cy - 6, 16);
      int off = (now / 250) % 6;
      for (int k = 0; k < 3; k++) {
        int x = cx - 8 + k * 9, y = cy + 8 + ((off + k * 2) % 6);
        g.drawHLine(x - 2, y, 5); g.drawVLine(x, y - 2, 5);
        g.drawPixel(x - 1, y - 1); g.drawPixel(x + 1, y + 1);
        g.drawPixel(x - 1, y + 1); g.drawPixel(x + 1, y - 1);
      }
      break;
    }
    case W_STORM:
      wiCloud(g, cx, cy - 6, 16);
      if ((now / 300) % 4 != 0) {
        g.drawTriangle(cx + 2, cy + 2, cx - 5, cy + 11, cx + 1, cy + 11);
        g.drawTriangle(cx + 1, cy + 9, cx - 2, cy + 18, cx + 5, cy + 9);
      }
      break;
  }
}
