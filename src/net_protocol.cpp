// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#include "net_protocol.hpp"
#include <cmath>
#include <cstring>
namespace proto {
uint16_t encodeAngle(float a) {return static_cast<uint16_t>(gm::wrap(a)*65535.0f/gm::TAU+.5f);}
float decodeAngle(uint16_t a) {return a*(gm::TAU/65535.0f);}
void pack(const gm::State& s,Packet& m) {
  m.magic=MAGIC;m.version=VER;m.type=STATE;m.frame=s.frame;
  m.phase=s.phase;m.wave=s.wave;m.event=s.event;m.eventType=s.eventType;m.playersActive=s.playersActive;m.banner10=static_cast<uint8_t>(s.banner>25.5f?255:s.banner*10);m.goFlash10=static_cast<uint8_t>(s.goFlash*10);
  m.countdown10=static_cast<uint16_t>(s.countdown*10);
  m.demoTime10=static_cast<uint16_t>(s.demoTime*10);
  m.waveQuota=s.waveQuota;m.destroyed=s.destroyed;m.escaped=s.escaped;
  for(int i=0;i<2;++i){
    const auto& p=s.players[i];
    m.lives[i]=p.lives; m.status[i]=p.status;
    m.position[i]=encodeAngle(p.angle);m.score[i]=p.score;
  }
  for(int i=0;i<gm::MAX_ENEMY;++i){
    const auto& e=s.enemies[i];auto& o=m.enemies[i];
    o.angle=encodeAngle(e.angle);o.radius=static_cast<uint8_t>(e.radius<0?0:(e.radius>255?255:e.radius));
    o.type=e.type;o.mode=e.mode;o.hp=e.hp;o.target=e.target;
  }
  for(int i=0;i<gm::MAX_SHOTS;++i){
    const auto& b=s.shots[i];auto& o=m.shots[i];
    o.angle=encodeAngle(b.angle);o.radius=b.active?static_cast<uint8_t>(b.radius>255?255:b.radius):0;
    o.owner=b.owner;
  }
}
void unpack(const Packet& m,gm::State& s) {
  s.frame=m.frame; s.phase=m.phase;s.wave=m.wave;s.event=m.event;s.eventType=m.eventType;s.banner=m.banner10*.1f;s.goFlash=m.goFlash10*.1f;
  s.playersActive=m.playersActive;s.countdown=m.countdown10*.1f;
  s.demoTime=m.demoTime10*.1f;
  s.waveQuota=m.waveQuota;s.destroyed=m.destroyed;s.escaped=m.escaped;
  for(int i=0;i<2;++i){
    auto& p=s.players[i];p.lives=m.lives[i];p.status=m.status[i];
    p.angle=decodeAngle(m.position[i]);p.score=m.score[i];
  }
  for(int i=0;i<gm::MAX_ENEMY;++i){
    auto& e=s.enemies[i];const auto& o=m.enemies[i];
    e.angle=decodeAngle(o.angle);e.radius=o.radius;
    e.type=o.type;e.mode=o.mode;e.hp=o.hp;e.target=o.target;
  }
  for(int i=0;i<gm::MAX_SHOTS;++i){
    auto& b=s.shots[i];const auto& o=m.shots[i];
    b.angle=decodeAngle(o.angle);b.radius=o.radius;b.active=(o.radius!=0);b.owner=o.owner;
  }
}
}
