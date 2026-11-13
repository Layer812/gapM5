// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
#include "net_protocol.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace net {
struct Received {
  uint8_t mac[6];
  proto::Packet msg;
};
class Link {
public:
  bool begin();
  void poll(uint32_t now, gm::State& game, int8_t direction, bool startHeld);
  void startSolo(gm::State& game, uint32_t now);
  bool online(uint32_t now) const {return peerKnown && now-lastSeen<1800;}
  bool isHost() const {return host;}
  bool hasPartner() const {return peerKnown;}
  bool isSolo() const {return solo;}
  bool ready() const {return ok;}
  const char* identity() const {return solo?"1P SOLO":(host?"1P HOST":"2P GUEST");}
  int8_t remoteDirection(uint32_t now) const {return now-lastInput<350?guestDirection:0;}
  QueueHandle_t inputQueue() const {return rx;}
  uint16_t getSession() const {return session;}
  bool consumeRemoteStart() {bool r=remoteStart;remoteStart=false;return r;}
private:
  bool ok=false,peerKnown=false,host=true,solo=false;
  uint8_t self[6]{},peer[6]{};
  QueueHandle_t rx=nullptr;
  uint16_t seq=0,session=0,lastRemoteSeq=0;
  uint32_t lastSeen=0,lastHello=0,lastStateTx=0,lastInputTx=0,lastInput=0;
    int8_t guestDirection=0;
  bool remoteStart=false;
  void associate(const uint8_t mac[6],uint32_t now,gm::State& game);
  void send(uint8_t type,const uint8_t dest[6],const gm::State& game,int8_t direction);
  friend void recvCallback(const uint8_t*,const uint8_t*,int);
};
}
