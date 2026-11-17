#pragma once
// ============================================================================
//  Expresiones segun la hora (modo automatico)
//  Cada fila: desde que hora rige y que caras puede elegir (al azar).
//  Mas alegre desde las 9, se va cansando y enojando hacia las 18.
// ============================================================================
#include "face.h"

struct HourMood { uint8_t from; const char* ids; };

static const HourMood SCHEDULE[] = {
  { 0, "sleeping"},
  { 7, "sleepy,bored,sleeping"},
  { 8, "neutral,sleepy,thinking,happy"},
  { 9, "happy,joy,excited,starstruck,love,wink"},        // pico de alegria
  {11, "happy,joy,cool,wink,love,excited"},
  {13, "happy,neutral,cool,mischief,wink"},
  {14, "neutral,thinking,cool,suspicious,happy"},
  {15, "neutral,thinking,suspicious,confused,bored"},
  {16, "bored,thinking,sleepy,suspicious,confused"},
  {17, "bored,sleepy,angry,dizzy,confused"},
  {18, "angry,sleepy,dead,dizzy,bored,cry"},             // pico de cansancio/enojo
  {19, "sleepy,bored,angry,neutral"},
  {20, "neutral,sleepy,bored,shy"},
  {21, "sleepy,sleeping"},
  {23, "sleeping"},
};
static const uint8_t SCHEDULE_COUNT = sizeof(SCHEDULE) / sizeof(SCHEDULE[0]);

static int scheduleSlot(int hour) {
  int slot = 0;
  for (uint8_t i = 0; i < SCHEDULE_COUNT; i++) if (SCHEDULE[i].from <= hour) slot = i;
  return slot;
}

// Elige al azar una cara de la franja (distinta de la actual si se puede)
static int pickForSlot(int slot, int current) {
  int idx[16], n = 0;
  const char* p = SCHEDULE[slot].ids;
  char id[16];
  while (*p && n < 16) {
    int k = 0;
    while (*p && *p != ',' && k < 15) id[k++] = *p++;
    id[k] = 0;
    if (*p == ',') p++;
    int e = exprIndexById(id);
    if (e >= 0) idx[n++] = e;
  }
  if (n == 0) return current;
  int choice = idx[random(n)];
  for (int tries = 0; tries < 6 && n > 1 && choice == current; tries++) choice = idx[random(n)];
  return choice;
}
