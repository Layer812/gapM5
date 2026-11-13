<div align="center">

![GapM5 — English arcade instruction card](assets/GapM5_Instruction_EN_2026.png)

# GapM5 — ギャップエムファイブ

**An original circular co-op rescue shooter for M5Stack StopWatch**

[日本語](README_JA.md) · [License](LICENSE) · [Artwork and music](ASSET_LICENSE.md)

</div>

## About GapM5

Two spacecraft share **one circular orbit**. Move freely to the left or right,
but while both ships are active, **neither can overtake the other**. Fire
continuously toward the center as waves of enemies emerge along straight and
curved paths, vanish offscreen and return for another attack.

An enemy's tractor beam can capture a player. The other pilot can shoot down
the captor to rescue their partner; **both ships receive a temporary power-up**.
Clear each wave by defeating the enemies or surviving their repeated attacks.

## Play

| Situation | Right button |
|---|---|
| Waiting for a partner | Start single-player |
| Connected over ESP-NOW | Start two-player co-op |
| Attract demo | Start playing |
| Game over | Play again |

- **Left / right buttons:** travel in either direction along the orbit
- **Shooting:** automatic, toward the center
- **After 10 seconds without starting:** a solo or synchronized co-op demo begins automatically
- **Capture and rescue:** the untrapped pilot can free their partner by destroying the captor
- **Demo mode:** shows co-op movement, capture, rescue, and teamwork without vibration

No Wi-Fi router or internet connection is required for two-player ESP-NOW play.

## Build (Windows / PlatformIO)

For M5Stack StopWatch (ESP32-S3, 16 MB flash, 8 MB PSRAM):

```bat
RUN_BUILD_ONLY.cmd
RUN_BUILD_FLASH_APP_ONLY.cmd COM13
```

Replace `COM13` with your device's actual serial port. The supplied flashing
script writes **the selected application partition only**; it does not erase
saved storage, bootloader, partition table or OTA settings. Never run a full
flash erase to install this game.

## Rights and credits

- **Original source code:** [MIT License](LICENSE), Copyright © 2026 LAYER8
- **Artwork, posters, and original music:** © 2026 LAYER8, all rights reserved; see [asset terms](ASSET_LICENSE.md)
- **Third-party libraries:** subject to their own licenses; see [notices](THIRD_PARTY_NOTICES.md)

GapM5 is an independent original game, not affiliated with or endorsed by any
arcade game publisher.
