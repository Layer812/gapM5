// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include <cstdint>

namespace gm {
constexpr float TAU = 6.2831853071795864769f;
constexpr float RING = 201.0f;
constexpr float DEMO_SPEED = 1.40f; // R8: faster attract loop, gameplay unchanged
constexpr int MAX_ENEMY = 16;
constexpr int MAX_SHOTS = 20;
constexpr uint8_t WAIT = 0, COUNTDOWN = 1, PLAY = 2, OVER = 3,
                  DEMO = 4, CLEAR = 5;
constexpr uint8_t E_NONE = 0, E_DELAY = 1, E_OUT = 2, E_ORBIT = 3,
                  E_RETURN = 4, E_CAPTOR = 5, E_WARNING = 6;
constexpr uint8_t P_ALIVE = 0, P_CAPTURED = 1, P_DEAD = 2;

struct Player {
  float angle = 0;
  float invuln = 0;
  float power = 0;
  float fireCooldown = 0;
  uint32_t score = 0;
  uint8_t lives = 3;
  uint8_t status = P_ALIVE;
};
struct Enemy {
  float angle = 0, radius = 0, speed = 0, timer = 0, beamCharge = 0;
  // Host-authoritative quadratic arc parameters. Only rendered polar position
  // is sent on the wire; clients never re-run the trajectory simulation.
  float pathBase = 0, curve = 0;
  uint8_t type = 0, mode = E_NONE, loops = 0, hp = 1, target = 255;
};
struct Shot {
  float angle = 0, radius = 0;
  uint8_t owner = 0;
  bool active = false;
};
struct State {
  Player players[2];
  Enemy enemies[MAX_ENEMY];
  Shot shots[MAX_SHOTS];
  uint32_t random = 0x12345678u;
  uint32_t frame = 0;
  uint8_t phase = WAIT, wave = 1, event = 0, eventType = 0;
  uint8_t playersActive = 2;
  uint8_t waveQuota = 0, destroyed = 0, escaped = 0;
  float countdown = 0, waveDelay = 0, banner = 0, goFlash = 0;
  float demoTime = 0;
  bool paused = false;
};
float wrap(float rad);
float angularDistance(float a, float b);
float demoTrailBearing(float angle, float radius, int slot, float olderRadius);
void start(State& s, bool solo, uint32_t seed);
void beginDemo(State& s, bool solo, uint32_t seed);
void tick(State& s, float dt, int8_t dirP1, int8_t dirP2);
void forceCapture(State& s, int playerIndex, int enemyIndex);
void destroyEnemy(State& s, int enemyIndex, int rescuerIndex);
}
