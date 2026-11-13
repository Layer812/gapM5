// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
#include <M5Unified.h>
namespace screen {
void draw(const gm::State& state, const char* role, bool paired, bool online, bool wifiOK, bool solo, uint8_t demoCountdown);
}
