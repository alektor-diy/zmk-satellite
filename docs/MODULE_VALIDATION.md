# キーボード用共通モジュールの検証

2026-11-13。Prospector標準送信を使う共通モジュールへ整理し、iOra専用のイベント駆動送信と別広告セット方式は採用しません。

## 実施した確認

| 構成 | ローカルビルド | FLASH | RAM |
|---|---|---:|---:|
| iOra left | 成功 | 263,500 B | 72,650 B |
| iOra right | 成功 | 182,088 B | 34,532 B |
| stock Corne left | 成功 | 213,424 B | 46,848 B |
| stock Corne right | 成功 | 171,240 B | 32,388 B |

ZMK v0.3、指定のZephyr 3.5、Prospector v2.2.3（固定commit）で確認しました。Windows / Python 3.13.7 / CMake 3.31.10 / GNU Arm GCC 12.2.0のcross-compile環境です。

- iOra、標準Corneとも共通 `.conf` の `CONFIG_ZMK_SATELLITE=y` だけで左centralが送信し、右は同じON設定でも状態広告無効。
- 標準Corneでは既定で無効なperipheral残量取得がONだけで有効になることを確認。両機種のcentralは名前からLEFTと判定。
- iOraのStudio／既存非LiPo電池管理／キーマップ／通常のトラックボール処理を維持。
- ONを外した左構成は、状態広告が無効でアダプターがビルド対象にないことを設定生成で確認（OFF版UF2の追加ビルドは未実施）。
- 任意の1000ms指定では、生成ソースの独自状態広告だけがBLE interval 1600 unitsへ変わることを確認。接続用proxyと分割探索burstの100〜150ms指定は不変。この別周期版は設定生成と生成ソースの検査で確認し、追加UF2ビルドは未実施。
- 既定100指定では、生成された上流ソースは元のv2.2.3から、生成元コメントと記載済みのidle-name交互更新補正以外に差がないことを照合。
- Zephyr 3.5の初期化形式の型不一致を共通wrapperで修正。
- 共通モジュールmetadataを読み込ませた状態でも、独立受信器のdisplay／ble／statusの3種類をZephyr 4.1でビルドできることを確認。受信器内ではZMK_SATELLITEは有効にならず、送信処理も入らない。

## 互換補正の範囲

中央側判定と初期化形式は共通アダプターが扱います。上流v2.2.3のidle-name間隔1では、待機中にmanufacturer状態が名前だけへ置換されたままになるため、2にして名前と状態を交互に更新します。無線は上流どおり連続広告です。独自のイベント送信方式に切り替えるものではありません。

`ZMK_STATUS_ADV_CENTRAL_SIDE` の生成 `.config` は上流の既定RIGHTのままに見える場合があります。実際にコンパイルする中央側は共通wrapperへ渡した解決値です。ビルドログの `ZMK Satellite: ... central=LEFT` とmoduleのコンパイル定義を確認してください。

## 未確認・注意

新しい共通モジュール構成の実機試験、電池消費、全キーボード／全ZMK forkでの検証は未実施です。標準CorneのCIワークフローは追加済みですがGitHub上での実行はpush後です。現在は公開リポジトリから取得でき、専用の読み取り認証は不要です。

元のZMK v0.3コードの未使用変数・ログ書式、非推奨ボード設定、GNU ldのRWX、本Windows環境のDTC未検出／長いobject pathなどの警告は残っています。Python devicetree生成・binding検査・Cビルド・リンクは通過しています。

## manifest import経由の最終確認

Satelliteの `b0660ff2f3784a0db54596cd660e1d6f7ea218a3` をローカルfile URLのwest projectとして取得し、`import: module/west.yml` によってProspector固定版が自動取得されることを確認しました。受信器用のルートwest.ymlは取り込まれず、既存ZMK v0.3／Zephyr 3.5を維持しています。その後、Satelliteを手動のEXTRA_MODULESへ指定せずiOra左右を再ビルドし成功しました。iOraへの適用は `92abc7f` です。

このテストは実際のwest import／module自動認識を通していますが、GitHub HTTPSの非公開依存認証やActions実行を検証したものではありません。以前の非公開依存向けの認証設定は撤去済みです。現行の導入は公開モジュール追加とON設定だけです。
