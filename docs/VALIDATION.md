# 検証記録 — 2026-11-13

## 実施済み

Windows環境で、Zephyr 4.1.0（commitはwest.ymlに固定）、Python 3.13.7、CMake 3.31.10、Ninja 1.13.2、GNU Arm Embedded GCC 12.2.0を使用。既存のQMK MSYSにあるArmコンパイラーを `cross-compile` toolchainとして使い、minimal libcでリンクしました。SDK 0.17.0を使う標準手順・CI設定も用意していますが、そのSDKを使ったビルドは本環境では実施していません。

| 段階 | XIAO向けビルド | FLASH | RAM |
|---|---|---:|---:|
| display | 成功、UF2生成 | 308,692 B | 71,488 B |
| ble | 成功、UF2生成 | 113,932 B | 36,652 B |
| status | 成功、UF2生成 | 347,140 B | 81,580 B |

使用可能領域はFLASH 788KiB、RAM 256KiB。UF2ファイルは各データブロックを検査し、nRF52840 family ID `0xADA52840`、先頭 `0x27000`、全書き込み先がアプリ領域 `0x27000..0xEC000` 未満に収まること、ブロック数・マジック値・番号の整合を確認。SHA-256は `firmware/manifest.json` と `firmware/SHA256SUMS.txt` に記録。

有効設定も確認: status/bleはBT_OBSERVERのみ、BT_CONN／BT_CENTRAL／BT_PERIPHERAL／BT_BROADCASTERを使わず、displayではBLEを無効化、bleではLVGL／DISPLAYを無効化。LVGLはメインスレッドから明示初期化して結果を確認します。

ホストGCC 13.2.0で `-std=c11 -Wall -Wextra -Werror` を指定し、同じ `protocol.c` / `state.c` をコンパイルして実行。次を通過しました。

- 独立した31バイトfixtureとmanufacturer-only（SCAN_RSP想定）で26バイト形式をデコード。
- 左右残量、4文字でNULのないレイヤー名、profile patch/devのマスク、ID wire order。
- 全ての短縮長、壊れたAD長、誤った識別子、未知major、peripheral role、無効profileの拒否。
- 非ASCII名の安全な置換、チャンネル0／1〜9／10以上の上流互換判定、固定IDの検査。
- 未受信状態、タイムアウト境界、別IDの混入拒否、同じIDからの復帰。
- 決定的に生成した不正AD入力100,000件で解析ループが終了すること。

このWindowsホスト試験ではAddressSanitizerは使用していません。Linux CIの `scripts/test-host.sh` はASan/UBSanを指定していますが、CI実行はGitHub接続後です。

YAMLとPowerShellスクリプトの構文も確認しました。`publish-private.ps1` の実際のリポジトリ作成／pushは認証がないため未実行です。

## ビルド時の注意表示

- ZephyrのUSB試験用VID `0x2FE3` / PID `0x0100` を使用している旨の表示あり。頒布用の割り当ては未確定。
- ASSERT有効の通知あり。試作の異常検出のため有効にしています。
- GNU ld 2.41によるELFのLOAD segment RWX警告あり。ビルド成功・UF2生成を確認していますが、この警告を解消済みとはしていません。
- 本環境ではDTC実行ファイルが見つからない旨の表示あり。ZephyrのPython devicetree生成・binding検証・Cコンパイルは完了。独立したDTC追加警告検査は未実施。標準手順／CIにはDTCの準備を記載しています。
- Kconfigの未定義設定／依存不成立による警告、アプリCソースのコンパイル警告は最終構成では検出されませんでした。

## 未実施

実機への書き込み、LCD表示と色／オフセット／SPI modeの確定、実際のキーボード広告での相互運用、USB電源バンク動作、消費電流、数時間以上の耐久試験。キーボード側のZMKリポジトリが未提示なので、そのFWのビルド／変更も行っていません。

上流ソースの形式との照合と合成fixtureの合格を、実際のBLE無線での互換性確認とは区別してください。実機チェックリストは `HARDWARE.md` にあります。

GitHubは既存CLI認証がなく、利用可能なブラウザもサインイン画面でした。**GitHub上のリポジトリ作成・pushは未完了**。ローカルGitの `main` と配布ZIPを用意し、接続後にprivate属性を検査して作成・pushするスクリプトを同梱しています。
