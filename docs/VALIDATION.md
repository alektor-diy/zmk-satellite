# 現行版の検証記録 — 2026-11-15

現行配布は `battery-compact-number-only` です。上段にキーボード名とレイヤー番号、中央に左右残量、下段にBT1〜BT5／USBと接続状態を表示します。レイヤー名・修飾キー・LIVE・RSSIは表示しません。

## ビルドとファイル検証

Windows、Zephyr 4.1.0、LVGL 9、GNU Arm Embedded GCC 12.2.0で3段階のビルドに成功。UF2のブロック番号・マジック・family ID `0xADA52840`・書込領域 `0x27000..0xEC000`・SHA-256を確認しました。サイズとハッシュの正本は [manifest](../firmware/manifest.json) と [SHA256SUMS](../firmware/SHA256SUMS.txt) です。

| 段階 | FLASH | RAM |
|---|---:|---:|
| display | 406,792 B | 71,488 B |
| ble | 114,448 B | 37,676 B |
| status | 445,736 B | 82,668 B |

表示設定はSPI mode 3、4MHz、オフセット0、RAM制御00 f8。固定レイヤー名対応表・機種固有の表示設定はありません。キーボード側の広告形式とON設定は変更していません。

## 確認済み

- プロトコル・状態管理・名前解析・BT状態のホスト試験と100,000件の不正フレーム検査。WindowsではASan未使用、Linux用テストはASan/UBSan付き。
- 実フォントでSylvaTwin_Lは146/158px、レイヤー255は42/48px、100%は91/96px。下段の接続状態も欄内に収まります。
- ユーザー実機でBLE受信、mode 3でのLCD描画、00 f8での色の正常化、前版UIの統合動作。
- キーボード向けモジュールのローカルビルドは [MODULE_VALIDATION.md](MODULE_VALIDATION.md) を参照。

## 未確認・既知の制約

最新配置自体の実機確認、長時間耐久・消費電流、全LCD／全キーボード／全ZMK forkでの互換性は未確認です。標準Zephyr SDK 0.17.0とGitHub Actionsの結果は、このローカルビルド結果とは別です。

USB VID/PIDはZephyr試験用。ELFのRWX LOAD segment警告があります。今回のローカル環境では独立したDTCによる追加検査は未実施ですが、Zephyrのbinding検証とCコンパイルは成功しています。

導入・操作手順は [INSTALL.md](INSTALL.md)、ハードウェアの切り分けは [HARDWARE.md](HARDWARE.md)、出自とライセンスの確認は [SOURCE_AUDIT.md](SOURCE_AUDIT.md) を参照してください。[過去の開発記録](history/VALIDATION-2026-11.md) は現行の設定手順ではありません。
