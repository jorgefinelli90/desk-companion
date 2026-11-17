#pragma once
// ============================================================================
//  Modo companero: cada tanto hace una cara y le dice algo a su duena.
//  "{n}" se reemplaza por el nombre (por defecto Kharlita, se cambia en la web).
//  Cada frase: desde/hasta que hora (24 h) puede salir, cara, texto.
// ============================================================================

struct Phrase { uint8_t from, to; const char* expr; const char* text; };

static const Phrase PHRASES[] = {
  // cualquier hora del dia
  { 8, 22, "joy",        "¡Hola {n}!!"},
  { 8, 22, "love",       "¡Te quiero, {n}!"},
  { 8, 22, "happy",      "¿Cómo va, {n}?"},
  { 8, 22, "starstruck", "{n}, ¡sos la mejor!"},
  { 8, 22, "excited",    "¡Vos podés, {n}!"},
  { 8, 22, "wink",       "Acá estoy, {n} ;)"},
  { 8, 22, "love",       "¿Me das un mimo, {n}?"},
  { 8, 22, "happy",      "¿Tomaste agua, {n}?"},
  { 8, 22, "thinking",   "¿En qué andás, {n}?"},
  { 8, 22, "shy",        "{n}... ¿me extrañaste?"},
  { 8, 22, "bored",      "Me aburro... ¡haceme caso, {n}!"},
  { 9, 20, "neutral",    "Estirate un poquito, {n}"},
  { 9, 20, "cool",       "{n}, ¡sos una genia!"},
  { 9, 20, "happy",      "¿Un mate, {n}?"},
  // manana
  { 7, 11, "happy",      "¡Buen día, {n}!"},
  { 7, 10, "sleepy",     "¿Ya desayunaste, {n}?"},
  { 9, 12, "excited",    "¡Qué lindo día, {n}!"},
  // mediodia
  {12, 14, "joy",        "¡A almorzar, {n}!"},
  // tarde
  {15, 18, "neutral",    "Un recreo, {n}"},
  {16, 19, "sleepy",     "¡Ya falta poco, {n}!"},
  {17, 19, "dizzy",      "Uff, qué día... ¿no, {n}?"},
  // noche
  {19, 22, "happy",      "¿Qué tal el día, {n}?"},
  {21, 23, "sleepy",     "Buenas noches, {n}"},
  {21, 23, "love",       "A descansar, {n}"},
};
static const uint8_t PHRASE_COUNT = sizeof(PHRASES) / sizeof(PHRASES[0]);

// Elige una frase valida para la hora (o cualquiera de dia si no hay hora)
static int pickPhrase(int hour, int last) {
  int ok[PHRASE_COUNT], n = 0;
  int h = hour < 0 ? 12 : hour;
  for (uint8_t i = 0; i < PHRASE_COUNT; i++)
    if (h >= PHRASES[i].from && h < PHRASES[i].to && i != last) ok[n++] = i;
  if (n == 0) return -1;
  return ok[random(n)];
}
