# 仮配線と実機検証

配線表はREADMEを参照。LCDはユーザー提示の画像上の7ピン仕様が前提で、販売URL・型番・基板裏面は未確認です。別モジュールのBLKを無条件に未接続としないでください。

## CSなしの設定根拠

固定したZephyr 4.1.0では `sitronix,st7789v` はMIPI DBI SPI経由です。`MIPI_DBI_SPI_CONFIG_DT`（`include/zephyr/drivers/mipi_dbi.h`）はSPIの `cs-gpios` がないと空GPIO specを生成します。nRF SPIMはSSピンを `NRF_SPIM_PIN_NOT_CONNECTED` に設定し、CS GPIOがない場合はGPIO制御を行いません。したがって**ダミーのCSをDピンへ割り当てる必要はありません**。

`reg=<0>` はドライバの論理slave番号で、物理CSピンを作る指定ではありません。MIPIの「4WIRE」はSCK/MOSI/CS/DCの分類名ですが、CSはLCD内部で選択されている前提です。CSが外にないモジュールはSPIバスを共有できません。

SPI2は初期4MHz、MOSIのみ。D9/MISOは使わず、UART0/I2C1/QSPIも無効化しています。短い配線・GND共通で検証してください。P0.28/P0.29は電源ピンではなくRES/DCです。

## 1. 表示だけを確認

1. 電源を外して導通・VCC/GND・Dピン表記を確認。
2. display UF2を書き込み。外周の1px枠が四辺とも見えること、下部の赤・緑・青バーの色順と模擬値を確認。
3. 5秒ごとのレイヤー番号更新、15秒以降の広告停止模擬、最後の模擬更新から5秒後のstale表示、25秒付近での復帰を確認。
4. USB充電器でも開始するか確認。PCのシリアル接続待ちはありません。

白画面はまずRES/DC逆・SCL/SDA逆・3V3・GND・ブートログを確認。BL点灯だけではLCD通信成功を意味しません。切り分けは次の一項目ずつ行います。

- **完全に白／無反応**: CSなしST7789のモジュール差でSPI mode 3が必要な場合があります。`config/mode3.overlay` を追加。
- **80行程度の欠け／ずれ**: 240×320 RAMのうちどこを240×240として使うかに差があります。`config/offset80.overlay` を追加して比較。
- **赤青が逆**: `mdac` のBGR bit（0x08）やST7789VのRGB/BGR pixel-format設定を確認。まずRGBバーで判定。
- **ネガ反転色**: `inversion-off;` をLCDノードへ追加して比較。
- **上下反転／回転**: `mdac` のMX/MY/MVとx/y-offsetをセットで調整。回転とオフセットは基板が確定してから固定。

追加overlayの例（既定のboard overlayも明示して保持）:

```sh
west build -p always -b xiao_ble/nrf52840 zmk-satellite -d build-display -- -DSATELLITE_STAGE=display '-DDTC_OVERLAY_FILE=boards/xiao_ble.overlay;config/mode3.overlay'
```

USBシリアルログの読取例: 端末ソフトを115200bpsで接続。FWはDTRを待ちません。起動ログを逃した場合は端末を開いてからリセットします。

## 2. BLEだけを確認

キーボード側にProspector v2.2.3を組込み、ble UF2でUSBログを見る。ID・layer・BTプロファイル・flags・左右残量が操作に対応するか確認。右centralでも左右を入れ替えていないことを確認。PCへのBT接続を維持したまま受信できることを確認します。

近くに複数の対応キーボードがあれば先にIDを固定。BLEデバイス名は本体の選択キーとして使いません。広告は暗号化・認証されていません。

## 3. 統合・耐久確認

- status UF2でdisplay/bleの結果と一致すること。
- BTプロファイルを切り替えても同じkeyboard IDを追うこと。
- PCのBTだけ切断した場合 `BT offline`、キーボード電源断では95秒後 `Signal lost / stale` となること。
- 片側電源断と両側電源断を別々に試す。central広告が続く場合のperipheral残量の鮮度には制約あり。
- 数分idle後も95秒タイムアウトにならないこと。idle間隔を変更した場合はタイムアウトも再設定。
- 同じキーボードを再投入し表示が自動復帰すること。
- USB再接続、電源バンク、数時間連続動作、他のBLE機器が多い環境で確認。
- 電流・温度・XIAOの3V3レギュレーター余裕、USBコネクタの機械的保持を実測。

## 頒布前に確定

LCD販売元・型番・基板寸法・固定穴・BL回路・SPI mode・オフセット・色設定、ブートローダー版、キーボード側の対応ZMK版、長時間試験、USB VID/PID、名称／商標の確認。PCBとケースはこの結果に合わせて設計します。FWのビルド合格は実機動作合格を代替しません。
