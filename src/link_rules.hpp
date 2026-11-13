// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include <cstdint>
namespace link_rules {
// Keep advertising even after one-sided discovery; otherwise the second
// watch may never learn about the first watch if a single HELLO was lost.
inline bool helloDue(uint32_t now,uint32_t lastHello){
  return static_cast<uint32_t>(now-lastHello)>=650u;
}
// A trusted STATE/INPUT packet from the correct MAC/session is enough to
// complete a missing half of the HELLO handshake. HELLO itself has session 0.
inline bool bootstrapFromData(bool alreadyKnown,uint8_t msgType,
                              uint16_t remoteSession,uint16_t expectedSession,
                              uint8_t inputType,uint8_t stateType){
  return !alreadyKnown && remoteSession==expectedSession && expectedSession!=0 &&
         (msgType==inputType || msgType==stateType);
}
}
