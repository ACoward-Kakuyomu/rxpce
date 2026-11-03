# rxpce 1.4.0

[English translation](#english) follows the Japanese text.

## 未リリース / Unreleased — 1.4.0公開後の開発版

* 入力設定を10個のプロファイルに保存できます。停止中にメニューまたはAlt+1〜Alt+0で切り替え、設定画面では複数の枠をまとめて編集できます。

* Tフラグが有効な演算で、ソース読出しとダイレクトページの読出し・書込みを命令内の時刻に合わせました。
* SBCへTフラグのメモリー演算を誤適用していた処理を修正し、不要な追加メモリーアクセスと待ち時間を除去しました。
* 外部水平同期の解除後に同期幅を重ねて待つ処理を修正しました。長い同期幅設定でラスタ割り込みが消失し、背景や上部表示が崩れる問題を解消します。
* **正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** 更新後は新規開始してください。ゲーム内セーブと設定は引き続き利用できます。

### English (Unreleased)

* Added ten input profiles. Switch profiles from the menu or with Alt+1 through Alt+0 while emulation is stopped, and edit multiple slots in one Settings session.

* Timed source reads and direct-page reads and writes within instructions that use the active T flag.
* Corrected the erroneous application of T-flag memory operations to SBC, removing unintended extra memory accesses and waits.
* Corrected a repeated sync-width delay after external horizontal sync release, preventing lost raster interrupts and corrupted backgrounds or upper-screen displays with long sync-width settings.
* **Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** Start normally after updating. In-game saves and settings remain usable.

以下は公開済み1.4.0の記録です。The following sections describe the published 1.4.0 release.

## 日本語

1.3.0からの主な変更です。

* CD画像とCUE参照のネットワーク読込みを明示許可制にしました。既定はローカルのみで、設定または変換補助の`--allow-network`から許可できます。
* GPUの提示待ちや最小化中も最新画像を保持し、最後の画像が失われる問題を修正しました。
* 表示区間が同期で打ち切られた場合、描かれなかった下端に以前の画像が残る問題を修正しました。
* 数字キー1・2による隠れた描画切替を廃止し、ゲームのキー割当と干渉しないようにしました。
* マルチタップの6ボタン位相をポート選択から分離し、途中までの入力読取り後も各padが共通の信号へ応答するよう修正しました。この変更前の形式のステートは読み込めません。
* IRQ制御レジスターのアドレスミラーを修正し、ステート復号時の機器時計を保存済みCPU時刻へ合わせました。
* VCE上位読出しの未使用bitと、VDCデータポートの読出しバッファ・待機を修正しました。Fastでも先読みした値を保持します。
* CD取り込みの境界探索で、先行して返る正常なリードアウトQを破損BCDと区別するよう修正しました。必要な境界証拠が欠ける場合は引き続き拒否します。
* 同じWASAPI障害を件数で要約し、ログ書き込み不能時の再試行負荷を抑えました。回復時は既存ログへ追記します。

* ラスタ割り込みの通知時期を修正し、画面分割部の枠に1pxの隙間が出る問題を改善しました。
* 表示停止（burst）中の描画・スプライト評価/IRQ・fetch予約を抑制し、ロード中の古いスプライトや不正パターンの表示を修正しました。burstの独立色$100と行内変更も反映します。
* 横スクロール値を表示前に保持する近似を採用し、上枠が一瞬横にずれる問題を修正しました。厳密な実機位相は未確定です。
* 通常CPUのデータ読出し時刻を改善し、機器状態の更新と読出し副作用の前後関係を調整しました。命令取得・間接ポインター等の厳密な時刻には近似が残ります。
* 縦スクロール値と描画有効設定を表示前に保持するよう修正し、画面分割部のスコア・枠・背景がちらつく問題を改善しました。
* 通常データ読出しで消えるVDC割り込み要求を保持し、画面分割の切替を取りこぼして一瞬大きく表示が崩れる問題を修正しました。

### 更新時の注意

**正式版1.3.0以前、および保存形式が異なる開発版のステートは1.4.0では読み込めません。** 更新後はゲームを新規起動し、新しいステートを作成してください。`Accurate`と`Fast`のステートも相互に読み込めません。

ゲーム内セーブ用の`backup.dat`と`.ram`、設定用の`rxpce.json`は引き続き利用できます。更新前にアプリを終了し、これらのファイルをバックアップしてください。ステート復帰でゲーム内セーブ用RAMも巻き戻る点は従来どおりです。

対象はWindows 11 x64です。ROM・BIOS・CDイメージは同梱していません。詳しくは[README](README.md)と[操作説明書](MANUAL.md)を参照してください。

## English

Main changes since 1.3.0:

* Network CD images and CUE references now require explicit permission. Local-only is the default; use the setting or the conversion helper’s `--allow-network` option to opt in.
* Retained the latest image during GPU presentation waits and minimization, fixing lost final frames.
* Fixed stale pixels at the bottom of the image when display is cut short by synchronization.
* Removed undocumented rendering toggles on the 1 and 2 keys so they no longer interfere with game bindings.
* Separated six-button pad phases from multitap port selection so partial scans preserve the shared signal response. States from the format preceding this change cannot be loaded.
* Fixed IRQ register address mirrors and initialized the runtime device clock at the saved CPU time when decoding states.
* Corrected unused bits in VCE high-byte reads and VDC data-port buffering and waits. Fast mode now retains prefetched data as well.
* CD capture now distinguishes valid leadout Q returned ahead of the requested sector from malformed BCD, while still rejecting missing boundary evidence.
* Summarized repeated WASAPI errors and throttled failed log writes. Recovery appends to the existing log.

* Corrected raster interrupt timing to fix one-pixel gaps at split-screen frame seams.
* Suppressed drawing, sprite evaluation/IRQs and fetch reservations during VDC burst blanking, fixing stale sprites and invalid patterns during loading. The independent burst color $100 and its intra-line changes are now handled.
* Held horizontal scroll before display using a prefetch approximation, fixing intermittent horizontal shifts in the top frame. Exact hardware phase remains unverified.
* Improved the timing of ordinary CPU data reads and their ordering relative to device updates and read side effects. Exact timing of instruction fetches and indirect pointers remains approximate.
* Held vertical scroll and display-enable settings before display to fix flickering scores, borders and backgrounds at screen splits.
* Retained VDC interrupt requests acknowledged by ordinary data reads, fixing missed screen-split updates that could briefly disrupt large parts of the display.

### Updating

**Save states from official releases 1.3.0 and earlier, or development builds with a different state format, cannot be loaded in 1.4.0.** Start games normally and create new states after updating. `Accurate` and `Fast` states remain separate.

In-game saves in `backup.dat` and `.ram`, and settings in `rxpce.json`, remain usable. Close the application and back up these files before updating. Loading a state still rolls back in-game save RAM.

Windows 11 x64 is supported. ROMs, BIOS files and CD images are not included. See the [README](README.md#english) and [user manual](MANUAL.md#english) for details.
