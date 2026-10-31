# rxpce 1.2.0

[English translation](#english) follows the Japanese text.

## 日本語

1.1.0からの主な変更です。

* CPUの命令取得を修正し、命令と同じアドレスをデータとして読む場合の誤読を防ぎました。通常のメモリー書込み時刻と、タイマー・映像・音源の進行も整えました。
* スプライトの選別・描画・衝突判定を整合させ、表示上限を超えた際の割り込みを改善しました。
* 映像の走査周期、表示・スクロール設定の反映時刻、パレットの白黒出力を改善しました。
* PSG・CD-DA・ADPCMのレート変換と波形履歴の保存・復帰を改善し、PSGの波形書込みや音量・ミュート処理も修正しました。
* 描画待ちからエミュレーションと音声生成を分離し、画面出力の遅延によって音声が途切れる問題に対処しました。

### 更新時の注意

**正式版1.1.0以前のステートは1.2.0では読み込めません。** 更新後はゲームを新規起動し、新しいステートを作成してください。`Accurate`と`Fast`のステートも相互に読み込めません。

ゲーム内セーブ用の`backup.dat`と`.ram`、設定用の`rxpce.json`は引き続き利用できます。更新前にアプリを終了し、これらのファイルをバックアップしてください。ステート復帰でゲーム内セーブ用RAMも巻き戻る点は従来どおりです。

対象はWindows 11 x64です。ROM・BIOS・CDイメージは同梱していません。詳しくは[README](README.md)と[操作説明書](MANUAL.md)を参照してください。

## English

Main changes since 1.1.0:

* Fixed CPU instruction fetching so that reading data from the instruction's address does not incorrectly reuse the opcode. Improved ordinary memory-write timing and its coordination with timers, video and audio.
* Aligned sprite selection, drawing and collision detection, and improved the interrupt raised when the sprite limit is exceeded.
* Improved video scan timing, display and scroll register timing, and monochrome palette output.
* Improved PSG, CD-DA and ADPCM resampling and restoration of waveform history. Fixed PSG waveform writes, volume and mute handling.
* Separated graphics presentation waits from emulation and audio generation to prevent delayed screen output from interrupting audio.

### Updating

**Save states from official releases 1.1.0 and earlier cannot be loaded in 1.2.0.** Start games normally and create new states after updating. `Accurate` and `Fast` states also remain separate.

In-game saves in `backup.dat` and `.ram`, and settings in `rxpce.json`, remain usable. Close the application and back up these files before updating. Loading a state still rolls back in-game save RAM.

Windows 11 x64 is supported. ROMs, BIOS files and CD images are not included. See the [README](README.md#english) and [user manual](MANUAL.md#english) for details.
