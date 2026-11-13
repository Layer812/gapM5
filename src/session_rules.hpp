// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
#include <cstdint>
namespace session {
// All screens use BtnB (right) as the START key during WAIT/DEMO/OVER.
// In a pair, HOST starts the authoritative State. GUEST transmits a request.
enum Action : uint8_t { NONE=0, START_SOLO=1, START_COOP=2, SEND_START=3 };
inline bool startable(uint8_t phase){
  return phase==gm::WAIT || phase==gm::DEMO || phase==gm::OVER;
}
inline Action onRight(bool press,bool partnerConnected,bool host,uint8_t phase){
  if(!press||!startable(phase))return NONE;
  if(!partnerConnected)return START_SOLO;
  return host?START_COOP:SEND_START;
}
inline bool shouldAutoDemo(uint8_t phase,uint32_t since,uint32_t now,bool authority){
  return phase==gm::WAIT && authority && static_cast<uint32_t>(now-since)>=10000u;
}
}
