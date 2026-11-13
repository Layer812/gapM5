// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#include "net_link.hpp"
#include "link_rules.hpp"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <cstring>

namespace net {
namespace {
Link* singleton=nullptr;
const uint8_t broadcast[6]{0xff,0xff,0xff,0xff,0xff,0xff};
uint16_t macSession(const uint8_t a[6],const uint8_t b[6]){
  uint16_t h=0x4a31;
  for(int i=0;i<6;++i)h=static_cast<uint16_t>((h*37u)^a[i]);
  for(int i=0;i<6;++i)h=static_cast<uint16_t>((h*37u)^b[i]);
  return h?h:1;
}
bool addPeer(const uint8_t mac[6]) {
  if(esp_now_is_peer_exist(mac))return true;
  esp_now_peer_info_t info{};
  memcpy(info.peer_addr,mac,6);
  info.channel=1;info.ifidx=WIFI_IF_STA;info.encrypt=false;
  return esp_now_add_peer(&info)==ESP_OK;
}
}
void recvCallback(const uint8_t* mac,const uint8_t* bytes,int len) {
  Link* p=singleton;
  if(!p||!p->inputQueue()||len!=int(sizeof(proto::Packet)))return;
  Received r{};
  memcpy(r.mac,mac,6);memcpy(&r.msg,bytes,sizeof(r.msg));
  if(r.msg.magic!=proto::MAGIC||r.msg.version!=proto::VER)return;
  if(r.msg.type!=proto::HELLO&&r.msg.type!=proto::INPUT_CMD&&r.msg.type!=proto::STATE)return;
  xQueueSend(p->inputQueue(),&r,0);
}
bool Link::begin() {
  rx=xQueueCreate(12,sizeof(Received));
  if(!rx)return false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false,false);
  WiFi.setSleep(false);
  if(esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE)!=ESP_OK)return false;
  if(esp_wifi_get_mac(WIFI_IF_STA,self)!=ESP_OK)return false;
  if(esp_now_init()!=ESP_OK)return false;
  singleton=this;
  if(!addPeer(broadcast))return false;
  if(esp_now_register_recv_cb(recvCallback)!=ESP_OK)return false;
  Serial.printf("[GapM5 R7] ESP-NOW ch1 %02x:%02x:%02x:%02x:%02x:%02x\n",self[0],self[1],self[2],self[3],self[4],self[5]);
  return ok=true;
}
void Link::associate(const uint8_t mac[6],uint32_t now,gm::State& game) {
  if(memcmp(mac,self,6)==0||solo||peerKnown)return;
  if(!addPeer(mac))return;
  memcpy(peer,mac,6);
  peerKnown=true;host=memcmp(self,peer,6)<0;
  const uint8_t* a=host?self:peer;
  const uint8_t* b=host?peer:self;
  session=macSession(a,b);
  lastSeen=now;lastInput=now;lastRemoteSeq=0;
  remoteStart=false;guestDirection=0;
  // Linking no longer starts the game. Both sides remain in WAIT, or in a
  // two-ship demo if the ten-second demo timer has already expired.
  if(host&&game.phase==gm::DEMO)gm::beginDemo(game,false,now^0x473415u);
  Serial.printf("[GapM5 R7] peer linked role=%s session=%04x; PRESS B START\n",host?"HOST 1P":"GUEST 2P",session);
}
void Link::startSolo(gm::State& game,uint32_t now) {
  solo=true;host=true;peerKnown=false;
  remoteStart=false;guestDirection=0;
  // Ignore future HELLO after SOLO is selected; no station configuration
  // changes are needed and the active app remains intact.
  gm::start(game,true,now^0x551abb32u);
  Serial.println("[GapM5 R7] SOLO START / right button");
}
void Link::send(uint8_t type,const uint8_t dest[6],const gm::State& game,int8_t direction) {
  proto::Packet p{};
  p.magic=proto::MAGIC;p.version=proto::VER;p.type=type;
  p.seq=++seq;p.session=session;p.direction=direction;
  if(type==proto::STATE)proto::pack(game,p);
  esp_now_send(dest,reinterpret_cast<const uint8_t*>(&p),sizeof(p));
}
void Link::poll(uint32_t now,gm::State& game,int8_t direction,bool startHeld) {
  if(!ok||solo)return;
  Received in{};
  while(xQueueReceive(rx,&in,0)==pdTRUE){
    if(in.msg.type==proto::HELLO){
      associate(in.mac,now,game);
      if(peerKnown&&memcmp(in.mac,peer,6)==0)lastSeen=now;
      continue;
    }
    if(!peerKnown && memcmp(in.mac,self,6)!=0) {
      // Recover if HELLO reached only the sender: its first authenticated-by-
      // session STATE/INPUT can bootstrap the missing peer. No new protocol.
      const bool smaller=memcmp(self,in.mac,6)<0;
      const uint16_t expected=smaller?macSession(self,in.mac):macSession(in.mac,self);
      if(link_rules::bootstrapFromData(peerKnown,in.msg.type,in.msg.session,
                                      expected,proto::INPUT_CMD,proto::STATE))
        associate(in.mac,now,game);
    }
    if(!peerKnown||memcmp(in.mac,peer,6)!=0||in.msg.session!=session)continue;
    lastSeen=now;
    if(in.msg.type==proto::INPUT_CMD&&host){
      if(in.msg.direction==2){
        remoteStart=true;
        guestDirection=0;
      } else {
        guestDirection=(in.msg.direction>=-1&&in.msg.direction<=1)?in.msg.direction:0;
      }
      lastInput=now;
    } else if(in.msg.type==proto::STATE&&!host){
      if(lastRemoteSeq==0||static_cast<int16_t>(in.msg.seq-lastRemoteSeq)>0){
        proto::unpack(in.msg,game);
        lastRemoteSeq=in.msg.seq;
      }
    }
  }
  // Keep discovery alive after linking: a missed first HELLO used to
  // leave the opposite watch stuck in SEARCH while this watch said LINKED.
  if(link_rules::helloDue(now,lastHello)){
    lastHello=now;send(proto::HELLO,broadcast,game,0);
  }
  if(!peerKnown)return;
  if(host){
    if(now-lastStateTx>=50){lastStateTx=now;send(proto::STATE,peer,game,0);}
  } else if(now-lastInputTx>=32) {
    lastInputTx=now;
    send(proto::INPUT_CMD,peer,game,startHeld?2:direction);
  }
}
}
