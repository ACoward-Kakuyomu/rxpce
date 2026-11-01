# rxpce 1.3.0

[English translation](#english) follows the Japanese text.

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
