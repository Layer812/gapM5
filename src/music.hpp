// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
#include <cstdint>
namespace music {
class Player {
public:
  void begin();
  void update(uint32_t ms,const gm::State& s);
  void event(uint8_t eventType);
  bool playing() const{return available;}
private:
  bool available=false;
  uint32_t nextBeat=0;
  int step=0;
  uint8_t lastPhase=gm::WAIT;
};
}
