# ZMK Satellite

XIAO nRF52840 と 1.3インチ 240×240 ST7789 SPI LCD を使う、ZMKキーボード状態表示専用の独立ファームウェアです。ZMKアプリを流用せず **Zephyr 4.1.0 + LVGL 9** で構成しています。Prospector互換広告を受信し、接続枠を消費するGATT接続は行いません。アクティブスキャンのSCAN_REQは送信します。

**試作版 v0.1.0。実機未検証。** ビルド結果は [検証記録](docs/VALIDATION.md) を参照してください。最初は付属の表示テスト用UF2から確認してください。

## 実装した機能

- 3段階のビルド: `display`（模擬表示）、`ble`（USBログのみ）、`status`（統合）。
- Manufacturer Dataの26バイト形式を境界検査して解析。ADVとSCAN_RSPの両方を受信。
- 左右バッテリー、レイヤー番号と最大4文字の名前、BTプロファイル1〜5、BT接続とUSB HID ready。
- 広告が95秒届かなければ `Signal lost / stale`。保持した値には経過時間を表示。
- 最初に受信したkeyboard IDを保持。同じIDならアドレス変更・タイムアウト後も復帰。他のキーボードの値を混ぜません。固定ID／チャンネルもビルド時に指定可能。
- BLE RX側では固定長キューにコピーし、メインスレッドだけが状態とLVGLを操作。
- USB-C給電のみ。タッチ・追加ボタン・自機バッテリー・輝度制御は使用しません。

## 配線

USBを外して配線し、**VCCは3V3** に接続します。LCDのSCL/SDAという表記はこのモジュールではSPIクロック／MOSIです。

| LCD 7ピン | XIAO | nRF52840 | 用途 |
|---|---|---|---|
| GND | GND | — | 共通GND |
| VCC | 3V3 | — | 3.3V電源 |
| SCL | D8 / SCK | P1.13 | SPI2クロック |
| SDA | D10 / MOSI | P1.15 | SPI2送信 |
| RES | D2 | P0.28 | リセット、Active low |
| DC | D3 | P0.29 | Command low / Data high |
| BLK | 未接続 | — | 提示された画像上では未接続で点灯 |

CSとMISOの接続はありません。SPI2はこのLCD専用です。XIAOの **Zephyr標準ボード `xiao_ble/nrf52840`** を使用し、ZMK固有の `/zmk` variantは使いません。対応ピンは固定バージョンの `seeed_xiao_connector.dtsi` で確認済みです。

LCDの販売URL・正確な型番は未提示です。BLKの動作、実際のCS内部固定、オフセット、色順、リセット動作は実機で確定してください。[配線と画面調整](docs/HARDWARE.md) に手順があります。

## すぐ試す

配布フォルダ `firmware/` に3種類のUF2をまとめます。XIAOのリセットパッドを素早く2回操作してUF2ブートローダードライブを表示し、次の順番でコピーします（ドライブ名はブートローダーにより異なります）。

1. `satellite-display.uf2`: BLEなし。外周枠、RGBバー、模擬76%／65%、5秒ごとに変わるレイヤー番号を確認。25秒周期で受信途絶も模擬します。**実際の受信値ではありません。**
2. `satellite-ble.uf2`: LCDなしでも動作。USBシリアル115200で `ID=... L=... R=... layer=... BT=...` を確認。キーボード側の対応が必要です。
3. `satellite-status.uf2`: 実際の広告を表示。標準で受信途絶95秒後にstaleへ移行。同じキーボードが戻れば復帰。

FWはUSBホスト／DTRを待ちません。USB充電器でも表示を開始します。UF2は標準Seeed/Adafruit系 **S140 v7レイアウト、アプリ開始0x27000** のブートローダーを想定します。既存のブートローダーやSoftDevice領域を消去しないでください。ZephyrのBLEコントローラーを使用し、SoftDevice APIにはリンクしません。

## キーボード側の準備

標準ZMK広告だけでは表示できません。キーボードのwest manifestに次のプロジェクトを追加し、元のZMK設定に合った手順で再ビルドしてください。**スキャナー本体のmanifestには追加しません。**

```yaml
# manifest.projects に追加
- name: prospector-zmk-module
  url: https://github.com/t-ogura/prospector-zmk-module
  revision: 72a71aeea816eb0f86ffc5b108b267076ce5f0af # v2.2.3
  path: modules/prospector-zmk-module
```

```conf
# centralまたは単体キーボード側
CONFIG_ZMK_STATUS_ADVERTISEMENT=y
# 任意: この設定はBLEデバイス名も変更します
# CONFIG_ZMK_STATUS_ADV_KEYBOARD_NAME="MyKeyboard"
# CONFIG_PROSPECTOR_CHANNEL=3
```

分割centralには、必要に応じて以下を追加します。peripheral側へ状態広告を無理に有効化しないでください。

```conf
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
CONFIG_ZMK_STATUS_ADV_CENTRAL_SIDE="RIGHT" # centralが左ならLEFT
# CONFIG_PROSPECTOR_EXPECTED_PERIPHERAL_COUNT=1
```

peripheral側はZMKのバージョン／既存設定に合わせ、バッテリー報告を有効にしたFWが必要です。`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY` はPCへバッテリーサービスを公開する用途で、本スキャナーには必須ではありません。AUX centralや複数peripheralの場合は [通信仕様](docs/PROTOCOL.md) と上流Kconfigに従ってマッピング・期待台数を設定してください。ユーザーのキーボード設定自体は未取得・未変更です。

## ソースからビルド

Zephyrの開発環境（Python 3.10以上、Git、CMake 3.20以上、Ninja、DTC、Zephyr SDK 0.17.0）を準備します。依存ソースはmanifestで固定しています。Ubuntu／WSLの例:

```sh
mkdir satellite-workspace && cd satellite-workspace
git clone <この非公開リポジトリのURL> zmk-satellite
python3 -m venv .venv
. .venv/bin/activate
pip install west 'cmake<4' ninja
west init -l zmk-satellite
west update
pip install -r zephyr/scripts/requirements-base.txt
west zephyr-export
west sdk install --version 0.17.0 --toolchains arm-zephyr-eabi
west build -b xiao_ble/nrf52840 zmk-satellite -d build-display -- -DSATELLITE_STAGE=display
west build -b xiao_ble/nrf52840 zmk-satellite -d build-ble -- -DSATELLITE_STAGE=ble
west build -b xiao_ble/nrf52840 zmk-satellite -d build-status -- -DSATELLITE_STAGE=status
```

成果物は各 `build-*/zephyr/zephyr.uf2`。Windowsネイティブでは `scripts/build.ps1` も使えます。ビルドモードやoverlayを変更するときは新しいbuildディレクトリ、または `west build -p always` を使ってください。自動ビルド用のGitHub Actionsも同梱していますが、GitHubへ登録して実行するまでCIは未検証です。

対象を固定する場合は、例えば次のファイルを作って追加します:

```conf
CONFIG_SATELLITE_KEYBOARD_ID="12345678" # スキャナーのログに出る順序
CONFIG_SATELLITE_CHANNEL=3
CONFIG_SATELLITE_TIMEOUT_MS=95000
```

```sh
west build -p always -b xiao_ble/nrf52840 zmk-satellite -d build-status -- -DSATELLITE_STAGE=status -DEXTRA_CONF_FILE=/absolute/path/to/local.conf
```

チャンネルは上流と同じく **0または10以上は全受信、1〜9は同じ値か送信側0を受信** です。10以上を専用ペア番号と考えないでください。ID指定が最も確実です。ID未指定時に違うキーボードが選ばれた場合は再起動、またはID固定して再ビルドします。ペアリング・認証の機能ではありません。

## 制約と後で確認する事項

- 実機の画面・BLE・長時間安定性・消費電流は未検証。表示の白飛び／欠け／色違いは [HARDWARE.md](docs/HARDWARE.md) で切り分け。
- 26バイト形式のProspector module-major 2を対象。旧version 1バイトも受理しますが、旧FWでの互換実証はしていません。未知のmajorは無視します。
- 4文字を超えるレイヤー名はプロトコルに存在しません。日本語／UTF-8は組込みフォントの都合で `?` になります。PCの名称、選択中の出力先そのものは広告にないため推測しません。
- peripheralの0%は上流ではN/Aにも使われるため `--`。左右どちらかのperipheralだけが切れたことを確実に示すフラグはなく、central広告継続中の古いperipheral残量は判別できません。
- 単体キーボードは `BAT` の1本表示。AUX残量・修飾キー・WPMは解析していますがUIには未表示。
- staleは広告が届かない意味です。PCとのBT切断は別の `BT offline` 表示です。スリープ・省電力広告間隔・電波状況の影響を受けます。タイムアウトはidle広告間隔より十分長くしてください（上流既定30秒、本体95秒）。
- 自動キーボード切替・設定保存・OTA・操作UI・輝度調整はありません。CSがないためバス共有不可。
- USB VID/PIDはZephyr試験用の既定値。頒布時は割り当て・USB識別を決めてください。
- 中間PCB／ケースは未設計。液晶の調達先・寸法・固定穴・USB抜き差し対策は実機合格後に確定。
- 名称はユーザー指定の **ZMK Satellite**。名称変更後の商標登録簿・類似称呼の調査は未実施。ZMK公式製品を名乗るものではありません。
- 非公開のGitリポジトリとしてローカルで準備。GitHub作成・pushは認証接続後に実施します。公開する設定は含めていません。

## ライセンスと参考元

独自CコードはMIT。LCD overlayはZephyrの初期化値を利用するApache-2.0です。Prospectorの広告レイアウトを参照していますが、UI・状態管理・受信コード・アセットはコピーしていません。独立端末にProspectorやZMK本体はリンクしません。[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) と `licenses/` を参照してください。

- [Prospector Scanner](https://github.com/t-ogura/zmk-config-prospector/tree/391cd16a33ca0e2b62199d298273ed36cc6de2f9)
- [広告モジュール v2.2.3](https://github.com/t-ogura/prospector-zmk-module/tree/72a71aeea816eb0f86ffc5b108b267076ce5f0af)
- [Zephyr XIAO BLE定義](https://github.com/zephyrproject-rtos/zephyr/tree/7823374e872145b5bd018bfe447839eb36042611/boards/seeed/xiao_ble)
