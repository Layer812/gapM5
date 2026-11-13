// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
// GapM5 R8: 1.40x synced demo with quadratic trails; R4 boot retained.
// Boot is based on bikeM5 R24's known display-success native sequence.
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_ota_ops.h>
#include "game_core.hpp"
#include "session_rules.hpp"
#include "net_link.hpp"
#include "draw_game.hpp"
#include "music.hpp"

#if !defined(BOARD_ID) || BOARD_ID != 30
#error "GapM5 only supports the M5Stack StopWatch (BOARD_ID=30)"
#endif
static_assert(static_cast<int>(m5::board_t::board_M5StopWatch)==30,
              "M5Unified StopWatch board ID changed");

namespace {
gm::State game{};
net::Link radioLink;
music::Player bgm;
bool lcdReady=false;
uint32_t lastLogic=0,lastDraw=0,vibrateUntil=0,restartHold=0;
uint8_t observedEvent=0;
uint32_t lastFrameNumber=0;
uint32_t introStarted=0;
uint32_t guestStartSendUntil=0;
uint32_t recoveryHeldSince=0;
uint32_t lastPanelRetry=0;
bool recoveryArmed=true;
uint8_t displayRecoveryCount=0;
uint16_t previouslyWarned=0;
uint32_t warningSoundAt=0;

// StopWatch CO5300 framebuffer can exist and report dimensions even when
// the physical AMOLED remains blank.  Force a *real* full-frame transfer.
// Only public M5GFX interfaces are used; never touch display power or
// reset GPIOs, never call M5.begin() again.
void refreshPanel(const char* reason) {
  auto& lcd = M5.Display;
  if (lcd.width() < 400 || lcd.height() < 400) return;
  lcd.waitDisplay();
  lcd.wakeup();                       // M5GFX-managed wake-up request
  lcd.setBrightness(240);
  lcd.display(0, 0, lcd.width(), lcd.height());
  lcd.waitDisplay();
  ++displayRecoveryCount;
  Serial.printf("[GapM5 R8] display refresh #%u: %s\n",
                displayRecoveryCount, reason);
}

void firstDisplay() {
  auto& lcd=M5.Display;
  const int w=lcd.width(),h=lcd.height();
  // A conspicuous first-frame boot banner helps distinguish a black panel
  // from a crash or a failed Wi-Fi/audio start. No panel GPIO tweaking.
  lcd.fillScreen(TFT_BLUE);
  lcd.fillRect(0,90,w,h-180,TFT_BLACK);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_WHITE,TFT_BLACK);
  lcd.drawString("GapM5 R7",w/2,h/2-52);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0x5BFF,TFT_BLACK);
  lcd.drawString("RADIAL CO-OP SHOOTER",w/2,h/2-10);
  lcd.drawString("ESP-NOW / STOPWATCH",w/2,h/2+40);
  lcd.drawCircle(w/2,h/2,((w<h?w:h)/2)-7,0x5BFF);
}
int8_t localMove(bool left,bool right){
  // Starting at the bottom: A/left moves clockwise (=screen-left), B moves CCW.
  if(left==right)return 0;
  return left?1:-1;
}
}

void setup() {
  Serial.begin(115200);
  delay(750);  // Give PMIC/IO expander an extra settling interval.
  const esp_partition_t* runningApp=esp_ota_get_running_partition();
  Serial.printf("[GapM5 R8] APP %s @ 0x%lx\n",
                runningApp?runningApp->label:"?",
                static_cast<unsigned long>(runningApp?runningApp->address:0));
  auto cfg=M5.config();
  cfg.serial_baudrate=115200;
  cfg.clear_display=true;
  cfg.fallback_board=m5::board_t::board_M5StopWatch;
  cfg.internal_imu=false;
  cfg.internal_rtc=false;
  // Register StopWatch codec pins/callback via M5Unified, but begin audio
  // playback only after the first AMOLED frame is already visible.
  cfg.internal_spk=true;
  cfg.internal_mic=false;
  cfg.external_imu=false;
  cfg.external_rtc=false;
  Serial.println("[GapM5 R7 BOOT] before M5.begin");
  M5.begin(cfg);   // The single owner of power/panel initialization.
  Serial.println("[GapM5 R7 BOOT] M5.begin returned");
  M5.Display.setRotation(0);
  M5.Display.setBrightness(200);
  const int w=M5.Display.width(),h=M5.Display.height();
  Serial.printf("[GapM5 R7 BOOT] board=%d LCD=%dx%d PSRAM=%lu Flash=%lu\n",
                int(M5.getBoard()),w,h,
                static_cast<unsigned long>(ESP.getPsramSize()),
                static_cast<unsigned long>(ESP.getFlashChipSize()));
  if(w<400 || h<400){
    Serial.println("[GapM5 R7 BOOT] invalid display: safe stop");
    return;
  }
  firstDisplay();  // Must happen before radio/audio/PSRAM assets.
  // Explicit full framebuffer transfer, not just drawing into PSRAM.
  refreshPanel("first draw");
  delay(180);
  refreshPanel("settled");
  lcdReady=true;
  Serial.println("[GapM5 R7 BOOT] blue boot frame drawn");
  delay(550);
  // M5Unified handles vibration through the StopWatch power driver.
  M5.Power.setVibration(0);
  const bool networkOkay=radioLink.begin();
  Serial.printf("[GapM5 R8] ESP-NOW %s; HELLO channel=1\n",networkOkay?"ready":"failed");
  bgm.begin();
  refreshPanel("after speaker");
  Serial.printf("[GapM5 R8] speaker %s\n",bgm.playing()?"ready":"muted");
  lastLogic=millis();lastDraw=lastLogic;introStarted=lastLogic;
}

void loop() {
  if(!lcdReady){delay(250);return;}
  M5.update();
  const uint32_t now=millis();
  bool a=M5.BtnA.isPressed();
  bool b=M5.BtnB.isPressed();
  bool both=a&&b;
  // Black-screen recovery can be invoked blindly: hold BOTH side buttons
  // for >=3 seconds. Works even if both displays are currently dark.
  // We do not change any saved game/OTA data or ESP-NOW peer state.
  if (both) {
    if (!recoveryHeldSince) recoveryHeldSince = now;
    if (recoveryArmed && now-recoveryHeldSince >= 3000) {
      recoveryArmed = false;
      refreshPanel("manual A+B");
    }
  } else {
    recoveryHeldSince = 0;
    recoveryArmed = true;
  }
  const int8_t direction=localMove(a,b);
  const bool rightPressed=M5.BtnB.wasPressed();
  const bool startable=session::startable(game.phase);
  // Latch even a brief GUEST press for 400ms, retransmitting across
  // ESP-NOW's 32ms input interval. HOST always authoritatively starts 2P.
  if(rightPressed && !a && startable)guestStartSendUntil=now+400u;
  const bool requestStart=startable && !a &&
      (b || (guestStartSendUntil &&
             static_cast<int32_t>(now-guestStartSendUntil)<0));
  radioLink.poll(now,game,direction,requestStart);
  const bool havePeer=radioLink.hasPartner();
  const bool isHost=radioLink.isHost();
  const bool connected=radioLink.online(now);
  bool isSolo=radioLink.isSolo();
  const bool remoteStart=radioLink.consumeRemoteStart();
  if(startable) {
    const session::Action action=session::onRight(rightPressed && !a,
                                                  havePeer && connected,isHost,game.phase);
    if(action==session::START_SOLO) {
      // Pairing is optional: right button leaves ESP-NOW WAIT for SOLO.
      radioLink.startSolo(game,now);
      isSolo=true;
      observedEvent=0;
    } else if(action==session::START_COOP ||
              (havePeer && connected && isHost && remoteStart)) {
      gm::start(game,false,now^0x6d5345u);
      observedEvent=0;
      Serial.println("[GapM5 R8] CO-OP START / right button");
    }
  }
  // Ten seconds of no START opens a non-interactive attract demo whether
  // a partner is present or not. Guest displays HOST's scripted snapshots.
  if(session::shouldAutoDemo(game.phase,introStarted,now,
                             !radioLink.hasPartner() || radioLink.isHost())) {
    gm::beginDemo(game,!radioLink.hasPartner(),now^0xDE065u);
    observedEvent=0;
    Serial.println("[GapM5 R8] DEMO starts automatically at 10 seconds");
  }
  isSolo=radioLink.isSolo();
  if(now-lastLogic>=16){
    const uint32_t step=now-lastLogic;
    lastLogic=now;
    if(isSolo || !radioLink.hasPartner() || radioLink.isHost()) {
      game.paused=(radioLink.hasPartner() && !isSolo && !connected &&
                   (game.phase==gm::PLAY || game.phase==gm::CLEAR));
      gm::tick(game,step*.001f,direction,radioLink.remoteDirection(now));
    } else {
      game.paused=!connected && (game.phase==gm::PLAY || game.phase==gm::CLEAR);
    }
    if(game.phase!=gm::WAIT && game.event!=observedEvent){
      observedEvent=game.event;
      bgm.event(game.eventType);
      M5.Power.setVibration(game.eventType==3?165:90);
      vibrateUntil=now+(game.eventType==3?230:100);
    }
  }
  if(vibrateUntil && static_cast<int32_t>(now-vibrateUntil)>=0){
    M5.Power.setVibration(0);
    vibrateUntil=0;
  }
  // Warn once per enemy when it commits to an inward dive. The receiver
  // uses the same mode from ESP-NOW so both watches receive the cue.
  uint16_t warningMask=0;
  for(int i=0;i<gm::MAX_ENEMY;++i)
    if(game.enemies[i].mode==gm::E_WARNING) warningMask |= uint16_t(1u<<i);
  if((warningMask & ~previouslyWarned) &&
     (warningSoundAt==0 || now-warningSoundAt>=220)) {
    bgm.event(4);
    warningSoundAt=now;
  }
  previouslyWarned=warningMask;
  bgm.update(now,game);
  // A handful of delayed refreshes to recover from late panel wake-up.
  // No repeated panel resets and no display-flicker test screen.
  if (displayRecoveryCount < 6 && now >= 2600u &&
      static_cast<int32_t>(now - lastPanelRetry) >= 2000) {
    lastPanelRetry = now;
    refreshPanel("delayed automatic");
  }
  if(now-lastDraw>=33){
    lastDraw=now;
    const uint32_t elapsed=(now-introStarted)/1000u;
    const uint8_t remaining=elapsed>=10?0:static_cast<uint8_t>(10u-elapsed);
    screen::draw(game,radioLink.identity(),radioLink.hasPartner(),
                 radioLink.online(now),radioLink.ready(),radioLink.isSolo(),remaining);
  }
  delay(1);
}
