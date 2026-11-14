# 試作用UF2

1. `satellite-display.uf2`: バッテリー重視の画面と模擬値を確認（BLE停止）。レイヤーと接続状態を3秒ごとに模擬します。
2. `satellite-ble.uf2`: USBログで実際のProspector互換広告を確認（LCD停止）。
3. `satellite-status.uf2`: 統合表示。

標準XIAO nRF52840 UF2/S140 v7ブートローダーレイアウト、アプリ0x27000。
SPI mode 3を標準設定にしています。試作実機でBLE受信とLCD描画を確認済みですが、表示品質・統合動作は検証中です。詳細は上位のREADMEとdocs/HARDWARE.mdを参照してください。

`manifest.json` にビルドサイズとSHA-256、`SHA256SUMS.txt` に照合用一覧を収録。
UF2だけを再配布するときも上位のLICENSE、THIRD_PARTY_NOTICES.md、licenses/を同梱してください。

表示版・統合版は、上段にキーボード名とレイヤー番号、中央に左右バッテリー、下段にBT1〜BT5／USBと接続状態を表示します。レイヤー名は表示しません。BT状態は接続中／ペアリング済み未接続／未ペアリングを区別します。USBはオレンジ色です。
