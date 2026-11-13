// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
#include <cstdint>

namespace proto {
constexpr uint16_t MAGIC=0x4d47;
constexpr uint8_t VER=2, HELLO=1, INPUT_CMD=2, STATE=3;
#pragma pack(push,1)
struct EnemyWire {
  uint16_t angle;
  uint8_t radius,type,mode,hp,target;
};
struct ShotWire {uint16_t angle;uint8_t radius,owner;};
struct Packet {
  uint16_t magic=MAGIC;
  uint8_t version=VER,type=0;
  uint16_t seq=0,session=0;
  uint32_t frame=0;
  int8_t direction=0;
  uint8_t phase=0,wave=1,event=0,eventType=0,playersActive=2,banner10=0,goFlash10=0;
  uint8_t waveQuota=0, destroyed=0, escaped=0;
  uint16_t demoTime10=0;
  uint8_t lives[2]{3,3},status[2]{0,0};
  uint16_t position[2]{0,0};
  uint16_t countdown10=0;
  uint32_t score[2]{0,0};
  EnemyWire enemies[gm::MAX_ENEMY]{};
  ShotWire shots[gm::MAX_SHOTS]{};
};
#pragma pack(pop)
static_assert(sizeof(Packet)<=250,"ESP-NOW v1 payload exceeds 250 bytes");
uint16_t encodeAngle(float a);
float decodeAngle(uint16_t a);
void pack(const gm::State& s, Packet& m);
void unpack(const Packet& m, gm::State& s);
}
