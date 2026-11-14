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
