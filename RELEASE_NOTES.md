# rxpce 1.3.0

[English translation](#english) follows the Japanese text.

## 未リリース / Unreleased — 1.3.0公開後の開発版

* ラスタ割り込みの通知時期を修正し、画面分割部の枠に1pxの隙間が出る問題を改善しました。
* 表示停止（burst）中の描画・スプライト評価/IRQ・fetch予約を抑制し、ロード中の古いスプライトや不正パターンの表示を修正しました。burstの独立色$100と行内変更も反映します。
* 横スクロール値を表示前に保持する近似を採用し、上枠が一瞬横にずれる問題を修正しました。厳密な実機位相は未確定です。
* 通常CPUのデータ読出し時刻を改善し、機器状態の更新と読出し副作用の前後関係を調整しました。命令取得・間接ポインター等の厳密な時刻には近似が残ります。
* **この開発版では正式版1.3.0以前、およびこの更新前の開発版のステートは読み込めません。** 新規開始して新しいステートを作成してください。ゲーム内セーブと設定は引き続き利用できます。

### English (Unreleased)

* Corrected raster interrupt timing to fix one-pixel gaps at split-screen frame seams.
* Suppressed drawing, sprite evaluation/IRQs and fetch reservations during VDC burst blanking, fixing stale sprites and invalid patterns during loading. The independent burst color $100 and its intra-line changes are now handled.
* Held horizontal scroll before display using a prefetch approximation, fixing intermittent horizontal shifts in the top frame. Exact hardware phase remains unverified.
* Improved the timing of ordinary CPU data reads and their ordering relative to device updates and read side effects. Exact timing of instruction fetches and indirect pointers remains approximate.
* **This development build cannot load save states from official releases 1.3.0 and earlier or from earlier development builds.** Start normally and create new states. In-game saves and settings remain usable.

The version number is unchanged. The following sections describe the published 1.3.0 release.
以下は公開済み1.3.0の記録です。今回、版番号は変更していません。

## 日本語

1.2.0からの主な変更です。

* 特殊な背景・スプライトモードの画素選択と、表示設定の反映時期を改善しました。
* CPUとDMAのVRAM書込み規則を修正し、Accurateで書込み先アドレスがずれて表示が乱れる問題を修正しました。
* Advanced設定に`Enforce hardware sprite limit`を追加しました。既定は有効です。無効にすると描画上のスプライト制限を緩和します。割り込み・衝突判定・DMA待機の規則は変わりません。
* Accurateで走査線途中のパレット変更を反映するようにしました。追加遅延0の近似で処理し、Fastは行頭の色を使う表示を維持します。
* ステート復帰時は不完全な表示を破棄し、新しい表示区間が完成してから映像を提示するようにしました。

### 更新時の注意

**正式版1.2.0以前のステートは1.3.0では読み込めません。** 更新後はゲームを新規起動し、新しいステートを作成してください。`Accurate`と`Fast`のステートも相互に読み込めません。

ゲーム内セーブ用の`backup.dat`と`.ram`、設定用の`rxpce.json`は引き続き利用できます。更新前にアプリを終了し、これらのファイルをバックアップしてください。ステート復帰でゲーム内セーブ用RAMも巻き戻る点は従来どおりです。

対象はWindows 11 x64です。ROM・BIOS・CDイメージは同梱していません。詳しくは[README](README.md)と[操作説明書](MANUAL.md)を参照してください。

## English

Main changes since 1.2.0:

* Improved pixel selection in special background and sprite modes, and the timing of display-register changes.
* Corrected CPU and DMA VRAM write rules and fixed misplaced writes that could corrupt the display in Accurate mode.
* Added `Enforce hardware sprite limit` under Advanced settings, enabled by default. Disabling it relaxes sprite drawing limits while keeping interrupt, collision and DMA-wait behavior unchanged.
* Accurate now applies palette changes within a scanline using a zero additional delay approximation. Fast continues to use the colors at the start of each line.
* Loading a state discards incomplete display output and waits for a newly drawn display interval to finish before presenting the image.

### Updating

**Save states from official releases 1.2.0 and earlier cannot be loaded in 1.3.0.** Start games normally and create new states after updating. `Accurate` and `Fast` states also remain separate.

In-game saves in `backup.dat` and `.ram`, and settings in `rxpce.json`, remain usable. Close the application and back up these files before updating. Loading a state still rolls back in-game save RAM.

Windows 11 x64 is supported. ROMs, BIOS files and CD images are not included. See the [README](README.md#english) and [user manual](MANUAL.md#english) for details.
