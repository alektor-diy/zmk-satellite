# 導入ガイド

ZMK Satelliteを組み立てるだけでは、キーボードの情報は表示されません。**表示器の準備**と、**いつも使っているキーボードのファームウェアへの送信機能の追加**が必要です。

このガイドは、すでに自分のZMK設定リポジトリからファームウェアを作れている人向けです。GitHub Actionsで作る方法を中心に説明します。キーボードの機種や既存設定は変更せず、状態を知らせる機能を追加します。

試作版につき実機での導入試験はまだ完了していません。対応する設定名と通信形式は上流ソースで確認していますが、すべてのキーボードでの動作を確認したものではありません。

## 最初に全体像を確認

```text
キーボード ── いつものBluetooth接続 ── PC・タブレット
    │
    └── 状態のお知らせ（BLE広告） ── ZMK Satelliteの画面
```

SatelliteとキーボードをBluetooth設定画面でペアリングする必要はありません。SatelliteをPCのキーボードとして登録する必要もありません。PCへ文字を送る接続は従来どおりです。

| 作業先 | すること | 使うファイル |
|---|---|---|
| 表示器のXIAO | SatelliteのFWを書き込む | このプロジェクトの `satellite-*.uf2` |
| 手持ちのZMKキーボード | 状態送信モジュールを追加してFWを作り直す | 自分のキーボード設定の `west.yml` と `.conf` |

**Satellite用のUF2はキーボードへ書き込まないでください。** キーボードには、そのキーボード用に作り直したFWを書き込みます。

## 1. 元に戻せる状態を用意

作業前に、現在動いているキーボードの左右または単体のFWを保存してください。設定リポジトリの現在の状態も、コミットやZIPで控えておきます。GitHubで変更する場合は作業用ブランチを作ると戻しやすくなります。

完成品のFWしか持っておらず、自分の設定からビルドできない場合は、先に販売元・設計者から設定リポジトリと通常のビルド手順を確認してください。既存のUF2に設定だけ後付けする方法ではありません。

## 2. まず表示器だけを試す

1. [READMEの配線表](../README.md#配線)どおりにXIAOとLCDを接続します。電源は3.3Vです。
2. XIAOをUF2書き込みモードにし、`firmware/satellite-display.uf2` をXIAOのドライブへコピーします。
3. `ZMK SATELLITE`、模擬のバッテリー残量、画面下の赤・緑・青が見えることを確認します。

この段階の76%／65%はテスト値です。キーボードを操作しても実際の残量には変わりません。白画面・色違い・画面の欠けは、先に [画面の切り分け手順](HARDWARE.md)で解消してください。

## 3. キーボードの設定リポジトリを開く

**ここからはSatelliteのリポジトリではなく、普段キーマップを編集している自分のZMK設定リポジトリで作業します。**

よくある構成は次のとおりです。`my_keyboard` は例なので、自分の機種名に読み替えます。

```text
自分のzmk-config/
├─ build.yaml                  ← ビルドするboard・shieldを確認
├─ config/
│  ├─ west.yml                 ← 送信モジュールを追加
│  ├─ my_keyboard.keymap       ← 今回は編集不要
│  └─ my_keyboard.conf         ← 状態送信の設定を追加
└─ .github/workflows/          ← 普段のビルド処理をそのまま使う
```

別の構成やメーカー独自ビルドでは場所が異なります。その場合は普段使っている `ZMK_CONFIG` の場所と、ビルドログに出る `.conf` の読込先を基準にしてください。ZMK本体やダウンロードしたモジュールの内部ファイルを直接書き換える必要はありません。

## 4. `config/west.yml` にモジュールを追加

`west.yml` は「ビルドに必要な部品をどこから取ってくるか」を指定するファイルです。既存の `manifest:` → `projects:` の一覧に、次の項目を**追加**します。

```yaml
    - name: prospector-zmk-module
      url: https://github.com/t-ogura/prospector-zmk-module
      revision: 72a71aeea816eb0f86ffc5b108b267076ce5f0af # v2.2.3
      path: modules/prospector-zmk-module
```

空白の位置は、既存の `- name: zmk` などと同じに揃えます。タブではなく空白を使ってください。`projects:` や `manifest:` をもう1つ作る必要はありません。直接URLを指定しているので `remotes:` の追加は不要です。

例として、既存のZMK指定が `main` のリポジトリなら、追加後は次の形になります。

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: prospector-zmk-module
      url: https://github.com/t-ogura/prospector-zmk-module
      revision: 72a71aeea816eb0f86ffc5b108b267076ce5f0af
      path: modules/prospector-zmk-module
  self:
    path: config
```

**上の全体例で自分のファイルを丸ごと上書きしないでください。** 自分のZMKの `revision`、fork、すでに使っているモジュールはそのまま残します。すでに `prospector-zmk-module` がある場合も二重登録せず、現在のURLとバージョンを確認します。別forkや別機能で使っている場合、無条件の置換は避けてください。

追加するのは **t-ogura版の状態広告モジュール** です。carrefinho版の元のドングル用モジュールとは用途が違います。また、キーボードを `prospector_scanner` shieldへ変更したり、キーボード側で `CONFIG_PROSPECTOR_MODE_SCANNER` を有効にしたりはしません。

## 5. キーボードの `.conf` に送信設定を追加

### 一体型キーボードの場合

普段使っている、ビルド対象の名前に合った `.conf` を開き、末尾に次を追加します。同じ設定がすでにある場合は、その行を編集して重複を避けます。

```conf
# ZMK Satelliteへ状態を知らせる
CONFIG_ZMK_STATUS_ADVERTISEMENT=y
CONFIG_ZMK_BATTERY_REPORTING=y
```

バッテリー表示は、もともとのボード定義に残量取得の仕組みがあることが前提です。この設定だけで未対応のバッテリーセンサーを追加できるわけではありません。

### 左右分割キーボードの場合

まず **central（PCと接続する側）** を確認します。左とは限りません。`build.yaml` の左右それぞれのビルドと、設計者の説明またはビルド済み `.config` の `CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y` を確認してください。USBを挿せる側が両方あっても、それだけでcentralは判定できません。

以下は **通常の左右2枚、左がcentral** の例です。`build.yaml` のshieldが `corne_left` / `corne_right` なら、ユーザー設定ファイル名は `config/corne_left.conf` / `config/corne_right.conf` です。他機種は自分のshield名、オンボードMCU機は適用されるboard名に読み替えてください。

#### 左右共通の `.conf` がある場合の準備

現在のZMKの公式手順では、`corne.conf` のような共通ファイルがあると、片側用ファイルが採用されない構成があります。古いZMKでは複数ファイルが読み込まれる実装もあり、バージョンによって違います。**左右別の設定を確実に使うため、このガイドでは共通 `.conf` を左右のファイルへ分けます。**

1. 元の `corne.conf` をリポジトリ外にもバックアップします。
2. 内容を省略せずコピーして `corne_left.conf` と `corne_right.conf` を作ります。既存の左右ファイルがある場合は上書きせず、設定を照合して元の設定も残します。
3. 共通の `corne.conf` は `corne.conf.backup` に改名し、`.conf` として読まれない状態にします。後で戻せるよう残して構いません。
4. 次の設定を左右のファイルへ追加します。**共通の `.keymap` はそのままで大丈夫です。**

左・central側の `config/corne_left.conf` に追加:

```conf
CONFIG_ZMK_STATUS_ADVERTISEMENT=y
CONFIG_ZMK_BATTERY_REPORTING=y
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
CONFIG_ZMK_STATUS_ADV_CENTRAL_SIDE="LEFT"
CONFIG_PROSPECTOR_EXPECTED_PERIPHERAL_COUNT=1
```

右・peripheral側の `config/corne_right.conf` に追加:

```conf
CONFIG_ZMK_BATTERY_REPORTING=y
CONFIG_BT_BAS=y
```

peripheralの `BT_BAS` は、centralへ残量を渡すためのバッテリーサービスです。省電力などを理由に以前 `CONFIG_BT_BAS=n` にしていた場合は、左右残量取得との両立を確認してください。

**右がcentralなら**、状態広告・FETCHING・EXPECTED_PERIPHERAL_COUNTを右の設定へ移し、`CENTRAL_SIDE="RIGHT"` にします。左をperipheral設定にします。キーボード本来の `ZMK_SPLIT_ROLE_CENTRAL` をこの作業のために切り替える必要はありません。

追加で使う `CENTRAL_BATTERY_LEVEL_PROXY` は、PCへ別のバッテリーサービスを見せるための機能です。Satelliteは広告を受け取るので、この表示のために新しく有効にする必要はありません。すでに使っている設定を削除する必要もありません。

ドングルやトラックボールがcentral、またはperipheralが2台以上ある構成は、この2枚の例をそのまま当てはめないでください。[複数デバイスの補足](#複数デバイスの補足)へ進みます。

## 6. 保存して、いつもの方法でFWを作る

GitHub上で編集した場合は、変更した `west.yml` と `.conf` を同じ作業ブランチへ保存します。普段のビルド用ワークフローを実行し、今回の変更が入った実行であることを確認してください。ブランチへのpushで自動実行されるか、手動実行かは、現在のワークフロー設定によります。

普段どおりActionsの結果が成功してから、成果物（Artifacts）のFWをダウンロードしてZIPを展開します。**左右分割は左右それぞれのFWを作り、対応する側へ書き込みます。** `build.yaml` のboardやshieldをSatellite向けに変える必要はありません。

ビルドの設定一覧または生成された `.config` で、少なくとも次を確認します。

| 確認するもの | 期待する結果 |
|---|---|
| モジュールの取得 | `prospector-zmk-module` が指定commitで取得されている |
| 読み込んだ設定 | 左右それぞれで、編集した `.conf` のパスがログに出る |
| central／単体 | `CONFIG_ZMK_STATUS_ADVERTISEMENT=y` |
| 分割central | `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` |
| 分割peripheral | バッテリー報告と `CONFIG_BT_BAS=y` |
| ビルド結果 | 設定の未定義・依存不成立・リンクエラーがない |

ZMK 0.3系と現在のZMKの対応は上流モジュールの説明に基づきます。特定のメーカーforkでのビルドは未確認です。エラーが出ても、ZMKのrevisionをまとめて最新に変更して回避する方法は取らず、まず設定名・取得先・元のZMKとの対応を確認してください。

### ローカルでビルドしている場合

既存のZMKワークスペースで、manifest編集後に `west update` を実行し、これまで使っているboard・shield・`ZMK_CONFIG` の指定を維持して再ビルドします。設定を作り直すため、ビルド先を新しくするか `west build -p always` を使います。Satelliteのワークスペースでキーボードをビルドしないよう注意してください。

## 7. キーボードへ書き込んで、通常動作を確認

機種ごとの通常の書き込み方法で、新しく作ったキーボード用FWを入れます。UF2対応機ならブートローダーのドライブへその側のUF2をコピーします。UF2以外の機種は販売元の方法に従ってください。

左右とも作業を終えたら、まず従来どおりPCで文字入力できることを確認します。今回の導入でペアリング消去や `settings_reset` を通常の手順として行う必要はありません。

任意設定の `CONFIG_ZMK_STATUS_ADV_KEYBOARD_NAME` は初回には追加しなくて構いません。これは状態表示用ラベルだけでなくPCから見えるBLE名も変えるため、既存の名前を保ったまま確認する方が切り分けやすくなります。

## 8. Satelliteで受信を確認

1. XIAOへ `firmware/satellite-ble.uf2` を書き込み、USBシリアル端末を115200bpsで開きます。
2. キーボードを起動してキー入力します。初回は近くの他の対応キーボードを停止しておくと、対象を選びやすくなります。
3. `ID=... L=... R=... layer=... BT=...` が出ることを確認します。`Scanning active` だけでは状態広告の受信成功ではありません。
4. XIAOへ `firmware/satellite-status.uf2` を書き込みます。`Receiving` と実際の状態が表示されれば統合表示に進めています。
5. レイヤー切替とBTプロファイル切替を試します。`BT 1` はZMKの番号0に対応します。

idle時の広告は上流の既定で約30秒間隔です。すぐ変わらない場合はキー操作して待ってください。広告が95秒届かなければ `Signal lost / stale`、広告が届いていてPCのBT接続だけが切れていれば `BT offline` です。

分割の0%は未取得にも使われるので `--` と表示します。片側を切ってもcentralが古い残量を広告し続ける場合があり、片側の接続をこの画面だけで確定することはできません。

## うまくいかない場合

| 症状 | 最初に確認すること |
|---|---|
| `ZMK_STATUS_ADVERTISEMENT` が未定義 | モジュールが取得されているか、URLとcommit、YAMLの空白位置、ビルド対象ブランチ |
| FETCHINGの依存関係エラー | central用設定をperipheralにも入れていないか、実際のcentralはどちらか |
| 設定を変えても反映されない | 共通 `.conf`、別の `ZMK_CONFIG`、ビルドキャッシュ、古いFWの書き込み |
| `Scanning...` のまま | キーボードへ新しいFWを入れたか、centralが起動しているか、スキャン用ではなく送信用設定を入れたか |
| バッテリー片側が `--` | peripheralの報告／BT_BAS、centralのFETCHING、左右接続とセンサー設定 |
| 左右の値が逆 | `CENTRAL_SIDE` が物理的なcentralの側と一致しているか |
| 別のキーボードが表示される | 再起動して対象を先に起動するか、[固定ID設定](../README.md#ソースからビルド)を使う |
| 放置するとstaleになる | キーボードのidle間隔がSatelliteの95秒より長くないか |
| キーボード自体が動かない | 保存しておいた元のFWへ戻し、設定差分とビルドログを確認 |

元へ戻すときは、キーボード設定の変更を元に戻して再ビルドするか、保存済みの元のFWを通常の方法で書き戻します。共通 `.conf` を分けた場合は、元のファイル構成も復元します。

## 複数デバイスの補足

- centralが左右のどちらでもない場合、**そのcentralのFW**に広告機能を入れます。左右のperipheralだけに入れても集約状態は送信されません。
- `CONFIG_ZMK_STATUS_ADV_CENTRAL_SIDE="AUX"` は左右の残量をperipheralから割り当て、central自身の残量をAux1へ送る形式です。Satelliteの現在の画面は左右のみ表示します。
- 左右2枚がperipheralなら `CONFIG_PROSPECTOR_EXPECTED_PERIPHERAL_COUNT=2`。ZMK自体の既存のperipheral台数設定とも整合させます。
- 必要に応じ `CONFIG_ZMK_STATUS_ADV_LEFT_PERIPHERAL` / `RIGHT_PERIPHERAL` で実際の接続順を指定します。値を重複させないでください。
- USB専用centralなどバッテリーセンサーがない機器には、バッテリー報告設定を機械的に追加せず、元のハードウェア定義・上流モジュールとの適合を確認します。

## 参照資料

- [ZMK公式: モジュール追加](https://zmk.dev/docs/features/modules)
- [ZMK公式: 設定ファイルの場所と選択](https://zmk.dev/docs/config)
- [ZMK公式: 分割のバッテリー取得／転送設定](https://zmk.dev/docs/config/split)
- [ZMK公式: バッテリー報告](https://zmk.dev/docs/config/battery)
- [採用したt-oguraモジュールのREADME](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/README.md)
- [詳細な広告仕様](PROTOCOL.md)、[実施した検証と限界](VALIDATION.md)
