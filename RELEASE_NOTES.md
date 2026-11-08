# rxpce 1.5.2

[English translation](#english) follows the Japanese text.

## 日本語

1.5.1からの主な変更です。

* F9で一時停止してRAMを検索できるメモリーツールを追加しました。1・2・4バイトの値一致と、前回F9からの不変・変化・増加・減少に対応し、10進数と16進数を併記します。
* Alt+F9で開くチート編集タブを追加しました。名前・有効状態・領域・オフセット・バイト数・値を編集でき、検索結果からの登録にも対応します。
* 停止中に一度だけ書き込む単発適用と、エミュレーターの各フレーム境界で値を固定する常時適用に対応しました。バックアップRAMと永続HuCard RAMは検索専用です。
* チート定義をゲームイメージの隣のJSONへ保存し、次回起動時に読み込みます。媒体・CD BIOSを照合し、CUEとCHDは個別、AccurateとFastは共通で管理します。保存失敗時は元ファイルと編集内容を保持します。
* 有効な常時適用コードがある間はリプレイ記録を無効にし、再生時は外部チートを適用しません。単発適用したRAMは記録開始時の状態に含まれます。
* **1.5.0／1.5.1のステートは引き続き読み込めます。正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** チート定義はステートに含めず、ゲーム内セーブと設定も引き続き利用できます。

## English

Changes since 1.5.1.

* Added a paused RAM search tool on F9. Search unsigned 1-, 2- or 4-byte values by exact value or changes since the previous F9 capture: unchanged, changed, increased or decreased. Values display in decimal and hexadecimal.
* Added a Cheats tab on Alt+F9. Edit names, enablement, regions, offsets, byte sizes and values, or register entries from search results.
* Added single-use application while paused and Always Apply codes at each emulated frame boundary. Backup RAM and persistent HuCard RAM remain search-only.
* Save definitions as JSON beside each game image and load them on the next startup. Media and CD BIOS identities are checked. CUE and CHD stay separate; Accurate and Fast share definitions. Failed saves preserve the previous file and editable changes.
* Replay recording is disabled while any Always Apply code is enabled. Playback ignores external cheats. RAM modified by a single-use code is included in the recording's start state.
* **Save states from 1.5.0 and 1.5.1 remain compatible. Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** Cheat definitions remain external to states. In-game saves and settings remain usable.
