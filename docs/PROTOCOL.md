# Prospector互換広告の確認記録

参照先: `t-ogura/prospector-zmk-module` v2.2.3、commit `72a71aeea816eb0f86ffc5b108b267076ce5f0af`。

確認したソース:

- [`include/zmk/status_advertisement.h`](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/include/zmk/status_advertisement.h): 26バイト構造、version/profileのビット配置、flags。
- [`src/status_advertisement.c`](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/src/status_advertisement.c): build_manufacturer_payload、左右マッピング、HWINFO由来ID、広告モード。
- [`src/status_scanner.c`](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/src/status_scanner.c): active scan、manufacturer照合、チャンネル規則。
- [`src/scanner_core.c`](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/src/scanner_core.c): 表示へのバッテリースロット割り当て。
- [`Kconfig`](https://github.com/t-ogura/prospector-zmk-module/blob/72a71aeea816eb0f86ffc5b108b267076ce5f0af/Kconfig): idle 30秒既定、central side、期待peripheral台数。

## AD構造

Legacy advertisingのAD type **0xFF Manufacturer Specific Data** に格納。Flags AD 3バイト + manufacturer AD 28バイト = 31バイト。以下のoffsetはADのlength/typeを除いたmanufacturer payload内です。`AB CD`は独自マーカーであり、AD type 0x16のBluetooth Service Dataや通常の16bit UUID広告として検索してはいけません。

| offset | 長さ | 意味 |
|---|---|---|
| 0 | 2 | FF FF: reserved company ID |
| 2 | 2 | AB CD: Prospector識別子、バイト列をそのまま照合 |
| 4 | 1 | 上位4bit major、下位4bit minor（v2.2.xなら22 hex） |
| 5 | 1 | 左バッテリー。単体は自機 |
| 6 | 1 | active layer |
| 7 | 1 | bit6 dev、bits5:3 patch、bits2:0 profile（0〜4） |
| 8 | 1 | connection count |
| 9 | 1 | flags |
| 10 | 1 | role: 0 standalone、1 central、2 peripheral |
| 11 | 1 | device index |
| 12 | 3 | 右バッテリー、Aux1、Aux2 |
| 15 | 4 | layer name。4文字全部使えるため終端NULを前提にしない |
| 19 | 4 | keyboard ID。数値変換せずwire orderで保持 |
| 23 | 1 | modifier bits |
| 24 | 1 | WPM |
| 25 | 1 | channel |

flags: bit0 Caps Word、bit1 charging（現senderのflags生成では未設定）、bit2 USB powered、bit3 USB HID ready、bit4 active BT profile connected、bit5 bonded。bit3とbit4は同時に立ち得ます。どちらが選択中のZMK endpointかを広告から確定できないため、それぞれ別に表示します。

profile_slot全体をプロファイル番号にしません。v2.2.3のprofile 2（画面BT 3）は `0x1A`。`& 7` が必須です。26バイト以上を受理し、末尾の追加領域は無視します。未来のmajorは拒否。role 2は状態集約元ではないため無視します。バッテリー100超は表示を `--` にします。

## 左右とID

ヘッダーの古いコメントだけを信頼すると左右を間違えます。v2.2.3のsenderは `CENTRAL_SIDE=RIGHT` のときbyte5にperipheral（左）、byte12にcentral（右）を設定済みです。LEFTでも左／右を維持。AUXならbyte5/12は左右peripheral、centralはAux1です。受信側で入れ替えません。

0はperipheral未取得・不在にも使われ、本当の0%と区別できません。分割機の左／右0は `--`、単体のbyte5=0は0%と表示します。分割の片側鮮度を示す別のタイムスタンプはありません。

keyboard IDはHWINFOを31倍加算ハッシュした4バイト（HWINFO失敗時はキーボード名のハッシュ）です。MAC/RPAではないため、プロファイル変更時にも追跡できます。ID衝突・他者による偽装は防ぎません。ログ表示の8桁をそのまま固定ID設定に使用してください。senderの数値表記ログはlittle-endianのため並びが逆に見える場合があります。

## 広告モードとタイムアウト

senderはZMKの接続可能広告へのpiggyback、non-connectable広告、connectable proxyを使い分けます。manufacturerがSCAN_RSPにある場合があるのでactive scanを使用。重複フィルターは無効にし、同じ状態の広告でもlast_seenを更新します。名前だけの応答はlast_seenを更新しません。

上流scannerと同じチャンネル判定: scanner=0、scanner>=10、keyboard=0、scanner=keyboard のいずれかなら受理。1〜9を専用チャンネルとして使えますが、送信側0は常に通ります。固定keyboard IDと併用してください。

起動後に最初の有効IDを選択し、その後はタイムアウトしても選択を保持。USB電源再投入で再選択できます。複数キーボード一覧や自動巡回は実装していません。

上流のidle間隔は最大300秒まで設定可能です。本体の95秒タイムアウトは既定30秒に対する暫定値。実際のキーボード設定が長ければ本体の `SATELLITE_TIMEOUT_MS` を増やしてください。
