# ZMK Satellite

XIAO nRF52840 と 1.3インチ 240×240 ST7789 SPI LCD を使う、ZMKキーボード状態表示専用の独立ファームウェアです。ZMKアプリを流用せず **Zephyr 4.1.0 + LVGL 9** で構成しています。Prospector互換広告を受信し、接続枠を消費するGATT接続は行いません。アクティブスキャンのSCAN_REQは送信します。

**試作版 v0.1.0。実機未検証。** ビルド結果は [検証記録](docs/VALIDATION.md) を参照してください。最初は付属の表示テスト用UF2から確認してください。

**キーボード側は [モジュールを追加してONにする](module/README.md) だけで通常構成を使えます。** このリポジトリは、独立した表示器FWと、既存ZMK向けの導入用モジュールの両方を提供します。

初めて導入する方は **[導入ガイド](docs/INSTALL.md)** から進めてください。既存キーボードのどのファイルへ何を追加するか、左右分割の例、ビルドと書き込み、元へ戻す方法を順番に説明しています。

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

普段使っているZMK設定の `config/west.yml` の `manifest.projects` に追加します。

```yaml
    - name: zmk-satellite
      url: https://github.com/alektor-diy/zmk-satellite
      revision: main
      path: modules/zmk-satellite
      import: module/west.yml
```

適用されるキーボードの共通 `.conf` に追加します。

```conf
CONFIG_ZMK_SATELLITE=y
```

あとは通常どおりキーボード用FWをビルドして書き込みます。通常の左右分割なら共通ファイル1か所でよく、左右別のCコードや広告設定を追加する必要はありません。標準動作はProspectorの100〜150ms周期の連続広告です。利用したい人が電池消費との兼ね合いでONにする追加機能としています。

任意で `CONFIG_ZMK_SATELLITE_ADV_INTERVAL_MS=1000` のように実際の独自状態広告の間隔を指定できます。表示・対象選択・受信タイムアウトなどはSatellite側の設定です。特殊な中央側配置、バッテリーを明示的に無効化した機種、非公開リポジトリのCI認証については [モジュールの説明](module/README.md) を参照してください。

**受信器用のルートwest.ymlをキーボードへimportしないでください。** `module/west.yml` はProspectorの依存だけを追加し、キーボードのZMK／Zephyr版を置き換えません。現在リポジトリが非公開のため、別リポジトリのActionsからの取得には読み取り認証が必要です。公開範囲はこの変更では変えていません。

全体の順番は [導入ガイド](docs/INSTALL.md)、モジュールの確認結果は [MODULE_VALIDATION.md](docs/MODULE_VALIDATION.md) にあります。

## ソースからビルド

Zephyrの開発環境（Python 3.10以上、Git、CMake 3.20以上、Ninja、DTC、Zephyr SDK 0.17.0）を準備します。依存ソースはmanifestで固定しています。Ubuntu／WSLの例:

```sh
mkdir satellite-workspace && cd satellite-workspace
git clone https://github.com/alektor-diy/zmk-satellite.git zmk-satellite
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
- リポジトリ: [alektor-diy/zmk-satellite](https://github.com/alektor-diy/zmk-satellite)。公開範囲はGitHub側の設定で管理します。

## 実装の出自とライセンス

Prospector／Prospector Scannerを参考にしています。今回照合した範囲では、**両プロジェクトのFWを丸ごとコピーしたものではありません**。広告の解析、1台の状態管理、受信キュー、240×240の表示画面を独立して実装しました。互換性のため、広告フォーマット・左右の割当・チャンネルなどの動作規則は上流に合わせています。

アプリ側のC／ヘッダー9ファイルを元のProspector FW・Scanner moduleのソースと照合し、完全一致ファイルや、コメントを除いた30トークン以上の連続一致は検出されませんでした。比較方法・参照commit・限界は **[ソース確認記録](docs/SOURCE_AUDIT.md)** と [機械比較結果](docs/reference-comparison.json) に残しています。参照元のソースを読んで開発しており、クリーンルーム開発を主張するものではありません。

| 部分 | 出自・扱い |
|---|---|
| アプリのCコード | 独立実装、MIT |
| BLE広告の形式 | t-ogura版Prospector moduleを参照して互換実装 |
| LCDの初期化値 | ZephyrのST7789 overlayから再利用、Apache-2.0。著作権者名・ライセンスを保持 |
| RTOS・表示ライブラリ等 | Zephyr／LVGL等の外部依存物。各ライセンスと通知を同梱 |
| キーボード側の送信処理 | t-oguraのMIT送信実装を共通アダプター経由で使用。独自受信器コードとは別 |
| 元のProspectorの基板・ケース等 | 今回取り込んでいない。元のハードウェア設計はCERN-OHL-P v2、元のFWはMIT |

「依存物まで一切流用していない」という意味ではありません。再利用箇所を区別して表示しています。[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) と `licenses/` をバイナリ配布時にも同梱してください。今後ケース等の設計を流用する場合は、FWとは別にハードウェア側の条件を確認します。

## 参考にしたプロジェクトと謝辞

**carrefinho氏とProspectorの貢献者の皆様**に、XIAOとカラーLCDによるデスクトップ表示器という構想と、公開されたハードウェア・FW資料に感謝します。

**t-ogura氏とProspector Scanner／Prospector ZMK Moduleの貢献者の皆様**に、キーボードの接続枠を使わず状態を表示する方式、広告プロトコル、キーボード側送信機能と実装資料を公開してくださったことに感謝します。Satelliteが既存の状態広告に対応できるのは、これらの公開資料のおかげです。

また、ZMK、Zephyr、LVGL、および関連ドライバ・フォントの開発者と貢献者の皆様に感謝します。本プロジェクトはこれらの団体・著者による公式製品や承認製品ではありません。

- [元のProspectorハードウェア — carrefinho/prospector](https://github.com/carrefinho/prospector)
- [元のProspector FW — carrefinho/prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module)
- [Prospector Scanner — t-ogura/zmk-config-prospector](https://github.com/t-ogura/zmk-config-prospector)
- [互換広告／キーボード側モジュール — t-ogura/prospector-zmk-module](https://github.com/t-ogura/prospector-zmk-module)
- [ZMK](https://zmk.dev/)、[Zephyr](https://www.zephyrproject.org/)、[LVGL](https://lvgl.io/)
