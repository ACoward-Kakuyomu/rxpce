# 開発中 / In development

1.6.0公開後の開発版です。PHPでstackに保存するBフラグと、SEI前からのIRQ要求が後発のIRQで失われる問題を修正しました。SEIが保持する要求集合を保存するため、HuCard/CD・Accurate/Fastの4種のステートGUIDを更新しています。

このほか、次のバグ修正を含みます。

* 描画リソースの部分的な生成失敗からの復旧と、最小化中のサイズ変更・全画面切替の扱いを修正しました。
* メモリーツールの準備・表示と他のダイアログが重なった際の入力抑止、ウィンドウの有効状態、カーソルの管理を修正しました。
* DirectSoundの一時的なバッファ喪失から再試行・再充填して復旧するようにし、復旧不能時には出力方式と対処方法を通知するようにしました。
* バックアップRAM等の同時保存で一時ファイルが競合し、保存内容が失われ得る問題を修正しました。
* 不整合なステートや不正な引数の検査を強化し、失敗時に既存の状態を保つ処理を改善しました。
* 最近使ったゲームの長いUnicodeパスの表示と、CD取り込みツールの出力先末尾区切り・デバイス情報の表示とJSON記録を修正しました。

性能上の留意点：測定したHuCard・Fastの一条件で、1.6.0に対するCPU時間の中央値が約14〜15%増加しました。検証条件では通常速度と音声供給を維持していますが、原因は未確定で、性能改善の別課題として残しています。環境・場面によって差は変わります。

**公開版1.6.0以前のステート・リプレイは読み込めません。** この変更前の開発版で作成したファイルも対象です。ゲームを新規起動し、F5のステートとF11の記録を作り直してください。ゲーム内セーブと設定、入力プロファイル、チート定義は引き続き利用できます。旧ファイルの移行・自動削除は行いません。同じ保存先へ保存すると上書きされるため、必要なファイルは更新前にバックアップしてください。

This development build after v1.6.0 fixes the B flag stacked by PHP and prevents later IRQs from cancelling a request retained across SEI. All four HuCard/CD and Accurate/Fast state GUIDs have changed to preserve the eligible request set.

Other bug fixes in this development build:

* Fixed recovery from partial graphics-resource creation failures and handling of size changes and fullscreen transitions while minimized.
* Fixed input suppression, window enabled states and cursor handling when memory-tool preparation or display overlaps other dialogs.
* Added retry and refill after temporary DirectSound buffer loss, with backend-specific recovery guidance when audio output cannot recover.
* Fixed competing temporary files during concurrent backup RAM and cartridge RAM saves that could lose saved contents.
* Strengthened validation of inconsistent states and invalid arguments, and improved preservation of existing state on failure.
* Fixed long Unicode paths in the recent-games menu and trailing output separators, device-identity display and JSON recording in the CD capture tool.

Performance note: one measured HuCard/Fast case used approximately 14–15% more median CPU time than v1.6.0. Normal speed and audio supply were maintained under the tested conditions, but the cause remains unknown and is tracked as a separate performance task. Results vary with the environment and scene.

**States and replays from official releases up to 1.6.0 cannot be loaded.** Files created by development builds before this change are also incompatible. Start games normally and create new F5 states and F11 recordings. In-game saves and settings, input profiles and cheat definitions remain usable. Old files are not migrated or deleted automatically; back up any files you need before saving to the same paths.

The release notes below describe the published versions. / 以下は公開済みバージョンの記録です。

# rxpce 1.6.0

[English translation](#english) follows the Japanese text.

## 日本語

1.5.3からの変更です。

* 全体とPSG／CD-DA／ADPCMの音量を個別に調整できるようになりました。すべて初期値は100%です。音源別の音量はSettingsのAudioタブで設定し、全体音量・MuteはView → Audioからも変更できます。保存成功後に反映します。音量切替時の急な変化を抑える処理も追加しました。
* ExplorerからPCE／ZIP／CUE／CHDを1個ドラッグ＆ドロップして開けるようになりました。停止中・通常プレイ中、ウィンドウ・全画面に対応します。ダイアログ・メモリーツール・リプレイ記録／再生・未保存記録などの処理中は受け付けず、後から自動実行する予約もしません。
* マスクされたIRQが、別の有効なIRQの受付を遅らせる問題を修正しました。
* WASAPI／DirectSoundの初期化に失敗した場合、通知に出力方式と復旧方法を表示するようにしました。
* エミュレーション開始時にFPS集計をリセットし、停止前の時間が次のセッションの表示へ混ざらないようにしました。

**公開版1.5.3以前のステート・リプレイは読み込めません。** IRQ受付の意味互換性のため、HuCard／CD・Accurate／Fastのステート4種のGUIDを更新しています。ゲームを新規起動し、F5のステートとF11の記録を作り直してください。旧ファイルは自動移行・削除しません。同じ保存先への保存で上書きされるため、更新前にバックアップしてください。

ゲーム内セーブ（`backup.dat`・`.ram`）、設定、入力プロファイル、チート定義は引き続き利用できます。入力構成とCD BIOSの一致は引き続き必要です。Accurate／Fast、およびCUE／CHDのステートは共有できません。

## English

Changes since 1.5.3.

* Added master and independent PSG/CD-DA/ADPCM volume controls, all defaulting to 100%. Set individual levels in the Audio tab in Settings; View → Audio also provides master volume and Mute controls. Changes apply after a successful save, with a gradual gain transition to reduce abrupt output changes.
* Added drag and drop from Explorer for one PCE/ZIP/CUE/CHD file. It works while idle or playing, in windowed or fullscreen mode. Drops are rejected during dialogs, memory tools, replay recording/playback or pending recording saves, and other busy operations; rejected drops are never queued for later execution.
* Fixed masked IRQ edges delaying the acceptance of another eligible IRQ.
* Added audio initialization failure notifications identifying WASAPI or DirectSound and suggesting recovery steps.
* Reset FPS accounting at each emulation start so time from the previous session does not affect the new session's display.

**States and replays from official releases up to 1.5.3 cannot be loaded.** All four HuCard/CD and Accurate/Fast state GUIDs were updated for the IRQ handling change's semantic compatibility. Start games normally and create new F5 states and F11 recordings. Old files are not migrated or deleted automatically; saving to the same path can overwrite them, so back up files before updating.

In-game saves (`backup.dat` and `.ram`) and settings, input profiles and cheat definitions remain usable. Matching input configuration and CD BIOS are still required. Accurate/Fast and CUE/CHD states remain separate.
