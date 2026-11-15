# ZMK Satellite

## 概要

ZMKキーボードの状態を表示する、XIAO nRF52840＋240×240 ST7789液晶用のファームウェアです。  
キーボード名、リング型バッテリー表示、レイヤー番号、BT1〜BT5／USBと接続状態を表示します。一体型は中央に1つ、左右分離型は左右に2つのリングを自動で表示します。

キーボードとのペアリングは不要で、ZMKファームウェア上で機能をONにするだけで利用できます。

## 導入方法

GitHub Actionsの成果物は、通常版 `satellite`、表示デモ `satellite_demo`、BLE診断版 `satellite_debug` です。  
各ZIPを展開すると同名のUF2ファイルが入っています。
表示デモは25秒ごとに左右分離型と一体型を切り替えます。

### 1. 表示器を組み立てる

USBを外して配線してください。  
対象はCSピンのない7ピン・1.3インチST7789液晶です。

| LCD | XIAO nRF52840 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | D8 / SCK |
| SDA | D10 / MOSI |
| RES | D2 |
| DC | D3 |
| BLK | 未接続 |

XIAOをUSB接続し、リセットを素早く2回操作して表示されるドライブへ、[UF2ファイル](firmware/satellite.uf2)をコピーします。  

液晶に情報が表示されることを確認します。
※表示設定は「SPI mode 3・RAM制御00 f8」を使用しています。使用する液晶によっては調整が必要な場合があります。

### 2. キーボード側を有効にする

使用したいZMKキーボードの `config/west.yml` の `manifest.projects` に追加します。  

```yaml
    - name: zmk-satellite
      url: https://github.com/alektor-diy/zmk-satellite
      revision: main
      path: modules/zmk-satellite
```

適用される共通 `.conf` に1行追加します。

```conf
CONFIG_ZMK_SATELLITE=y
```

普段どおりキーボード用FWをビルドして書き込みます。  
分割キーボードは左右とも更新してください。

### 3. 状態を表示する
書き込み後、表示機に情報が表示されることを確認します。  

なおZMK SATELLITEを有効化したキーボードで新規にデバイスとペアリングする際、候補にキーボードが表示されない場合があります。  
この場合は接続先機器のBluetoothを一度OFFにしてから、再度ONにしてください。

またキーボード側の広告送信により電池消費が増える可能性があります。


受信できない場合は[BLE診断用UF2](firmware/satellite_debug.uf2)のUSBシリアルログ（115200bps）で確認してデバッグしてください。。

## ライセンスと謝辞

本プロジェクトは[MITライセンス](LICENSE)です。依存物のライセンス・著作権表示は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)と[licenses/](licenses/)に収録しています。バイナリ配布時も同梱してください。

[Prospector（carrefinho氏）](https://github.com/carrefinho/prospector)、[Prospector Scanner（t-ogura氏）](https://github.com/t-ogura/zmk-config-prospector)、[Prospector ZMK Module](https://github.com/t-ogura/prospector-zmk-module)を参考にしました。構想と実装資料を公開された皆様に感謝します。
