# ZMK Satellite

## これはなにか

ZMKキーボードの状態を表示する、XIAO nRF52840＋240×240 ST7789液晶用のファームウェアです。キーボード名、左右のバッテリー、レイヤー番号、BT1〜BT5／USBと接続状態を表示します。

Prospector互換のBLE広告を受信するため、キーボードとのペアリングは不要です。USB-Cで給電します。

## 導入方法

### 1. 表示器を組み立てる

USBを外して配線してください。対象はCSピンのない7ピン・1.3インチST7789液晶です。

| LCD | XIAO nRF52840 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | D8 / SCK |
| SDA | D10 / MOSI |
| RES | D2 |
| DC | D3 |
| BLK | 未接続 |

XIAOをUSB接続し、リセットを素早く2回操作して表示されるドライブへ、[表示テスト用UF2](firmware/satellite-display.uf2)をコピーします。表示される残量は模擬値です。

試作実機で正常表示を確認した設定（SPI mode 3・RAM制御00 f8）を使用しています。異なる液晶では調整が必要な場合があります。

### 2. キーボード側を有効にする

自分のZMK設定リポジトリの `config/west.yml` の `manifest.projects` に追加します。既存の設定は残してください。

```yaml
    - name: zmk-satellite
      url: https://github.com/alektor-diy/zmk-satellite
      revision: main
      path: modules/zmk-satellite
      import: module/west.yml
```

適用される共通 `.conf` に1行追加します。

```conf
CONFIG_ZMK_SATELLITE=y
```

普段どおりキーボード用FWをビルドして書き込みます。分割キーボードは左右とも更新してください。

### 3. 状態を表示する

表示器へ[統合版UF2](firmware/satellite-status.uf2)を書き込み、キーボードを起動します。**Satellite用UF2はキーボードへ書き込まないでください。**

受信できない場合は[BLE診断用UF2](firmware/satellite-ble.uf2)のUSBシリアルログ（115200bps）で確認できます。

BTは接続中・ペアリング済み未接続・未ペアリングを区別します。95秒広告が届かないとSIGNAL LOSTになります。BTとUSBの両方が接続されている場合、実際の出力先は広告から判定できません。

なおキーボード側の広告送信により電池消費が増える可能性があります。

## ライセンスと謝辞

本プロジェクトは[MITライセンス](LICENSE)です。依存物のライセンス・著作権表示は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)と[licenses/](licenses/)に収録しています。バイナリ配布時も同梱してください。

[Prospector（carrefinho氏）](https://github.com/carrefinho/prospector)、[Prospector Scanner（t-ogura氏）](https://github.com/t-ogura/zmk-config-prospector)、[Prospector ZMK Module](https://github.com/t-ogura/prospector-zmk-module)を参考にしました。構想と実装資料を公開された皆様に感謝します。 
