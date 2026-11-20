// ============================================================================
//  face.h  -  Motor de expresiones procedurales para OLED 128x64 (U8g2)
//  Todas las caras se dibujan con primitivas (sin bitmaps), con transiciones
//  suaves, parpadeo automatico, mirada que deambula y efectos animados.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <math.h>
#include <string.h>

enum EyeType : uint8_t {
  EYE_NORMAL,   // ojo redondeado con parpados (la mayoria de las caras)
  EYE_HEART,    // corazon que late
  EYE_SPIRAL,   // espiral girando (mareado)
  EYE_X,        // X  (KO)
  EYE_CLOSED,   // U  cerrado relajado (durmiendo)
  EYE_HAPPY,    // ^  cerrado feliz
  EYE_CHEVRON,  // > <  apretados (llorando)
  EYE_STAR      // estrella (deslumbrado)
};

enum MouthType : uint8_t {
  M_NONE, M_SMALL, M_SMILE, M_BIG, M_O, M_FLAT, M_FROWN,
  M_WAVY, M_CAT, M_GRIN, M_SMIRK, M_TONGUE
};

enum AnimType : uint8_t { A_NONE, A_BOUNCE, A_SHAKE, A_SWAY, A_BREATHE };

enum : uint16_t {
  FX_TEAR     = 1 << 0,
  FX_TEARS    = 1 << 1,
  FX_ZZZ      = 1 << 2,
  FX_QUESTION = 1 << 3,
  FX_BLUSH    = 1 << 4,
  FX_SPARKLE  = 1 << 5,
  FX_SWEAT    = 1 << 6,
  FX_VEIN     = 1 << 7,
  FX_GLASSES  = 1 << 8,
  FX_SHINE    = 1 << 9,
  FX_HEARTS   = 1 << 10,
  FX_EXCLAIM  = 1 << 11,
  FX_DOTS     = 1 << 12,
  FX_RAIN     = 1 << 13,   // lluvia cayendo
  FX_FROST    = 1 << 14,   // copitos de nieve
};

struct Expression {
  const char* id;      // identificador para la API
  const char* name;    // nombre visible
  const char* emoji;   // para la web
  uint8_t eyeL, eyeR;  // EyeType
  uint8_t w, h, r;     // tamano del ojo y radio de esquinas
  uint8_t lidIn;       // % parpado superior lado interno (enojo)
  uint8_t lidOut;      // % parpado superior lado externo (tristeza)
  uint8_t lidBot;      // % "mejilla" inferior (ojos sonrientes)
  int8_t  lookX, lookY;// mirada base -100..100
  uint8_t sizeR;       // % escala del ojo derecho
  uint8_t mouth;       // MouthType
  uint8_t anim;        // AnimType
  uint16_t fx;         // efectos
  bool wander;         // la mirada deambula sola
};

#define N EYE_NORMAL
static const Expression EXPRESSIONS[] = {
  //  id            nombre          emoji   eyeL        eyeR        w  h  r   in out bot  lx  ly  sR  boca     anim       fx                         wander
  {"neutral",    "Normal",       "🙂",  N,          N,          28,30, 9,  0, 0, 0,   0,  0,100, M_SMALL, A_BREATHE, 0,                          true },
  {"happy",      "Feliz",        "😊",  N,          N,          30,30,10,  0, 0,45,   0,  0,100, M_SMILE, A_BREATHE, 0,                          true },
  {"joy",        "Risa",         "😆",  EYE_HAPPY,  EYE_HAPPY,  30,30,10,  0, 0, 0,   0,  0,100, M_BIG,   A_BOUNCE,  FX_BLUSH,                   false},
  {"love",       "Enamorado",    "😍",  EYE_HEART,  EYE_HEART,  30,30,10,  0, 0, 0,   0,  0,100, M_SMILE, A_BREATHE, FX_HEARTS,                  false},
  {"sad",        "Triste",       "😢",  N,          N,          28,28, 9,  0,45, 0,   0, 40,100, M_FROWN, A_BREATHE, FX_TEAR,                    false},
  {"cry",        "Llorando",     "😭",  EYE_CHEVRON,EYE_CHEVRON,30,30,10,  0, 0, 0,   0,  0,100, M_WAVY,  A_SHAKE,   FX_TEARS,                   false},
  {"angry",      "Enojado",      "😠",  N,          N,          30,26, 6, 60, 0, 0,   0,  0,100, M_FROWN, A_SHAKE,   FX_VEIN,                    false},
  {"surprised",  "Sorprendido",  "😮",  N,          N,          26,34,13,  0, 0, 0,   0,-20,100, M_O,     A_NONE,    FX_EXCLAIM,                 false},
  {"scared",     "Asustado",     "😨",  N,          N,          20,22, 9,  0,25, 0,   0,-10,100, M_WAVY,  A_SHAKE,   FX_SWEAT,                   true },
  {"sleepy",     "Con sueño",    "😪",  N,          N,          30,28, 9, 62,62, 0,   0, 30,100, M_FLAT,  A_BREATHE, FX_ZZZ,                     false},
  {"sleeping",   "Durmiendo",    "😴",  EYE_CLOSED, EYE_CLOSED, 30,30, 9,  0, 0, 0,   0,  0,100, M_O,     A_BREATHE, FX_ZZZ,                     false},
  {"wink",       "Guiño",        "😉",  N,          EYE_HAPPY,  30,30,10,  0, 0,30,   0,  0,100, M_TONGUE,A_NONE,    0,                          false},
  {"confused",   "Confundido",   "😕",  N,          N,          28,30, 9,  0,20, 0,  25,-25, 70, M_WAVY,  A_NONE,    FX_QUESTION,                false},
  {"suspicious", "Sospechoso",   "🤨",  N,          N,          30,30, 8, 48,48, 0,  75,  0,100, M_FLAT,  A_NONE,    0,                          false},
  {"thinking",   "Pensando",     "🤔",  N,          N,          24,26, 9,  0,15, 0, -55,-60,100, M_SMIRK, A_NONE,    FX_DOTS,                    false},
  {"dizzy",      "Mareado",      "😵‍💫", EYE_SPIRAL, EYE_SPIRAL, 30,30, 9,  0, 0, 0,   0,  0,100, M_WAVY,  A_SWAY,    0,                          false},
  {"excited",    "Emocionado",   "😃",  N,          N,          32,34,12,  0, 0, 0,   0,-10,100, M_BIG,   A_BOUNCE,  FX_SPARKLE | FX_SHINE,      false},
  {"starstruck", "Deslumbrado",  "🤩",  EYE_STAR,   EYE_STAR,   30,30,10,  0, 0, 0,   0,  0,100, M_BIG,   A_BOUNCE,  FX_SPARKLE,                 false},
  {"bored",      "Aburrido",     "😑",  N,          N,          30,28, 8, 50,50, 0, -60, 20,100, M_FLAT,  A_NONE,    0,                          false},
  {"cool",       "Canchero",     "😎",  N,          N,          30,30, 9,  0, 0, 0,   0,  0,100, M_SMIRK, A_NONE,    FX_GLASSES,                 false},
  {"shy",        "Tímido",       "☺️",  N,          N,          24,24,10,  0, 0,25, -35, 55,100, M_SMALL, A_NONE,    FX_BLUSH,                   false},
  {"mischief",   "Travieso",     "😏",  N,          N,          30,26, 8, 50,10,35,   0,  0,100, M_GRIN,  A_NONE,    0,                          false},
  {"dead",       "KO",           "😵",  EYE_X,      EYE_X,      30,30, 9,  0, 0, 0,   0,  0,100, M_TONGUE,A_NONE,    0,                          false},
  // caras del clima
  {"rainy",      "Lluvioso",     "🌧️", N,          N,          28,26, 9,  0,40, 0,   0, 35,100, M_FROWN, A_BREATHE, FX_RAIN,                    false},
  {"cold",       "Con frío",     "🥶",  N,          N,          26,20, 8, 30,30, 0,   0,  0,100, M_WAVY,  A_SHAKE,   FX_FROST,                   false},
  {"hot",        "Con calor",    "🥵",  N,          N,          30,24, 9, 45,45, 0,   0, 25,100, M_TONGUE,A_BREATHE, FX_SWEAT,                   false},
};
#undef N

static const uint8_t EXPR_COUNT = sizeof(EXPRESSIONS) / sizeof(EXPRESSIONS[0]);

static inline int exprIndexById(const char* id) {
  for (uint8_t i = 0; i < EXPR_COUNT; i++)
    if (strcmp(EXPRESSIONS[i].id, id) == 0) return i;
  return -1;
}

class FaceEngine {
public:
  void begin(U8G2* disp, uint8_t index) { g = disp; set(index, true); }
  uint8_t index() const { return idx; }
  const Expression& expr() const { return *e; }

  void set(uint8_t i, bool instant = false) {
    if (i >= EXPR_COUNT) i = 0;
    idx = i;
    e = &EXPRESSIONS[i];
    uint32_t now = millis();
    if (instant) {
      w = e->w; h = e->h; r = e->r;
      lidIn = e->lidIn; lidOut = e->lidOut; lidBot = e->lidBot;
      lookX = e->lookX; lookY = e->lookY; sizeR = e->sizeR;
      cy = targetCy(); eyeL = e->eyeL; eyeR = e->eyeR; transStart = 0;
    } else if (e->eyeL != eyeL || e->eyeR != eyeR) {
      transStart = now ? now : 1;   // cierra ojos -> cambia forma -> abre
      swapped = false;
    }
    tgx = tgy = 0;
    nextWander = now + 1500;
  }

  void draw(uint32_t now) { update(now); render(now); }

private:
  U8G2* g = nullptr;
  const Expression* e = &EXPRESSIONS[0];
  uint8_t idx = 0, eyeL = EYE_NORMAL, eyeR = EYE_NORMAL;
  float w = 28, h = 30, r = 9, lidIn = 0, lidOut = 0, lidBot = 0;
  float lookX = 0, lookY = 0, sizeR = 100, cy = 25;
  float squash = 1, blink = 1, gx = 0, gy = 0;
  int tgx = 0, tgy = 0;
  uint32_t transStart = 0, blinkStart = 0, nextBlink = 1500, nextWander = 0, lastT = 0;
  bool swapped = true;

  float targetCy() const { return e->mouth == M_NONE ? 32 : 25; }
  static void approach(float& v, float target, float k) { v += (target - v) * k; }
  static int R(float v) { return (int)lroundf(v); }
  static int U(float v) { int i = (int)lroundf(v); return i < 0 ? 0 : i; }

  void update(uint32_t now) {
    float dt = lastT ? (float)(now - lastT) : 33.f;
    lastT = now;
    if (dt > 120) dt = 120;
    float k = 1.f - expf(-dt / 70.f);
    approach(w, e->w, k);  approach(h, e->h, k);  approach(r, e->r, k);
    approach(lidIn, e->lidIn, k); approach(lidOut, e->lidOut, k); approach(lidBot, e->lidBot, k);
    approach(lookX, e->lookX, k); approach(lookY, e->lookY, k);
    approach(sizeR, e->sizeR, k); approach(cy, targetCy(), k);

    squash = 1;
    if (transStart) {
      float p = (now - transStart) / 240.f;
      if (p >= 1) transStart = 0;
      else {
        if (p >= 0.5f && !swapped) { eyeL = e->eyeL; eyeR = e->eyeR; swapped = true; }
        squash = fabsf(1 - 2 * p);
      }
    }
    if (!transStart) { eyeL = e->eyeL; eyeR = e->eyeR; }

    if (now >= nextBlink) {
      blinkStart = now;
      nextBlink = now + (random(100) < 18 ? 320 : random(2200, 6500));
    }
    float bp = (now - blinkStart) / 160.f;
    blink = (bp >= 0 && bp < 1) ? 1 - sinf(bp * PI) : 1;

    if (e->wander) {
      if (now >= nextWander) {
        if (random(100) < 35) { tgx = 0; tgy = 0; }
        else { tgx = random(-55, 56); tgy = random(-35, 36); }
        nextWander = now + random(1200, 4200);
      }
    } else { tgx = tgy = 0; }
    float kg = 1.f - expf(-dt / 45.f);
    gx += (tgx - gx) * kg;
    gy += (tgy - gy) * kg;
  }

  void render(uint32_t now) {
    float t = (float)now;
    float ox = 0, oy = 0;
    switch (e->anim) {
      case A_BOUNCE:  oy = -fabsf(sinf(t * 0.009f)) * 4; break;
      case A_SHAKE:   ox = sinf(t * 0.07f) * 1.2f; break;
      case A_SWAY:    ox = sinf(t * 0.0035f) * 5; oy = sinf(t * 0.007f) * 1.5f; break;
      case A_BREATHE: oy = sinf(t * 0.0022f) * 1.2f; break;
      default: break;
    }
    float lx = constrain(lookX + gx, -100.f, 100.f);
    float ly = constrain(lookY + gy, -100.f, 100.f);
    float dx = lx * 0.14f, dy = ly * 0.09f;
    float ecy = cy + dy + oy;
    float ex[2];

    g->setDrawColor(1);
    g->setFontMode(1);

    for (int i = 0; i < 2; i++) ex[i] = (i == 0 ? 36 : 92) + dx + ox;

    if (e->fx & FX_GLASSES) {
      drawGlasses(ox + dx * 0.3f, cy + oy);
    } else {
      for (int i = 0; i < 2; i++) {
        float sc = (i == 1 ? sizeR / 100.f : 1.f) * (1 + (i == 1 ? lx : -lx) * 0.0008f);
        uint8_t type = i == 0 ? eyeL : eyeR;
        float sq = squash;
        if (type == EYE_NORMAL && blink < sq) sq = blink;
        drawEye(i, type, ex[i], ecy, w * sc, h * sc, r * sc, sq, t);
      }
    }
    drawMouth(e->mouth, 64 + dx * 0.5f + ox, cy + 28 + dy * 0.35f + oy, t);
    drawFx(t, ex, ecy, ox, oy);
  }

  // ---------------------------------------------------------------- ojos
  void drawEye(int i, uint8_t type, float cx, float cy, float ew, float eh, float er, float sq, float t) {
    if (type != EYE_NORMAL && sq < 0.25f) {        // a mitad de transicion: linea
      g->drawBox(U(cx - ew / 2), U(cy - 1), U(ew), 2);
      return;
    }
    switch (type) {
      case EYE_NORMAL: {
        float hh = eh * sq; if (hh < 2) hh = 2;
        int iw = R(ew), ih = R(hh);
        int x0 = R(cx - ew / 2), y0 = R(cy - hh / 2);
        x0 = max(x0, 0);
        y0 = max(y0, 0);
        int rr = R(er), maxr = min(iw, ih) / 2 - 1;
        rr = max(0, min(rr, maxr));
        if (rr > 0) g->drawRBox(x0, y0, iw, ih, rr); else g->drawBox(x0, y0, iw, ih);
        if (sq < 0.35f) break;
        g->setDrawColor(0);
        float dIn = hh * lidIn / 100.f, dOut = hh * lidOut / 100.f;
        float dL = (i == 0) ? dOut : dIn, dR = (i == 0) ? dIn : dOut;   // lado interno = hacia la nariz
        if (dL > 0.5f || dR > 0.5f) {
          int xa = x0 - 1, xb = x0 + iw, yt = y0 - 1;
          g->drawTriangle(xa, yt, xb, yt, xb, yt + R(dR) + 1);
          g->drawTriangle(xa, yt, xb, yt + R(dR) + 1, xa, yt + R(dL) + 1);
        }
        float lb = hh * lidBot / 100.f;
        if (lb > 0.5f) {
          float ry = hh * 0.55f, rx = ew * 0.75f;
          g->drawFilledEllipse(U(cx), U(y0 + ih - lb + ry), U(rx), U(ry));
        }
        if (e->fx & FX_SHINE) {
          int sr = max(2, R(ew * 0.12f));
          g->drawDisc(U(cx - ew * 0.18f), U(cy - hh * 0.18f), sr);
          g->drawDisc(U(cx + ew * 0.16f), U(cy + hh * 0.16f), 1);
        }
        g->setDrawColor(1);
        break;
      }
      case EYE_HEART: {
        float pulse = 1 + 0.09f * sinf(t * 0.012f);
        int rr = max(2, R(ew * 0.25f * sq * pulse));
        int icx = R(cx), icy = R(cy), ty = icy - rr / 2;
        g->drawDisc(U(icx - rr), U(ty), rr);
        g->drawDisc(U(icx + rr), U(ty), rr);
        g->drawTriangle(icx - 2 * rr, ty + 1, icx + 2 * rr, ty + 1, icx, icy + R(rr * 1.9f));
        break;
      }
      case EYE_SPIRAL: {
        float maxR = min(ew, eh) * 0.5f * sq, a0 = t * 0.006f;
        float px = cx, py = cy;
        for (float th = 0.3f; th <= 6 * PI; th += 0.25f) {
          float rad = maxR * th / (6 * PI);
          float x = cx + rad * cosf(th + a0), y = cy + rad * sinf(th + a0);
          g->drawLine(U(px), U(py), U(x), U(y));
          g->drawLine(U(px + 1), U(py), U(x + 1), U(y));
          px = x; py = y;
        }
        break;
      }
      case EYE_X: {
        float s = min(ew, eh) * 0.38f * sq;
        for (int k = -1; k <= 1; k++) {
          g->drawLine(U(cx - s + k), U(cy - s), U(cx + s + k), U(cy + s));
          g->drawLine(U(cx + s + k), U(cy - s), U(cx - s + k), U(cy + s));
        }
        break;
      }
      case EYE_CLOSED: {
        int rr = R(ew * 0.42f);
        for (int k = 0; k < 3; k++)
          g->drawCircle(U(cx), U(cy - rr * 0.6f + k), rr, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        break;
      }
      case EYE_HAPPY: {
        int rr = R(ew * 0.42f);
        for (int k = 0; k < 3; k++)
          g->drawCircle(U(cx), U(cy + rr * 0.5f + k), rr, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
        break;
      }
      case EYE_CHEVRON: {
        float s = min(ew, eh) * 0.35f;
        float d = (i == 0) ? 1 : -1;               // ">" izquierda, "<" derecha
        for (int k = 0; k < 3; k++) {
          float o = k * d;
          g->drawLine(U(cx - d * s + o), U(cy - s), U(cx + d * s + o), U(cy));
          g->drawLine(U(cx + d * s + o), U(cy), U(cx - d * s + o), U(cy + s));
        }
        break;
      }
      case EYE_STAR: {
        float pulse = 1 + 0.08f * sinf(t * 0.01f + i);
        float Ro = min(ew, eh) * 0.58f * sq * pulse, Ri = Ro * 0.45f;
        float a = -PI / 2 + 0.15f * sinf(t * 0.004f);
        float px[10], py[10];
        for (int k = 0; k < 10; k++) {
          float rr = (k & 1) ? Ri : Ro, ang = a + k * PI / 5;
          px[k] = cx + rr * cosf(ang); py[k] = cy + rr * sinf(ang);
        }
        for (int k = 0; k < 10; k++) {
          int n = (k + 1) % 10;
          g->drawTriangle(R(cx), R(cy), R(px[k]), R(py[k]), R(px[n]), R(py[n]));
        }
        break;
      }
    }
  }

  void drawGlasses(float ox, float cy) {
    const int lens[2] = {36, 92};
    int y = R(cy);
    g->drawBox(U(16 + ox), U(y - 11), 96, 3);
    g->drawBox(U(55 + ox), U(y - 8), 18, 3);
    for (int i = 0; i < 2; i++) {
      int cx = R(lens[i] + ox);
      g->drawRBox(U(cx - 19), U(y - 10), 38, 19, 7);
      g->setDrawColor(0);
      g->drawLine(U(cx - 12), U(y + 4), U(cx - 4), U(y - 6));
      g->drawLine(U(cx - 11), U(y + 4), U(cx - 3), U(y - 6));
      g->drawLine(U(cx - 6), U(y + 4), U(cx - 1), U(y - 2));
      g->setDrawColor(1);
    }
  }

  // ---------------------------------------------------------------- boca
  void thickArc(int x, int y, int rad, uint8_t opt, int th) {
    for (int k = 0; k < th; k++) g->drawCircle(U(x), U(y + k), rad, opt);
  }

  void drawMouth(uint8_t m, float fx, float fy, float t) {
    int mx = R(fx), my = R(fy);
    const uint8_t ARC_LO = U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT;
    const uint8_t ARC_HI = U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT;
    switch (m) {
      case M_SMALL:  thickArc(mx, my - 4, 5, ARC_LO, 2); break;
      case M_SMILE:  thickArc(mx, my - 7, 8, ARC_LO, 2); break;
      case M_BIG:    g->drawDisc(U(mx), U(my - 5), 10, ARC_LO); break;
      case M_O:
        g->drawFilledEllipse(U(mx), U(my), 4, 5);
        g->setDrawColor(0); g->drawFilledEllipse(U(mx), U(my), 2, 3); g->setDrawColor(1);
        break;
      case M_FLAT:   g->drawBox(U(mx - 7), U(my), 14, 2); break;
      case M_FROWN:  thickArc(mx, my + 3, 7, ARC_HI, 2); break;
      case M_WAVY: {
        int px = mx - 10, py = my;
        for (int x = mx - 10; x <= mx + 10; x += 2) {
          int y = R(my + 2 * sinf((x - mx) * 0.6f + t * 0.01f));
          g->drawLine(U(px), U(py), U(x), U(y));
          g->drawLine(U(px), U(py + 1), U(x), U(y + 1));
          px = x; py = y;
        }
        break;
      }
      case M_CAT:
        thickArc(mx - 3, my - 3, 3, ARC_LO, 2);
        thickArc(mx + 3, my - 3, 3, ARC_LO, 2);
        break;
      case M_GRIN:
        g->drawFilledEllipse(U(mx), U(my - 4), 15, 8, ARC_LO);
        g->setDrawColor(0); g->drawHLine(U(mx - 13), U(my - 1), 27); g->setDrawColor(1);
        break;
      case M_SMIRK:
        for (int k = 0; k < 2; k++) g->drawLine(U(mx - 7), U(my + 1 + k), U(mx + 6), U(my - 2 + k));
        thickArc(mx + 6, my - 5, 3, U8G2_DRAW_LOWER_RIGHT, 2);
        break;
      case M_TONGUE:
        thickArc(mx, my - 7, 8, ARC_LO, 2);
        g->drawRBox(U(mx), U(my - 1), 8, 7, 3);
        g->setDrawColor(0); g->drawVLine(U(mx + 4), U(my + 2), 3); g->setDrawColor(1);
        break;
      default: break;
    }
  }

  // ---------------------------------------------------------------- efectos
  void drop(float x, float y, float s) {
    int ix = R(x), iy = R(y), is = max(1, R(s));
    if (iy > 66) return;
    g->drawDisc(U(ix), U(iy), is);
    g->drawTriangle(ix - is, iy, ix + is, iy, ix, iy - R(s * 2.6f));
  }

  void miniHeart(int x, int y, int s) {
    g->drawDisc(U(x - s), U(y), s);
    g->drawDisc(U(x + s), U(y), s);
    g->drawTriangle(x - 2 * s, y + 1, x + 2 * s, y + 1, x, y + 2 * s + 1);
  }

  void drawFx(float t, const float* ex, float ecy, float ox, float oy) {
    uint16_t fx = e->fx;
    if (fx & FX_TEAR) {
      float p = fmodf(t, 1800.f) / 1800.f;
      drop(ex[0] - w * 0.3f, ecy + h / 2 + 3 + p * 16, 2);
    }
    if (fx & FX_TEARS) {
      for (int i = 0; i < 2; i++)
        for (int k = 0; k < 2; k++) {
          float p = fmodf(t + k * 450 + i * 200, 900.f) / 900.f;
          drop(ex[i] + (i == 0 ? -9 : 9), ecy + 6 + p * 26, 2);
        }
    }
    if (fx & FX_ZZZ) {
      g->setFont(u8g2_font_6x10_tf);
      for (int k = 0; k < 3; k++) {
        float p = fmodf(t / 2400.f + k / 3.f, 1.f);
        int x = R(106 + p * 12), y = R(24 - p * 16);
        g->drawStr(U(x), U(y), p > 0.5f ? "Z" : "z");
      }
    }
    if (fx & FX_QUESTION) {
      g->setFont(u8g2_font_helvB14_tf);
      g->drawStr(U(114 + ox), U(18 + 2 * sinf(t * 0.005f) + oy), "?");
    }
    if (fx & FX_EXCLAIM) {
      g->setFont(u8g2_font_helvB14_tf);
      int j = (fmodf(t, 1200.f) < 120) ? -2 : 0;
      g->drawStr(U(117 + ox), U(17 + j + oy), "!");
    }
    if (fx & FX_DOTS) {
      for (int k = 0; k < 3; k++) {
        bool on = fmodf(t / 400.f, 4.f) > k;
        if (on) g->drawDisc(U(106 + k * 7 + ox), U(7 + oy), 2);
      }
    }
    if (fx & FX_BLUSH) {
      for (int i = 0; i < 2; i++) {
        int bx = R(ex[i] + (i == 0 ? -10 : 4)), by = R(ecy + h / 2 + 2);
        for (int k = 0; k < 3; k++) g->drawLine(U(bx + k * 4), U(by + 3), U(bx + k * 4 + 3), U(by));
      }
    }
    if (fx & FX_SPARKLE) {
      static const int8_t sp[4][2] = {{6, 8}, {121, 10}, {9, 52}, {119, 50}};
      for (int k = 0; k < 4; k++) {
        int s = R(1.5f + 2.5f * sinf(t * 0.008f + k * 1.7f));
        if (s <= 0) continue;
        g->drawHLine(U(sp[k][0] - s), U(sp[k][1]), 2 * s + 1);
        g->drawVLine(U(sp[k][0]), U(sp[k][1] - s), 2 * s + 1);
      }
    }
    if (fx & FX_SWEAT) {
      float p = fmodf(t, 2000.f) / 2000.f;
      drop(116 + ox, 8 + p * 12 + oy, 2.5f);
    }
    if (fx & FX_VEIN) {
      float s = 1 + 0.25f * sinf(t * 0.015f);
      int cx = R(114 + ox), cyy = R(10 + oy);
      for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
          int a = R(2 * s), b = R(6 * s);
          g->drawLine(U(cx + sx * a), U(cyy + sy * b), U(cx + sx * a), U(cyy + sy * a));
          g->drawLine(U(cx + sx * a), U(cyy + sy * a), U(cx + sx * b), U(cyy + sy * a));
        }
    }
    if (fx & FX_RAIN) {                  // gotitas cayendo por toda la pantalla
      for (int k = 0; k < 7; k++) {
        float p = fmodf(t / 900.f + k * 0.37f, 1.f);
        int x = (k * 19 + 7) % 128, y = R(p * 70) - 6;
        if (y < 0) continue;
        g->drawLine(U(x), U(y), U(x - 2), U(y + 5));
      }
    }
    if (fx & FX_FROST) {                 // copitos
      for (int k = 0; k < 5; k++) {
        float p = fmodf(t / 3000.f + k * 0.29f, 1.f);
        int x = R((k * 27 + 9) % 128 + 3 * sinf(t * 0.002f + k)), y = R(p * 68) - 2;
        if (y < 2 || x < 2) continue;
        g->drawHLine(U(x - 2), U(y), 5); g->drawVLine(U(x), U(y - 2), 5);
      }
    }
    if (fx & FX_HEARTS) {
      for (int k = 0; k < 2; k++) {
        float p = fmodf(t / 2600.f + k * 0.5f, 1.f);
        int x = R((k ? 121 : 7) + 2 * sinf(t * 0.004f + k));
        miniHeart(x, R(62 - p * 58), 2);
      }
    }
  }
};
