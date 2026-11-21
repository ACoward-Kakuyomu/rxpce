# rxpce 1.7.0

[English translation](#english) follows the Japanese text.

## 日本語

1.6.0からの変更です。

PHPでstackに保存するBフラグと、SEI前からのIRQ要求が後発のIRQで失われる問題を修正しました。SEIが保持する要求集合を保存するため、HuCard/CD・Accurate/Fastの4種のステートGUIDを更新しています。

このほか、次のバグ修正を含みます。

* 描画リソースの部分的な生成失敗からの復旧と、最小化中のサイズ変更・全画面切替の扱いを修正しました。
* メモリーツールの準備・表示と他のダイアログが重なった際の入力抑止、ウィンドウの有効状態、カーソルの管理を修正しました。
* DirectSoundの一時的なバッファ喪失から再試行・再充填して復旧するようにし、復旧不能時には出力方式と対処方法を通知するようにしました。
* バックアップRAM等の同時保存で一時ファイルが競合し、保存内容が失われ得る問題を修正しました。
* 不整合なステートや不正な引数の検査を強化し、失敗時に既存の状態を保つ処理を改善しました。
* 最近使ったゲームの長いUnicodeパスの表示と、CD取り込みツールの出力先末尾区切り・デバイス情報の表示とJSON記録を修正しました。

性能上の留意点：測定したHuCard・Fastの一条件で、1.6.0に対するCPU時間の中央値が約14〜15%増加しました。検証条件では通常速度と音声供給を維持していますが、原因は未確定で、性能改善の別課題として残しています。環境・場面によって差は変わります。

**公開版1.6.0以前のステート・リプレイは読み込めません。** この変更前の開発版で作成したファイルも対象です。ゲームを新規起動し、F5のステートとF11の記録を作り直してください。ゲーム内セーブと設定、入力プロファイル、チート定義は引き続き利用できます。旧ファイルの移行・自動削除は行いません。同じ保存先へ保存すると上書きされるため、必要なファイルは更新前にバックアップしてください。

## English

Changes since 1.6.0.

This release fixes the B flag stacked by PHP and prevents later IRQs from cancelling a request retained across SEI. All four HuCard/CD and Accurate/Fast state GUIDs have changed to preserve the eligible request set.

Other bug fixes in this release:

* Fixed recovery from partial graphics-resource creation failures and handling of size changes and fullscreen transitions while minimized.
* Fixed input suppression, window enabled states and cursor handling when memory-tool preparation or display overlaps other dialogs.
* Added retry and refill after temporary DirectSound buffer loss, with backend-specific recovery guidance when audio output cannot recover.
* Fixed competing temporary files during concurrent backup RAM and cartridge RAM saves that could lose saved contents.
* Strengthened validation of inconsistent states and invalid arguments, and improved preservation of existing state on failure.
* Fixed long Unicode paths in the recent-games menu and trailing output separators, device-identity display and JSON recording in the CD capture tool.

Performance note: one measured HuCard/Fast case used approximately 14–15% more median CPU time than v1.6.0. Normal speed and audio supply were maintained under the tested conditions, but the cause remains unknown and is tracked as a separate performance task. Results vary with the environment and scene.

**States and replays from official releases up to 1.6.0 cannot be loaded.** Files created by development builds before this change are also incompatible. Start games normally and create new F5 states and F11 recordings. In-game saves and settings, input profiles and cheat definitions remain usable. Old files are not migrated or deleted automatically; back up any files you need before saving to the same paths.
