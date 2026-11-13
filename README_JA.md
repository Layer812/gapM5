<div align="center">

![GapM5 — 日本語インストカード](assets/GapM5_Instruction_JP_2026.png)

# GapM5 — ギャップエムファイブ

**M5Stack StopWatch 2台で協力する、円周移動型レスキューシューティング**

[English README](README.md) · [ソースのライセンス](LICENSE) · [画像・音源の権利](ASSET_LICENSE.md)

</div>

## ゲーム紹介

2機は**同じ円周軌道を左右どちらにも自由に移動**します。ただし、2機が正常な間は
お互いを追い越せません。弾は円の中心へ向けて自動射撃します。
敵は中心から直線や二次曲線を描いて外へ飛び、画面の外に抜けてから再突入することがあります。
捕獲光線で片方が動けなくなったら、もう片方が捕獲機を撃破して救出できます。
救出後は2機とも一定時間パワーアップします。

## 操作とデモ

| 状態 | 右物理ボタン |
|:--|:--|
| ESP-NOW 接続待機中 | 待機を解除して1人用スタート |
| ESP-NOWで接続済み | どちらかが押せば2人用スタート |
| デモ再生中 | 新しいゲームを開始 |
| ゲームオーバー | 最初から再開 |

- **プレイ中の左右ボタン：** 円周移動
- **10秒間操作しない：** 1人用／接続時は同期2人用のデモ開始
- **左右同時3秒：** AMOLEDの画面再転送（黒画面復帰）
- **射撃：** 中央に向けて自動連射

**2台連携のデモ速度と演出をそろえるには両方をR8に更新してください。**
R4で実機復旧に成功した画面復帰処理と、HOSTがゲーム状態を計算してGUESTへ送る
ESP-NOW v2（235バイト）方式は維持しています。

## ビルドと書き込み（Windows / PlatformIO）

対象：M5Stack StopWatch、ESP32-S3R8 / Flash 16MB / PSRAM 8MB。
PlatformIO Espressif32 v6.12.0、M5Unified/M5GFX v0.3.0、`BOARD_ID=30`。

```bat
RUN_BUILD_ONLY.cmd
RUN_BUILD_FLASH_APP_ONLY.cmd COM13
```

`COM13` は実際の機体のCOM番号に置き換えてください。
スクリプトは実機のパーティションと起動APPを読み、**現行起動APPだけに書き込み**ます。
bootloader / partition table / otadata / NVS / SPIFFSは書き換えず、全消去は行いません。

## 動作確認の状況

- **R4：** 2台の実機で画面表示・ESP-NOW接続の成功報告あり。
- **R7/R8：** PC上のC++ゲームロジックテストPASS（二次曲線、捕獲と救出、同期デモ等）。
- **R8：** ESP32-S3向けの実ビルド・2台実機でのR8プレイは、ここでは**未確認**。

## ライセンス・権利表示

**オリジナルソースコード：MIT License / Copyright © 2026 LAYER8**
（[LICENSE](LICENSE)）。
**インストカード・画像・音楽：© 2026 LAYER8 / All rights reserved**
（[ASSET_LICENSE.md](ASSET_LICENSE.md)）。
外部依存ライブラリにはそれぞれのライセンスがあります
（[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)）。

GapM5はオリジナル作品です。過去のアーケードゲームの雰囲気を参考にしていますが、
特定のゲームメーカーとの提携・公認関係はありません。
