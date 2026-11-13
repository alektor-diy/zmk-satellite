# 試作用UF2

1. `satellite-view-display.uf2`: 最初にこれでLCDと配線を確認（模擬値、BLE停止）。
2. `satellite-view-ble.uf2`: USBログで実際のProspector互換広告を確認（LCD停止）。
3. `satellite-view-status.uf2`: 統合表示。

標準XIAO nRF52840 UF2/S140 v7ブートローダーレイアウト、アプリ0x27000。
実機未検証です。詳細は上位のREADMEとdocs/HARDWARE.mdを参照してください。

`manifest.json` にビルドサイズとSHA-256、`SHA256SUMS.txt` に照合用一覧を収録。
UF2だけを再配布するときも上位のLICENSE、THIRD_PARTY_NOTICES.md、licenses/を同梱してください。
