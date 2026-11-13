// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
// GapM5 R6 - audible lobby + original syncopated techno-pop sequencer (not a transcription).
// E minor / D major jazz-voiced 8-bar progression, octave bass, short stabs.
#include "music.hpp"
#include "music_score.hpp"
#include <M5Unified.h>
#include <cmath>
namespace music {
namespace {
float freq(uint8_t midi) {
  return 440.0f * powf(2.0f, (static_cast<int>(midi)-69) / 12.0f);
}
}
void Player::begin() {
  nextBeat=0; step=0; lastPhase=gm::WAIT;
  available=M5.Speaker.begin();
  Serial.printf("[GapM5 R6 AUDIO] Speaker.begin=%s\n",available?"OK":"FAILED");
  if(available) {
    // R2 used master volume 40/255 and played nothing while waiting for 2P.
    // Keep a moderate, clearly audible master gain; no direct codec GPIO writes.
    M5.Speaker.setVolume(108);
    // M5Speaker channels 0..7: lead, bass, chord trio, FX, kick, snare/hat.
    const uint8_t vol[8]={210,200,112,108,108,205,150,130};
    for(uint8_t c=0;c<8;++c)M5.Speaker.setChannelVolume(c,vol[c]);
    const bool bootTone=M5.Speaker.tone(784.0f,160,0,true);
    Serial.printf("[GapM5 R6 AUDIO] startup chime=%s\n",bootTone?"queued":"FAILED");
  }
}
void Player::event(uint8_t type) {
  if(!available)return;
  if(type==3)M5.Speaker.tone(1047,230,5,true);      // rescue
  else if(type==2)M5.Speaker.tone(245,200,5,true); // capture
  else if(type==1)M5.Speaker.tone(141,140,5,true); // hit
  else if(type==4)M5.Speaker.tone(880,90,5,true); // inward enemy warning
  else if(type==5){ // stage clear, original three-note synth fanfare
    M5.Speaker.tone(659,210,5,true);
    M5.Speaker.tone(784,220,3,true);
    M5.Speaker.tone(988,230,4,true);
  }
}
void Player::update(uint32_t ms,const gm::State& s) {
  if(!available)return;
  if(s.phase==gm::OVER) {
    lastPhase=s.phase;nextBeat=ms;step=0;return;
  }
  if(s.paused)return;
  if(s.phase!=lastPhase) {
    lastPhase=s.phase;
    nextBeat=ms;
    step=0;
    // WAIT is no longer silent. Sound is softer while finding partner.
    M5.Speaker.setVolume((s.phase==gm::PLAY || s.phase==gm::DEMO || s.phase==gm::CLEAR)?108:78);
    Serial.printf("[GapM5 R6 AUDIO] phase=%d / music active\n",int(s.phase));
  }
  if(static_cast<int32_t>(ms-nextBeat)<0)return;
  const uint32_t tempo=(s.wave>=8)?106u:((s.wave>=4)?110u:music_score::STEP_MS);
  nextBeat+=tempo;
  // Do not burst-generate late notes when LCD/wireless momentarily blocks.
  if(static_cast<int32_t>(ms-nextBeat)>static_cast<int32_t>(tempo))nextBeat=ms+tempo;
  if(s.phase==gm::COUNTDOWN) {
    if(step%4==0)M5.Speaker.tone(523.0f,82,0,true);
    ++step; return;
  }
  const int bar=(step/16)%8;
  const int pos=step%16;
  using namespace music_score;
  if(LEAD[bar][pos] && (s.phase!=gm::WAIT || pos%4==0)) {
    M5.Speaker.tone(freq(LEAD[bar][pos]),uint32_t(tempo*.83f),0,true);
  }
  const int8_t note=BASS_PATTERN[pos];
  if(note>=0) {
    const uint8_t pitch=ROOT[bar]+((note==10)?SEVENTH[bar]:note);
    M5.Speaker.tone(freq(pitch),uint32_t(tempo*1.36f),1,true);
  }
  // Stabs on the 'and' beat; full 3-note voicings, not pentatonic runs.
  if((pos==2 || pos==10 || (s.wave>=5 && pos==14)) &&
     s.phase!=gm::WAIT) {
    for(int c=0;c<3;++c)M5.Speaker.tone(freq(CHORDS[bar][c]),uint32_t(tempo*.99f),2+c,true);
  }
  if(pos%4==0)M5.Speaker.tone(72,uint32_t(tempo*.33f),6,true); // kick pulse
  if(pos==4 || pos==12)M5.Speaker.tone(186,uint32_t(tempo*.31f),7,true);
  else if(pos==2 || pos==6 || pos==10 || pos==14)
    M5.Speaker.tone(3500,uint32_t(tempo*.12f),7,true); // short hat-like tick
  ++step;
}
}
