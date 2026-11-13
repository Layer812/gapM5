// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#include "draw_game.hpp"
#include <cmath>
#include <cstdio>
namespace screen {
namespace {
// Subdued enemy colors: slate, dusty olive, old-rose. Players are brighter.
constexpr uint16_t BG=0x0000, GRID=0x0883, GRID2=0x1085,
  BLUE=0x55DF, BLUE_D=0x22B9, ROSE=0xFA39, WHITE=0xFFFF,
  SLATE=0x9D30, OLIVE=0xA593, DUSTY=0xB34D,
  PINK=0xFB7D, ORANGE=0xFC2A, GOLD=0xE6A8;
int W=468,H=468,CX=234,CY=234;
float S=1;
int px(float x){return CX+static_cast<int>(x*S);}
int py(float y){return CY+static_cast<int>(y*S);}
int len(float r){return static_cast<int>(r*S+.5f);}
void line(lgfx::LGFX_Device& g,float ax,float ay,float bx,float by,uint16_t c) {
  g.drawLine(px(ax),py(ay),px(bx),py(by),c);
}
void tri(lgfx::LGFX_Device& g,float ax,float ay,float bx,float by,float cx,float cy,uint16_t c) {
  g.fillTriangle(px(ax),py(ay),px(bx),py(by),px(cx),py(cy),c);
}
void txt(lgfx::LGFX_Device& g,const char* t,float x,float y,uint16_t fg,
         const lgfx::IFont* font=&fonts::Font2) {
  g.setFont(font);g.setTextColor(fg,BG);g.setTextDatum(middle_center);
  g.drawString(t,px(x),py(y));
}
void ship(lgfx::LGFX_Device& g,float a,float r,uint16_t main,bool captured,bool blink) {
  if(blink)return;
  float sx=cosf(a),sy=sinf(a);
  float cx=r*sx,cy=r*sy;
  float nx=-sx,ny=-sy;  // gun and nose always face the center
  float tx=-sy,ty=sx;
  auto at=[&](float forward,float sideways,float& x,float& y){
    x=cx+nx*forward+tx*sideways;
    y=cy+ny*forward+ty*sideways;
  };
  float x0,y0,x1,y1,x2,y2;
  at(17,0,x0,y0);at(-7,-11,x1,y1);at(-7,11,x2,y2);
  tri(g,x0,y0,x1,y1,x2,y2,main);
  at(0,0,x0,y0);at(-13,-17,x1,y1);at(-9,-3,x2,y2);
  tri(g,x0,y0,x1,y1,x2,y2,WHITE);
  at(0,0,x0,y0);at(-9,3,x1,y1);at(-13,17,x2,y2);
  tri(g,x0,y0,x1,y1,x2,y2,WHITE);
  at(3,0,x0,y0);g.fillCircle(px(x0),py(y0),len(3.6),BLUE_D);
  if(!captured){
    at(-14,0,x0,y0);at(-26,0,x1,y1);
    line(g,x0,y0,x1,y1,main);
  }
}
void enemy(lgfx::LGFX_Device& g,const gm::Enemy& e) {
  if(e.mode==gm::E_NONE || e.mode==gm::E_DELAY)return;
  if(e.radius>237)return;
  const float a=e.angle;
  const float ur=cosf(a),vr=sinf(a),tx=-vr,ty=ur;
  const float r=e.radius;
  const float x=ur*r,y=vr*r;
  const float z=3.5f+e.radius*.034f;
  const uint16_t c=e.type==1?OLIVE:(e.type==2?DUSTY:SLATE);
  tri(g,x+ur*z*1.3f,y+vr*z*1.3f,
      x-ur*z+tx*z*.75f,y-vr*z+ty*z*.75f,
      x-ur*z-tx*z*.75f,y-vr*z-ty*z*.75f,c);
  tri(g,x-ur*z*.3f+tx*z*.40f,y-vr*z*.3f+ty*z*.40f,
      x-ur*z*.5f+tx*z*1.6f,y-vr*z*.5f+ty*z*1.6f,
      x+ur*z*.43f+tx*z*.95f,y+vr*z*.43f+ty*z*.95f,c);
  tri(g,x-ur*z*.3f-tx*z*.40f,y-vr*z*.3f-ty*z*.40f,
      x+ur*z*.43f-tx*z*.95f,y+vr*z*.43f-ty*z*.95f,
      x-ur*z*.5f-tx*z*1.6f,y-vr*z*.5f-ty*z*1.6f,c);
  g.fillCircle(px(x),py(y),len(1.6f+z*.11f),e.type==2?PINK:WHITE);
  if(e.mode==gm::E_CAPTOR)g.drawCircle(px(x),py(y),len(z*1.5f),PINK);
}
// Dotted parabolic afterimages in the attract demo: reconstruct positions
// from the received polar point and the slot's deterministic demo curvature.
// Thus HOST and GUEST render the same curve without changing ESP-NOW packet.
void demoCurveTrail(lgfx::LGFX_Device& g,const gm::Enemy& e,int slot) {
  if(e.mode!=gm::E_OUT || e.radius<42 || e.radius>gm::RING+4)return;
  const uint16_t c=(slot&1)?DUSTY:BLUE_D;
  for(int j=1;j<=5;++j) {
    const float r=e.radius-float(j)*9.0f;
    if(r<8)break;
    const float a=gm::demoTrailBearing(e.angle,e.radius,slot,r);
    const float x=r*cosf(a),y=r*sinf(a);
    g.fillCircle(px(x),py(y),len(j<=2?2.2f:1.25f),c);
  }
}
void inboundWarning(lgfx::LGFX_Device& g,const gm::Enemy& e,uint32_t frame) {
  if(e.mode!=gm::E_WARNING)return;
  // Visible on BOTH watches via the existing enemy mode/angle snapshot.
  // The real enemy remains outside the display at radius 247.
  const float a=e.angle,u=cosf(a),v=sinf(a),tx=-v,ty=u;
  const uint16_t c=((frame/9)%2) ? ORANGE : GOLD;
  const float tip=207.0f,base=222.0f;
  tri(g, tip*u,tip*v,
      base*u+7.0f*tx,base*v+7.0f*ty,
      base*u-7.0f*tx,base*v-7.0f*ty,c);
  line(g,228*u+7*tx,228*v+7*ty, 228*u-7*tx,228*v-7*ty,c);
}
void background(lgfx::LGFX_Device& g,uint32_t frame) {
  g.fillScreen(BG);
  const int outer=len(229);
  g.drawCircle(CX,CY,outer,BLUE_D);
  g.drawCircle(CX,CY,outer-2,GRID2);
  for(int r=47;r<=200;r+=42)g.drawCircle(CX,CY,len(float(r)),GRID);
  for(int i=0;i<24;++i){
    const float a=i*gm::TAU/24;
    const float cs=cosf(a),sn=sinf(a);
    line(g,cs*35,sn*35,cs*222,sn*222,GRID2);
  }
  // Original procedural stars. The radial movement sells the 3-D depth.
  for(int i=0;i<60;++i) {
    const float a=gm::wrap((i*2.39996f)+(i%6)*.018f);
    const float f=fmodf(float((i*71 + frame*(i%4+1))%750),750.0f)/750.0f;
    const float r=15+f*207;
    if(i%5==0) {
      const uint16_t c=(i%3==0)?OLIVE:BLUE_D;
      line(g,r*cosf(a),r*sinf(a),(r+2.2f)*cosf(a),(r+2.2f)*sinf(a),c);
    } else {
      const uint16_t c=(i%3==0)?GRID2:(i%3==1?SLATE:BLUE_D);
      g.drawPixel(px(r*cosf(a)),py(r*sinf(a)),c);
    }
  }
  g.drawCircle(CX,CY,len(17),BLUE_D);
  g.fillCircle(CX,CY,len(5),WHITE);
  g.drawCircle(CX,CY,len(11),GRID2);
}
void hud(lgfx::LGFX_Device& g,const gm::State& s,const char* role,bool paired,
         bool online,bool wifiOK,bool solo,uint8_t demoCountdown){
  char tmp[60];
  txt(g,"GapM5",0,-208,WHITE,&fonts::Font4);
  snprintf(tmp,sizeof(tmp),"1P %06lu",static_cast<unsigned long>(s.players[0].score%1000000ul));
  txt(g,tmp,-110,-174,BLUE,&fonts::Font2);
  if(s.playersActive==2 || (s.phase==gm::WAIT && paired)){
    snprintf(tmp,sizeof(tmp),"2P %06lu",static_cast<unsigned long>(s.players[1].score%1000000ul));
    txt(g,tmp,110,-174,ROSE,&fonts::Font2);
  }
  if(s.phase==gm::WAIT){
    txt(g,"RADIAL CO-OP",0,-108,BLUE,&fonts::Font4);
    txt(g,(paired && online)?"2P LINKED!":(wifiOK?"ESP-NOW SEARCHING":"ESP-NOW OFF"),
        0,-39,paired?GOLD:WHITE,&fonts::Font4);
    txt(g,paired&&online?"RIGHT: START 2P":"RIGHT: START SOLO",0,12,GOLD,&fonts::Font4);
    snprintf(tmp,sizeof(tmp),"DEMO IN %u",static_cast<unsigned>(demoCountdown));
    txt(g,tmp,0,64,WHITE,&fonts::Font4);
    txt(g,"LEFT / RIGHT : MOVE",0,112,BLUE,&fonts::Font2);
    return;
  }
  if(s.phase==gm::DEMO){
    txt(g,"- AUTO DEMO -",0,-128,GOLD,&fonts::Font4);
    const float t=s.demoTime;
    const char* message=s.playersActive==1?"CENTER ATTACK":
      (t<3.5f?"CO-OP ATTACK":
      (t<6.5f?"NO OVERTAKING":
      (t<9.0f?"1P CAPTURED!":
      (t<11.5f?"2P TO RESCUE":"RESCUE BONUS!"))));
    txt(g,message,0,42,(t>=6.5f&&t<11.5f)?ORANGE:WHITE,&fonts::Font4);
    txt(g,paired&&online?"RIGHT: START 2P":"RIGHT: START SOLO",0,102,GOLD,&fonts::Font4);
    if(paired && online){
      txt(g,"SYNCHRONIZED 2P",0,151,GOLD,&fonts::Font2);
      txt(g,role,0,-166,BLUE,&fonts::Font2);
    } else txt(g,"DEMO REPEATS",0,150,BLUE,&fonts::Font2);
    return;
  }
  snprintf(tmp,sizeof(tmp),"WAVE %02u",s.wave);
  txt(g,tmp,0,195,GOLD,&fonts::Font2);
  snprintf(tmp,sizeof(tmp),"1P x%u",s.players[0].lives);
  txt(g,tmp,-119,191,BLUE,&fonts::Font2);
  if(s.playersActive==2){
    snprintf(tmp,sizeof(tmp),"2P x%u",s.players[1].lives);
    txt(g,tmp,120,191,ROSE,&fonts::Font2);
  }
  if(s.phase==gm::PLAY || s.phase==gm::CLEAR){
    snprintf(tmp,sizeof(tmp),"DESTROYED %u/%u   MISSED %u",
             s.destroyed,s.waveQuota,s.escaped);
    txt(g,tmp,0,163,WHITE,&fonts::Font2);
  }
  if(!online&&paired&&!solo){
    txt(g,"LINK LOST",0,78,ORANGE,&fonts::Font4);
    txt(g,"AUTO RECONNECT",0,109,WHITE,&fonts::Font2);
  }
  if(s.phase==gm::PLAY&&s.goFlash>.1f)txt(g,"GO!",0,72,GOLD,&fonts::Font4);
  if(s.phase==gm::COUNTDOWN){
    snprintf(tmp,sizeof(tmp),"%d",int(ceilf(s.countdown)));
    txt(g,tmp,0,65,GOLD,&fonts::Font4);
    txt(g,"GET READY",0,106,WHITE,&fonts::Font4);
  }
  if(s.phase==gm::CLEAR){
    txt(g,"WAVE CLEAR!",0,50,GOLD,&fonts::Font4);
    snprintf(tmp,sizeof(tmp),"BONUS +%lu",100ul*static_cast<unsigned long>(s.destroyed));
    txt(g,tmp,0,92,WHITE,&fonts::Font4);
    txt(g,"NEXT WAVE...",0,124,BLUE,&fonts::Font2);
  }
  if(s.phase==gm::OVER){
    txt(g,"GAME OVER",0,66,ORANGE,&fonts::Font4);
    txt(g,"RIGHT: RESTART",0,111,GOLD,&fonts::Font4);
  }
  if(s.players[0].status==gm::P_CAPTURED)txt(g,"1P CAPTURED",-99,117,BLUE,&fonts::Font2);
  if(s.players[1].status==gm::P_CAPTURED)txt(g,"2P CAPTURED",99,117,ROSE,&fonts::Font2);
  if(s.banner>.1f && s.phase==gm::PLAY){
    const char* phrase=s.eventType==3?"RESCUE BONUS!":(s.eventType==2?"CAPTURED!":"HIT!");
    txt(g,phrase,0,139,s.eventType==3?GOLD:ORANGE,&fonts::Font4);
  }
  if(s.phase==gm::PLAY && online && paired){
    txt(g,role,0,-140,GOLD,&fonts::Font2);
  } else if(solo && s.phase==gm::PLAY){
    txt(g,"SOLO MODE",0,-140,GOLD,&fonts::Font2);
  }
}
} // namespace
void draw(const gm::State& s,const char* role,bool paired,bool online,bool wifiOK,bool solo,uint8_t demoCountdown){
  auto& lcd=M5.Display;
  W=lcd.width();H=lcd.height();CX=W/2;CY=H/2;
  S=(W<H?W:H)/468.0f;
  lcd.startWrite();
  background(lcd,s.frame);
  for(int j=0;j<gm::MAX_ENEMY;++j){
    const auto& e=s.enemies[j];
    if(e.mode==gm::E_CAPTOR && e.target<2){
      const auto& p=s.players[e.target];
      line(lcd,e.radius*cosf(e.angle),e.radius*sinf(e.angle),
           108*cosf(e.angle),108*sinf(e.angle),PINK);
      line(lcd,(e.radius+4)*cosf(e.angle), (e.radius+4)*sinf(e.angle),
           110*cosf(e.angle),110*sinf(e.angle),DUSTY);
      const float mid=(e.radius+108)*.5f;
      lcd.drawCircle(px(mid*cosf(e.angle)),py(mid*sinf(e.angle)),len(7),GRID2);
    } else if(e.type==2 && e.mode==gm::E_OUT && e.target<2) {
      const auto& p=s.players[e.target];
      if(p.status==gm::P_ALIVE){
        line(lcd,e.radius*cosf(e.angle),e.radius*sinf(e.angle),
             196*cosf(p.angle),196*sinf(p.angle),DUSTY);
        if(s.frame%12<6)lcd.drawCircle(px(196*cosf(p.angle)),py(196*sinf(p.angle)),len(22),PINK);
      }
    }
  }
  for(const auto& b:s.shots)if(b.active){
    const float u=cosf(b.angle),v=sinf(b.angle);
    line(lcd,(b.radius-8)*u,(b.radius-8)*v,b.radius*u,b.radius*v,
        b.owner?ROSE:BLUE);
  }
  if(s.phase==gm::DEMO)
    for(int i=0;i<gm::MAX_ENEMY;++i)demoCurveTrail(lcd,s.enemies[i],i);
  for(const auto& e:s.enemies)enemy(lcd,e);
  // Draw HUD before player ships: a ship never disappears behind a score label
  // while traversing the top or bottom of the complete circular track.
  hud(lcd,s,role,paired,online,wifiOK,solo,demoCountdown);
  for(const auto& e:s.enemies)inboundWarning(lcd,e,s.frame);
  for(int i=0;i<s.playersActive;++i){
    const auto& p=s.players[i];
    if(p.status==gm::P_DEAD)continue;
    bool blink=(p.invuln>0 && (s.frame%12)<4);
    if(p.status==gm::P_CAPTURED){
      float a=p.angle;
      for(const auto& e:s.enemies)if(e.mode==gm::E_CAPTOR && e.target==i){a=e.angle;break;}
      ship(lcd,a,108.0f,i?ROSE:BLUE,true,false);
    } else ship(lcd,p.angle,gm::RING,i?ROSE:BLUE,false,blink);
  }
  if(s.phase==gm::DEMO && s.playersActive==2 && s.demoTime>=11.5f){
    for(int i=0;i<2;++i){
      const float a=s.players[i].angle;
      const float x=gm::RING*cosf(a),y=gm::RING*sinf(a);
      lcd.drawCircle(px(x),py(y),len(22.0f+(s.frame%12)*.23f),GOLD);
    }
  }
  if(s.phase==gm::DEMO && s.playersActive==2 && s.demoTime>=3.5f && s.demoTime<6.5f){
    // The lit barrier between the two crafts makes NO OVERTAKING unmistakable.
    const float a=(s.players[0].angle+s.players[1].angle)*.5f;
    const float xx=gm::RING*cosf(a), yy=gm::RING*sinf(a);
    lcd.fillCircle(px(xx),py(yy),len(5),ORANGE);
    lcd.drawCircle(px(xx),py(yy),len(11),GOLD);
  }
  lcd.endWrite();
}
}
