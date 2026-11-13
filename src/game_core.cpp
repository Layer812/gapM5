// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#include "game_core.hpp"
#include <cmath>

namespace gm {
namespace {
uint32_t next(State& s) {
  uint32_t v = s.random;
  v ^= v << 13; v ^= v >> 17; v ^= v << 5;
  return s.random = v ? v : 0x12345678u;
}
float randFloat(State& s) { return (next(s) & 65535u) / 65536.0f; }
float clamp01(float x) { return x<0?0:(x>1?1:x); }
// Curved enemy flight is a quadratic Bezier-like polar projection. Radius
// continues increasing while the *tangential* offset follows a parabola.
// "curve" is signed; 0 means traditional straight flight. No integration
// drift: every frame derives angle directly from the saved departure lane.
float outboundAngle(const Enemy& e) {
  const float u=clamp01((e.radius-8.0f)/(RING+23.0f-8.0f));
  const float radial=e.radius;
  const float tangential=e.curve*(2.0f*u*(1.0f-u)*160.0f + u*u*95.0f);
  return wrap(e.pathBase + atan2f(tangential,radial));
}
// Return warning remains accurate at the rim; the inward leg bends only once
// the enemy has safely crossed the pilot's ring.
float inboundAngle(const Enemy& e) {
  const float u=clamp01((RING-12.0f-e.radius)/(RING-12.0f-6.0f));
  return wrap(e.pathBase - e.curve*.48f*(4.0f*u*(1.0f-u)));
}

void waveStart(State& s) {
  for (auto& e: s.enemies) e = Enemy{};
  for (auto& b: s.shots) b = Shot{};
  const int count = (10 + int(s.wave) * 2 > MAX_ENEMY) ? MAX_ENEMY : 10 + int(s.wave) * 2;
  s.waveQuota=static_cast<uint8_t>(count);
  s.destroyed=0; s.escaped=0;
  for (int i=0; i<count; ++i) {
    auto& e=s.enemies[i];
    e.mode = E_DELAY;
    e.timer = 0.15f + 0.25f*i;
    e.type = ((i+int(s.wave))%7 == 2) ? 2 : ((i%5==0) ? 1 : 0);
    e.hp = e.type == 1 ? 2 : 1;
    e.speed = 62.0f + s.wave*5.0f + 19.0f*randFloat(s);
    e.angle = wrap((i * TAU/count) + (s.wave * 0.26f) + (randFloat(s)-.5f)*.24f);
    e.pathBase=e.angle;
    // Wave 1: half straight, half gentle arcs. Higher waves bend more.
    const float bend=(.45f+.065f*fminf(float(s.wave),7.0f))*(.76f+.45f*randFloat(s));
    e.curve = (i%3==0)?0.0f:((i%2)?bend:-bend);
    e.radius = 5;
  }
  s.waveDelay=0;
}
void clearWave(State& s) {
  s.phase=CLEAR;
  s.waveDelay=2.2f;
  s.banner=2.2f;
  s.eventType=5;
  ++s.event;
  // A reward for keeping the enemy from escaping after three passes.
  const uint32_t bonus=100u*uint32_t(s.destroyed);
  for(int i=0;i<s.playersActive;++i) s.players[i].score+=bonus;
  for(auto& b:s.shots)b=Shot{};
}
void loseLife(State& s, int id) {
  auto& p=s.players[id];
  if(p.status!=P_ALIVE || p.invuln>0) return;
  if(p.lives>0) --p.lives;
  if(p.lives==0) p.status=P_DEAD;
  else p.invuln=1.7f;
  ++s.event;s.eventType=1;s.banner=.7f;
}
void fire(State& s, int playerIndex) {
  for(auto& b:s.shots) if(!b.active) {
    b.active=true; b.angle=s.players[playerIndex].angle;
    b.radius=RING-17; b.owner=static_cast<uint8_t>(playerIndex); return;
  }
}
void collision(State& s) {
  for(int i=0;i<MAX_SHOTS;++i) {
    auto& b=s.shots[i];
    if(!b.active)continue;
    for(int j=0;j<MAX_ENEMY;++j) {
      auto& e=s.enemies[j];
      if(e.mode!=E_OUT && e.mode!=E_RETURN && e.mode!=E_CAPTOR)continue;
      const float tol=e.radius<75?.20f:(e.radius<145?.115f:.09f);
      if(fabsf(b.radius-e.radius)<13.0f && angularDistance(b.angle,e.angle)<tol) {
        b.active=false;
        if(e.hp>1)--e.hp;
        else destroyEnemy(s,j,b.owner);
        break;
      }
    }
  }
}
void checkOver(State& s) {
  if(s.playersActive==1) {
    if(s.players[0].status!=P_ALIVE)s.phase=OVER;
  } else if(s.players[0].status!=P_ALIVE && s.players[1].status!=P_ALIVE) {
    s.phase=OVER;
  }
}
// Demonstration enemies follow the SAME curved polar projection as the game.
// The curve depends only on index, so guest can draw identical trails without
// increasing the 235-byte ESP-NOW packet or receiving path internals.
float demoCurve(int i) {
  if(i%4==0)return 0.0f;  // Preserve a few straight runners for contrast.
  return ((i%2)?1.0f:-1.0f)*(0.96f+0.10f*float(i%3));
}
void demoEnemy(Enemy& e,float base,float radius,uint8_t type,float curve) {
  e=Enemy{};e.mode=E_OUT;e.type=type;e.hp=1;
  e.pathBase=wrap(base);e.curve=curve;e.radius=radius;
  e.angle=outboundAngle(e);
}
void demoShot(Shot& b,float angle,float radius,uint8_t owner) {
  b=Shot{};b.active=true;b.angle=wrap(angle);b.radius=radius;b.owner=owner;
}
void demoTick(State& s,float dt) {
  const float previous=s.demoTime;
  // Faster attract mode (about 11.4s for a complete 16s script).
  // HOST runs the timeline; GUEST receives the packed State snapshot.
  s.demoTime+=dt*DEMO_SPEED;
  if(s.demoTime>=16.0f){s.demoTime=fmodf(s.demoTime,16.0f);}
  const float t=s.demoTime;
  // Presentation is real State data, sent by HOST in standard STATE packets.
  // It never mutates the result of an actual game: start() resets everything.
  for(auto& e:s.enemies)e=Enemy{};
  for(auto& b:s.shots)b=Shot{};
  for(auto& p:s.players){p.status=P_ALIVE;p.invuln=0;p.power=0;p.lives=3;}
  s.players[0].score=1200;
  s.players[1].score=s.playersActive==2?900:0;
  // Two clear, distinct pilot paths before the no-overtaking encounter.
  s.players[0].angle=wrap(2.95f-.15f*fminf(t,3.5f)+.055f*sinf(2.9f*t));
  s.players[1].angle=wrap(.38f+.14f*fminf(t,3.5f)-.052f*sinf(2.6f*t));
  s.wave=1;s.waveQuota=12;s.destroyed=5;s.escaped=0;
  if(s.playersActive==1) {
    for(int i=0;i<4;++i)
      demoEnemy(s.enemies[i],wrap(.8f+i*1.31f+.06f*sinf(t*.45f+i)),
                25.0f+fmodf(t*81.0f+i*43.0f,181.0f),i%3==0?1:0,demoCurve(i));
    for(int i=0;i<4;++i)
      demoShot(s.shots[i],s.players[0].angle,
               184.0f-fmodf(t*295.0f+i*39.0f,170.0f),0);
    return;
  }
  if(t<3.5f || t>=11.5f){
    if(t>=11.5f) {
      const float together=t-11.5f;
      // Maintain separation while visibly sweeping a whole sector together.
      s.players[0].angle=1.79f+.24f*together;
      s.players[1].angle=2.38f+.24f*together;
      s.players[0].power=1.0f;s.players[1].power=1.0f;
      s.destroyed=9;
    }
    for(int i=0;i<5;++i)
      demoEnemy(s.enemies[i],wrap(.25f+i*1.14f+.08f*sinf(t*.56f+i)),
                21.0f+fmodf(t*89.0f+i*47.0f,187.0f),i%3==0?1:0,demoCurve(i));
    for(int i=0;i<6;++i){
      const int p=i%2;
      demoShot(s.shots[i],s.players[p].angle,
               181.0f-fmodf(t*305.0f+i*31.0f,160.0f),static_cast<uint8_t>(p));
    }
  } else if(t<6.5f) {
    // Both ships close in on the same sector and stop at a 0.26rad gap;
    // neither is ever permitted to pass through its partner.
    const float d=fminf((t-3.5f)*.31f,.64f);
    s.players[0].angle=2.425f-d;
    s.players[1].angle=.87f+d;
    demoEnemy(s.enemies[0],.3f,92,1,demoCurve(0));
    demoEnemy(s.enemies[1],4.1f,132,0,demoCurve(1));
    demoShot(s.shots[0],s.players[0].angle,130,0);
    demoShot(s.shots[1],s.players[1].angle,95,1);
  } else if(t<11.5f) {
    s.players[0].angle=1.785f;
    s.players[0].status=P_CAPTURED;
    const float lead=fmaxf(0.0f,t-8.0f);
    // Free 2P may pass immobilised 1P to reach the captor.
    s.players[1].angle=1.51f+fminf(lead*.31f,.87f);
    auto& e=s.enemies[0];
    e.mode=E_CAPTOR;e.angle=1.785f+.035f*sinf(t*2.7f);
    e.radius=137.0f;e.type=2;e.target=0;e.hp=1;
    if(t>=9.0f) {
      for(int i=0;i<5;++i)
        demoShot(s.shots[i],s.players[1].angle,
          184.0f-fmodf((t-9.0f)*300.0f+i*27.0f,118.0f),1);
    }
  }
  // Synchronised capture/rescue effects on both watches.
  if((previous<6.5f && t>=6.5f) || (t<previous && t>=6.5f)) {
    ++s.event;s.eventType=2;s.banner=1.0f;
  }
  if((previous<11.5f && t>=11.5f) || (t<previous && t>=11.5f)) {
    ++s.event;s.eventType=3;s.banner=1.8f;
  }
}
} // namespace

// Project the enemy's last few positions back along its quadratic lane.
// For paired demos e.curve itself is not included in the compact network
// snapshot. Reconstruct the signed curvature from the array slot on BOTH
// watches, and derive the starting lane from the received angle/radius.
float demoTrailBearing(float angle, float radius, int slot, float olderRadius) {
  const float curve=demoCurve(slot);
  if(curve==0)return wrap(angle);
  const float nowU=clamp01((radius-8.0f)/(RING+23.0f-8.0f));
  const float oldU=clamp01((olderRadius-8.0f)/(RING+23.0f-8.0f));
  const auto deflection=[&](float r,float u){
    const float tangential=curve*(2.0f*u*(1.0f-u)*160.0f+u*u*95.0f);
    return atan2f(tangential,r);
  };
  return wrap(angle-deflection(radius,nowU)+deflection(olderRadius,oldU));
}

float wrap(float a) {
  while(a<0)a+=TAU;
  while(a>=TAU)a-=TAU;
  return a;
}
float angularDistance(float a,float b) {
  float d=fabsf(wrap(a)-wrap(b));
  return d>TAU*.5f?TAU-d:d;
}
void start(State& s,bool solo,uint32_t seed) {
  s=State{};
  s.random=seed?seed:0x12345678u;
  s.playersActive=solo?1:2;
  s.players[0].angle=2.20f;s.players[1].angle=.94f;
  s.players[1].lives=solo?0:3;
  s.players[1].status=solo?P_DEAD:P_ALIVE;
  s.phase=COUNTDOWN;s.countdown=3.0f;
  waveStart(s);
}
void beginDemo(State& s,bool solo,uint32_t seed) {
  s=State{};
  s.random=seed?seed:0x4d454d4fu;
  s.playersActive=solo?1:2;
  s.phase=DEMO;
  // Set frame=0 so both watches start with the same scripted sequence.
  s.frame=0;s.demoTime=0;
  demoTick(s,0.0f);
}
void forceCapture(State& s,int id,int ei) {
  if(id<0||id>1||ei<0||ei>=MAX_ENEMY)return;
  auto& p=s.players[id];auto& e=s.enemies[ei];
  if(p.status!=P_ALIVE||s.playersActive==1)return;
  p.status=P_CAPTURED;
  e.mode=E_CAPTOR;e.radius=140;e.target=static_cast<uint8_t>(id);
  e.hp=1;e.beamCharge=0;
  ++s.event;s.eventType=2;s.banner=1.1f;
}
void destroyEnemy(State& s,int ei,int rescuer) {
  if(ei<0||ei>=MAX_ENEMY||rescuer<0||rescuer>=2)return;
  auto& e=s.enemies[ei];if(e.mode==E_NONE)return;
  const uint8_t target=e.target;
  const bool wasCaptor=e.mode==E_CAPTOR&&target<2;
  const float rescueAngle=e.angle;
  const uint8_t pointsType=e.type;
  e=Enemy{};
  if(s.phase==PLAY && s.destroyed<s.waveQuota)++s.destroyed;
  s.players[rescuer].score+=wasCaptor?1100:(pointsType==1?250:120);
  if(wasCaptor && s.players[target].status==P_CAPTURED) {
    s.players[target].status=P_ALIVE;
    float arrival=rescueAngle;
    const int partner=1-int(target);
    if(s.playersActive==2 && s.players[partner].status==P_ALIVE &&
       angularDistance(arrival,s.players[partner].angle)<.30f) {
      const float delta=wrap(arrival-s.players[partner].angle);
      arrival=wrap(s.players[partner].angle+(delta<TAU*.5f?.31f:-.31f));
    }
    s.players[target].angle=arrival;
    s.players[target].invuln=2.3f;
    s.players[target].power=3.5f;
    s.players[rescuer].power=3.5f;
    ++s.event;s.eventType=3;s.banner=1.9f;
  }
}
void tick(State& s,float dt,int8_t d1,int8_t d2) {
  if(s.paused||s.phase==WAIT||s.phase==OVER)return;
  if(dt<0)return;
  if(dt>.05f)dt=.05f;
  ++s.frame;
  if(s.banner>0)s.banner-=dt;
  if(s.goFlash>0)s.goFlash-=dt;
  if(s.phase==DEMO){demoTick(s,dt);return;}
  if(s.phase==CLEAR){
    s.waveDelay-=dt;
    if(s.waveDelay<=0){if(s.wave<99)++s.wave;waveStart(s);s.phase=PLAY;s.goFlash=.75f;}
    return;
  }
  if(s.phase==COUNTDOWN) {
    s.countdown-=dt;
    if(s.countdown<=0){s.countdown=0;s.phase=PLAY;s.goFlash=.75f;}
    return;
  }
  const int8_t dirs[2]={d1,d2};
  for(int i=0;i<s.playersActive;++i) {
    auto& p=s.players[i];
    if(p.invuln>0)p.invuln-=dt;
    if(p.power>0)p.power-=dt;
    if(p.status!=P_ALIVE)continue;
    if(dirs[i]) {
      const float proposed=wrap(p.angle+dirs[i]*2.05f*dt);
      const int other=1-i;
      if(s.playersActive==1||s.players[other].status!=P_ALIVE||
         angularDistance(proposed,s.players[other].angle)>.22f)p.angle=proposed;
    }
    p.fireCooldown-=dt;
    if(p.fireCooldown<=0){fire(s,i);p.fireCooldown=p.power>0?.105f:.225f;}
  }
  for(auto& b:s.shots)if(b.active){b.radius-=270.0f*dt;if(b.radius<8)b.active=false;}
  bool any=false;
  for(int ei=0;ei<MAX_ENEMY;++ei) {
    auto& e=s.enemies[ei];if(e.mode==E_NONE)continue;
    any=true;
    if(e.mode==E_DELAY) {
      e.timer-=dt;
      if(e.timer<=0){e.mode=E_OUT;e.radius=8;e.angle=e.pathBase;}
      continue;
    }
    if(e.mode==E_OUT) {
      e.radius+=e.speed*dt;
      e.angle=outboundAngle(e);
      // Capture warning: must be aimed continuously for >0.5s.
      // Moving off the beam cancels the lock. No unavoidable instant capture.
      if(e.type==2&&s.playersActive==2&&e.radius>103&&e.radius<187) {
        int target=-1;
        if(e.target<2&&s.players[e.target].status==P_ALIVE&&
           angularDistance(s.players[e.target].angle,e.angle)<.27f)
          target=e.target;
        if(target<0){
          float nearest=.32f;
          for(int p=0;p<2;++p){
            const float gap=angularDistance(s.players[p].angle,e.angle);
            if(s.players[p].status==P_ALIVE&&s.players[p].invuln<=0&&gap<nearest){
              target=p;nearest=gap;
            }
          }
        }
        if(target>=0) {
          e.target=static_cast<uint8_t>(target);
          const float gap=angularDistance(s.players[target].angle,e.angle);
          e.beamCharge+=gap<.24f?dt:-dt*2.2f;
          if(e.beamCharge<0)e.beamCharge=0;
          if(e.beamCharge>=.52f){forceCapture(s,target,ei);}
        } else {e.target=255;e.beamCharge=0;}
      } else if(e.type==2){e.target=255;e.beamCharge=0;}
      // Outbound enemies leave ABOVE the player track: no contact on exit.
      if(e.radius>RING+23){e.mode=E_ORBIT;e.radius=RING+46;e.timer=0;}
    } else if(e.mode==E_ORBIT) {
      e.timer+=dt;e.radius=RING+46;
      e.angle=wrap(e.angle+(ei%2?-1:1)*1.35f*dt);
      if(e.timer>1.32f){e.mode=E_WARNING;e.timer=.72f;}
    } else if(e.mode==E_WARNING) {
      e.radius=RING+46;e.timer-=dt;
      if(e.timer<=0){e.mode=E_RETURN;e.pathBase=e.angle;}
    } else if(e.mode==E_RETURN) {
      e.radius-=e.speed*1.4f*dt;
      e.angle=inboundAngle(e);
      if(e.radius<RING+5&&e.radius>RING-10) {
        for(int p=0;p<s.playersActive;++p)
          if(s.players[p].status==P_ALIVE&&s.players[p].invuln<=0&&
             angularDistance(e.angle,s.players[p].angle)<.115f)loseLife(s,p);
      }
      if(e.radius<=6) {
        if(++e.loops>=3){e=Enemy{};if(s.escaped<s.waveQuota)++s.escaped;}
        else {
          e.mode=E_OUT;e.radius=8;e.pathBase=wrap(e.angle+.50f);
          e.curve=-e.curve; // subsequent pass curls the other way
          e.angle=e.pathBase;
        }
      }
    } else if(e.mode==E_CAPTOR) {
      e.radius=137.0f+3.5f*sinf(s.frame*.025f);
      e.angle=wrap(e.angle+.37f*dt);
    }
  }
  collision(s);
  checkOver(s);
  if(s.phase==OVER)return;
  // CLEAR requires every initial enemy to be shot or have exhausted 3 runs,
  // including any captor. No hidden enemy remains between waves.
  if(!any && s.destroyed+s.escaped>=s.waveQuota)clearWave(s);
}
}
