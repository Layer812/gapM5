// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LAYER8.
#pragma once
#include "game_core.hpp"
namespace feedback {
inline bool allowVibration(uint8_t phase) {
  return phase!=gm::DEMO;
}
}
