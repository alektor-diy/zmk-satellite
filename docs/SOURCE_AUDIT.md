# ソースの出自・再利用・ライセンス確認

確認日: 2026-11-13。対象は現在のZMK Satellite試作版です。

## 確認結果

**今回確認した範囲では、Prospector／Prospector Scannerのファームウェアを丸ごとコピーした実装ではありません。** Satelliteの広告解析、状態管理、受信処理、画面は独立した実装になっています。一方で、互換性のために広告の形式・動作規則を参照し、LCD初期化値とビルド時の外部ライブラリを明示的に再利用しています。

「他者のコードを1行も含まない」「完全なクリーンルーム開発」「すべての権利問題がないと保証する」という意味ではありません。実装時に参照元のソースを読んでおり、以下は出自を追跡するための技術的な照合記録です。

## 取得して確認した参照元

| 参照元 | 確認したcommit | ルートのライセンス | 今回の関係 |
|---|---|---|---|
| [carrefinho/prospector](https://github.com/carrefinho/prospector/tree/19ab80f9e656592b9d0319ecfb9c35bf9c5e10dd) | `19ab80f9e656592b9d0319ecfb9c35bf9c5e10dd` | CERN-OHL-P v2 | 元のハードウェア構想。ケース等の設計データを取り込んでいない |
| [carrefinho/prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module/tree/77a852293532879c992847e01db27abe4f94fa9f) | `77a852293532879c992847e01db27abe4f94fa9f` | MIT | 元のドングル用FWとのソース比較 |
| [t-ogura/zmk-config-prospector](https://github.com/t-ogura/zmk-config-prospector/tree/391cd16a33ca0e2b62199d298273ed36cc6de2f9) | `391cd16a33ca0e2b62199d298273ed36cc6de2f9` | MIT | Scannerの構成・使い方・参照先を確認 |
| [t-ogura/prospector-zmk-module](https://github.com/t-ogura/prospector-zmk-module/tree/72a71aeea816eb0f86ffc5b108b267076ce5f0af) | `72a71aeea816eb0f86ffc5b108b267076ce5f0af`（v2.2.3） | MIT | 広告形式・送信側要件・scanner処理との比較 |
| [Zephyr](https://github.com/zephyrproject-rtos/zephyr/tree/7823374e872145b5bd018bfe447839eb36042611) | `7823374e872145b5bd018bfe447839eb36042611`（v4.1.0） | 主にApache-2.0 | プラットフォーム、標準ドライバ、LCD初期化値 |

ライセンスは各リポジトリの実際の `LICENSE` と、使用するファイルのヘッダーで確認しました。**元のハードウェアと元のFWのライセンスは別です。** 保存した上流の通知本文にハードウェアをMITと記載した箇所がありますが、それをハードウェア設計の許諾根拠にはしていません。ハードウェア側の実際の `LICENSE` はCERN-OHL-P v2です。

## ファイルごとの照合と実装の違い

Satelliteの `src/` にあるC／ヘッダー9ファイル、計2,878トークン（コメント除外）を確認しました。

| Satellite側 | 参照元で比較した内容 | Satelliteの実装 |
|---|---|---|
| `protocol.c` / `.h` | packedな `zmk_status_adv_data`、AD解析、version/profileマクロ | packed構造体をコピーせず、バイト位置から明示的にデコード。文字列終端・長さを検査 |
| `scanner.c` / `.h` | `status_scanner.c`、アドレスと名前の管理、ring buffer送信 | Zephyrの固定長 `k_msgq` へ必要データだけをコピー。名前キャッシュを持たない |
| `state.c` / `.h` | `scanner_core.c` の複数キーボード配列、選択、ロック、タイムアウト | 1台のIDを保持する小さな状態構造体。メインスレッドだけが更新 |
| `ui.c` / `.h` | Prospectorのwidget、レイアウト、タッチ、フォント・アイコン | LVGL標準ラベル中心の240×240画面。独自配置。既存widget・画像・独自フォントを取り込んでいない |
| `main.c` | shield・ZMKイベント経由の起動処理 | 独立したZephyrアプリの起動とループ。表示／受信／統合の段階別ビルド |

自動比較では、コメントと空白を除去し、識別子・文字列・数値・演算子を維持したトークン列を使用しました。C／ヘッダー9ファイルについて、参照したProspector系C／ヘッダー／overlay等113ファイルとの**完全一致ファイルは0、30トークンの連続一致窓も0**でした。共通の短いAPI呼出しや通信定数まで存在しないと主張するものではありません。

この比較は、名前変更や処理の組み替えを含むあらゆる類似性を検出できる仕組みではありません。そこで、上表の構造と責務の違い、リンクするソース、アセットの有無も合わせて確認しています。

Scannerの設定リポジトリと元のハードウェアリポジトリには、この比較対象拡張子に該当するファイルがなかったため、自動比較件数は0です。各LICENSE・README・ファイル構成は別途確認しています。Satelliteに元のSTL、CAD、組立画像、ロゴ、Prospector固有の画面アセットはありません。

## 実際に再利用しているもの

### 通信形式と動作規則

26バイトの配置、識別子、profile/versionのビット配置、左右バッテリースロット、チャンネル規則を互換性のために踏襲しています。これは参照元を隠した独自プロトコルではありません。[PROTOCOL.md](PROTOCOL.md) に根拠となるソースを固定commit付きで記録しています。

### LCD初期化値

`boards/xiao_ble.overlay` のST7789初期化値は、Zephyrの `boards/shields/st7789v_generic/st7789v_waveshare_240x240.overlay` を元にしています。自動比較でも元のProspector FWに61窓、Scanner moduleに39窓、ZephyrのWaveshare overlayに89窓の一致が出ました。窓は重複して数えるため、コピー行数・割合ではありません。確認した一致部分はLCD設定の共通する並びです。

このoverlayを「全面的な独自コード」とは扱いません。Zephyr由来の著作権者名（Jan Van Winkel／PHYTEC Messtechnik）と `SPDX-License-Identifier: Apache-2.0` をファイル先頭に保持し、Apache-2.0本文を `licenses/Zephyr-Apache-2.0.txt` に同梱しています。ピン割当・CSなし・画面サイズの設定は今回の構成に合わせています。

### ビルド依存物

Zephyr、LVGL、CMSIS、Nordic HAL、組込みフォント等は外部のオープンソースです。これらを使っていることと、自作のアプリ部分が独立実装であることは別です。MIT／Apache-2.0／BSD系／OFL／CC0等の通知と、コンパイル対象から集めたヘッダーを `licenses/` に保存しています。GCCランタイムの例外条項も同梱しています。

Satellite側の `CMakeLists.txt`、`west.yml`、依存ロックを確認し、ZMK本体やProspector moduleのコードをSatelliteのアプリへリンクしていないことも確認しました。**キーボード側には、t-oguraの広告モジュールを別途組み込む必要があります。そちらは上流コードを使用するFWです。** キーボード側の配布物にも、そのZMK・追加module・その他の依存物の通知を保ってください。SatelliteのMITだけでそれらを再ライセンスするものではありません。

## 再現方法と記録

[reference-comparison.json](reference-comparison.json) に比較対象のcommit、対象ファイルのSHA-256、一致結果を保存しています。比較用スクリプトは `scripts/audit-reference.py` です。上記参照元を同じcommitへチェックアウトしたうえで実行できます。

```sh
python scripts/audit-reference.py \
  --reference scanner=/path/to/zmk-config-prospector \
  --reference scanner-module=/path/to/t-ogura-prospector-zmk-module \
  --reference original-firmware=/path/to/carrefinho-prospector-zmk-module \
  --reference original-hardware=/path/to/carrefinho-prospector \
  --reference zephyr-lcd=/path/to/zephyr/boards/shields/st7789v_generic
```

## 頒布前に再確認する範囲

今回確認したソースと依存物について、無断の全面コピーや、既知の流用箇所での通知脱落は見つかりませんでした。ただしこの記録は、すべての権利関係を網羅する法的な適法性証明ではありません。

特に今後、Prospectorのケース・CAD等を元に中間PCBやケースを作る場合は、元のCERN-OHL-P v2と設計データ固有の通知を改めて確認します。FWのMITを根拠にハードウェアまで扱わないでください。新たなフォント・画像・bootloader・メーカーSDKを追加した場合も、この確認記録の対象外です。

## キーボード向け共通モジュールの追加

上の9ファイル比較は独立受信器の `src/` を対象とした記録です。後から追加した `module/` は別の役割で、Prospector v2.2.3のMIT送信コードを**明示的に再利用**します。受信器の独立実装という説明を、キーボード側の送信処理まで自作であるという意味にはしません。

ビルド時に固定した上流ソースを読み、元の著作権表示を保持した生成ソースをコンパイルします。共通アダプターはZephyr 3.5の初期化形式と左右の自動判定に対応し、ユーザーが指定した場合だけ独自状態広告の間隔を変更します。待機中に名前だけの広告になり続ける箇所は、名前／状態を交互に更新する補正を加えています。Prospector標準の連続広告方式と26バイト形式は維持しています。

変更のコードは `module/CMakeLists.txt` と `module/sender.c` に見える形で保存し、上流MIT本文も引き続き同梱しています。iOra専用のイベント駆動送信や別広告セットは、共通モジュールには持ち込んでいません。
