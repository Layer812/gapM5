// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#include "../src/game_core.hpp"
#include "../src/net_protocol.hpp"
#include "../src/session_rules.hpp"
#include "../src/link_rules.hpp"
#include "../src/feedback_rules.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

static void advance(gm::State& s,float sec,int8_t p1=0,int8_t p2=0){
  for(int i=0;i<int(sec*20+.5f);++i)gm::tick(s,.05f,p1,p2);
}
int main(){
  using namespace gm;
  assert(sizeof(proto::Packet)<=250 && sizeof(proto::Packet)==235);
  assert(!feedback::allowVibration(gm::DEMO));
  assert(feedback::allowVibration(gm::PLAY));
  assert(feedback::allowVibration(gm::CLEAR));
  assert(proto::VER==2); // Same packet format as R6, no mixed-protocol lobby.
  // In R6 HELLO stopped after one-sided discovery: drop that first HELLO
  // and the other watch could never join. R7 always re-advertises.
  assert(link_rules::helloDue(1300,650));
  assert(!link_rules::helloDue(1200,650));
  assert(link_rules::helloDue(550,0xfffffefau));
  assert(link_rules::bootstrapFromData(false,proto::STATE,0xABCD,0xABCD,
                                       proto::INPUT_CMD,proto::STATE));
  assert(link_rules::bootstrapFromData(false,proto::INPUT_CMD,0xABCD,0xABCD,
                                       proto::INPUT_CMD,proto::STATE));
  assert(!link_rules::bootstrapFromData(false,proto::STATE,0xABCE,0xABCD,
                                       proto::INPUT_CMD,proto::STATE));
  assert(!link_rules::bootstrapFromData(true,proto::STATE,0xABCD,0xABCD,
                                       proto::INPUT_CMD,proto::STATE));
  assert(session::onRight(true,false,true,WAIT)==session::START_SOLO);
  assert(session::onRight(true,true,true,WAIT)==session::START_COOP);
  assert(session::onRight(true,true,false,WAIT)==session::SEND_START);
  assert(session::onRight(true,true,false,DEMO)==session::SEND_START);
  assert(session::onRight(true,true,true,OVER)==session::START_COOP);
  assert(session::onRight(true,false,true,PLAY)==session::NONE);
  assert(!session::shouldAutoDemo(WAIT,1000,10999,true));
  assert(session::shouldAutoDemo(WAIT,1000,11000,true));
  assert(!session::shouldAutoDemo(WAIT,1000,11000,false));
  assert(!session::shouldAutoDemo(PLAY,1000,11000,true));
  assert(session::shouldAutoDemo(WAIT,0xfffff000u,0xfffff000u+10000u,true));

  State s{};
  start(s,false,0x238a1u);
  assert(s.phase==COUNTDOWN && s.playersActive==2 && s.waveQuota==12);
  advance(s,3.5f);
  assert(s.phase==PLAY);
  s.players[0].angle=.25f;s.players[1].angle=.65f;
  advance(s,.40f,+1,0);
  assert(angularDistance(s.players[0].angle,s.players[1].angle)>=.219f);
  // Beam-captured players cannot steer or fire; uncaptured partner can pass.
  s.enemies[0].mode=E_OUT;s.enemies[0].type=2;s.enemies[0].angle=2.3f;
  forceCapture(s,0,0);
  assert(s.players[0].status==P_CAPTURED);
  auto oldPos=s.players[0].angle;
  advance(s,.15f,+1,0);
  assert(s.players[0].angle==oldPos);
  s.players[1].angle=s.players[0].angle;
  tick(s,.05f,0,1);
  assert(s.players[1].angle!=s.players[0].angle);
  const auto scoreBefore=s.players[1].score;
  const auto killedBefore=s.destroyed;
  destroyEnemy(s,0,1);
  assert(s.players[0].status==P_ALIVE);
  assert(s.players[1].score==scoreBefore+1100);
  assert(s.destroyed==killedBefore+1);
  assert(angularDistance(s.players[0].angle,s.players[1].angle)>=.30f-.0001f);
  assert(s.players[0].power>0&&s.players[1].power>0);

  // All 12 enemies must be accounted for (shot or three-pass escapes).
  State clear{};start(clear,true,4421);
  clear.phase=PLAY;
  for(auto& e:clear.enemies)e=Enemy{};
  clear.destroyed=10;clear.escaped=2;
  clear.players[0].score=1000;
  tick(clear,.05f,0,0);
  assert(clear.phase==CLEAR);
  assert(clear.players[0].score==2000);
  advance(clear,2.3f);
  assert(clear.phase==PLAY && clear.wave==2);
  assert(clear.destroyed==0 && clear.escaped==0 && clear.waveQuota==14);

  // Active captor prevents an early stage clear; rescuing it can clear.
  State captive{};start(captive,false,1022);captive.phase=PLAY;
  for(auto& e:captive.enemies)e=Enemy{};
  captive.destroyed=11;
  captive.enemies[0].mode=E_OUT;captive.enemies[0].type=2;
  forceCapture(captive,0,0);
  assert(captive.players[0].status==P_CAPTURED);
  tick(captive,.05f,0,0);
  assert(captive.phase==PLAY);
  destroyEnemy(captive,0,1);
  tick(captive,.05f,0,0);
  assert(captive.phase==CLEAR);
  assert(captive.players[0].status==P_ALIVE);

  // Enemy disappears beyond the circular display and warns before reentry.
  State t{};start(t,true,55);t.phase=PLAY;
  for(auto& e:t.enemies)e=Enemy{};
  t.enemies[0].mode=E_OUT;t.enemies[0].radius=RING+21;
  t.enemies[0].angle=1.0f;t.enemies[0].speed=75.0f;
  tick(t,.05f,0,0);
  assert(t.enemies[0].mode==E_ORBIT && t.enemies[0].radius>=247.0f);
  advance(t,1.35f);
  assert(t.enemies[0].mode==E_WARNING && t.enemies[0].radius>237);
  const float incomingAngle=t.enemies[0].angle;
  advance(t,.6f);
  assert(t.enemies[0].mode==E_WARNING);
  assert(angularDistance(t.enemies[0].angle,incomingAngle)<.00001f);
  advance(t,.2f);
  assert(t.enemies[0].mode==E_RETURN);
  // Return can still hit if the player stays in its announced lane.
  State h{};start(h,true,777);h.phase=PLAY;
  for(auto& e:h.enemies)e=Enemy{};
  h.enemies[0].mode=E_RETURN;h.enemies[0].angle=h.players[0].angle;
  h.enemies[0].radius=RING+7;h.enemies[0].speed=85;
  h.enemies[0].pathBase=h.enemies[0].angle;
  const auto oldLives=h.players[0].lives;
  tick(h,.05f,0,0);
  assert(h.players[0].lives==oldLives-1);
  
  // The stage contains straight and parabolic paths.  A curved enemy
  // acquires a pronounced angular deflection at mid-radius, then escapes
  // off-screen without gaining any outbound collision damage.
  State curved{};start(curved,true,991);curved.phase=PLAY;
  bool sawStraight=false, sawCurved=false;
  for(const auto& e:curved.enemies) if(e.mode==E_DELAY) {
    if(e.curve==0) sawStraight=true;
    if(fabsf(e.curve)>.35f) sawCurved=true;
  }
  assert(sawStraight && sawCurved);
  const float baseAngle=curved.enemies[1].pathBase;
  const float curveStrength=curved.enemies[1].curve;
  assert(fabsf(curveStrength)>.35f);
  for(auto& e:curved.enemies)e=Enemy{};
  auto& arc=curved.enemies[1];
  arc.mode=E_OUT;arc.pathBase=baseAngle;arc.angle=baseAngle;
  arc.curve=curveStrength;arc.radius=101.0f;arc.speed=70;
  tick(curved,.05f,0,0);
  assert(arc.mode==E_OUT);
  const float bendAtMid=angularDistance(arc.angle,baseAngle);
  assert(bendAtMid>.20f);
  arc.radius=RING+23.0f;tick(curved,.05f,0,0);
  assert(arc.mode==E_ORBIT && arc.radius>RING+40);
  assert(curved.players[0].lives==3);

  // Paired DEMO: scripted cross-limit, tractor beam, rescue, joint attack.
  State demo{};beginDemo(demo,false,2026);
  assert(demo.phase==DEMO && demo.playersActive==2);
  assert(DEMO_SPEED==1.40f);
  assert(demo.enemies[1].mode==E_OUT && fabsf(demo.enemies[1].curve)>.90f);
  // Demo ships move ~1.4x faster; curved enemies follow the same quadratic
  // projection as live gameplay, not a lateral sinusoidal animation.
  const float sampleRadius=demo.enemies[1].radius;
  const float bearing=demo.enemies[1].angle;
  const float trail=demoTrailBearing(bearing,sampleRadius,1,fmaxf(8.f,sampleRadius-12.f));
  assert(angularDistance(bearing,trail)>.005f);
  const float initialP0=demo.players[0].angle, initialP1=demo.players[1].angle;
  advance(demo,1.9f);
  assert(fabsf(demo.demoTime-1.9f*DEMO_SPEED)<.02f);
  assert(angularDistance(demo.players[0].angle,initialP0)>.3f);
  assert(angularDistance(demo.players[1].angle,initialP1)>.3f);
  assert(angularDistance(demo.players[0].angle,demo.players[1].angle)>.22f);
  // Host snapshots drive GUEST demo, not a second independent timeline.
  proto::Packet mid{};proto::pack(demo,mid);
  State mirror{};proto::unpack(mid,mirror);
  assert(mirror.phase==DEMO && mirror.playersActive==2);
  assert(angularDistance(mirror.players[0].angle,demo.players[0].angle)<.0002f);
  assert(angularDistance(mirror.players[1].angle,demo.players[1].angle)<.0002f);

  advance(demo,2.1f);
  assert(angularDistance(demo.players[0].angle,demo.players[1].angle)>=.255f);
  assert(demo.players[0].status==P_ALIVE && demo.players[1].status==P_ALIVE);
  advance(demo,1.0f);
  assert(demo.players[0].status==P_CAPTURED);
  assert(demo.enemies[0].mode==E_CAPTOR && demo.enemies[0].target==0);
  advance(demo,1.5f);
  assert(demo.players[0].status==P_CAPTURED);
  assert(demo.shots[0].active && demo.shots[0].owner==1);
  advance(demo,1.8f);
  assert(demo.players[0].status==P_ALIVE && demo.players[1].status==P_ALIVE);
  assert(demo.players[0].power>0 && demo.players[1].power>0);
  assert(demo.players[0].score==2200 && demo.players[1].score==1900);
  // HOST -> GUEST snapshot sends demo time, stage counts and both ships.
  // On guest, reconstruct the same trail solely from packet-derived fields.
  const float guestTrail0=demoTrailBearing(demo.enemies[1].angle,demo.enemies[1].radius,1,
                                          fmaxf(8.f,demo.enemies[1].radius-12.f));
  proto::Packet m{};proto::pack(demo,m);
  State guest{};proto::unpack(m,guest);
  assert(guest.phase==DEMO && guest.playersActive==2);
  assert(fabsf(guest.demoTime-demo.demoTime)<.11f);
  assert(guest.destroyed==demo.destroyed && guest.waveQuota==demo.waveQuota);
  assert(guest.players[0].score==demo.players[0].score);
  assert(guest.players[1].score==demo.players[1].score);
  assert(guest.players[0].power>0 && guest.players[1].power>0);
  assert(angularDistance(guest.players[1].angle,demo.players[1].angle)<.0002f);
  const float guestTrail1=demoTrailBearing(guest.enemies[1].angle,guest.enemies[1].radius,1,
                                          fmaxf(8.f,guest.enemies[1].radius-12.f));
  assert(angularDistance(guestTrail0,guestTrail1)<.05f);
  // Right-button start after demo resets all scripted scores/positions.
  start(demo,false,42);
  assert(demo.phase==COUNTDOWN && demo.players[0].score==0 && demo.players[1].score==0);
  assert(demo.destroyed==0 && demo.players[0].status==P_ALIVE);
  State solo{};beginDemo(solo,true,88);
  advance(solo,10.0f);
  assert(solo.phase==DEMO && solo.playersActive==1);
  start(solo,true,998);
  assert(solo.phase==COUNTDOWN && solo.playersActive==1 && solo.players[1].status==P_DEAD);

  std::cout<<"PASS GapM5 R9: Packet="<<sizeof(proto::Packet)
           <<" bytes, 2P demo rescue reward mirrored, vibration gate, core and link rules\n";
}
