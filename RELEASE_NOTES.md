# rxpce 1.5.0

[English translation](#english) follows the Japanese text.

## 日本語

1.4.0からの主な変更です。

* 入力プロファイルを10個まで保存できます。停止中にメニューまたはAlt+1〜Alt+0で切り替え、設定画面では複数の枠をまとめて編集できます。
* Tフラグが有効な演算で、ソース読出しとダイレクトページの読出し・書込みを命令内の時刻に合わせました。
* SBCへTフラグのメモリー演算を誤適用していた処理を修正し、不要な追加メモリーアクセスと待ち時間を除去しました。
* 外部水平同期の解除後に同期幅を重ねて待つ処理を修正しました。長い同期幅設定でラスタ割り込みが消失し、背景や上部表示が崩れる問題を解消します。
* **正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** 更新後は新規開始してください。ゲーム内セーブと設定は引き続き利用できます。

## English

Changes since 1.4.0.

* Added ten input profiles. Switch profiles from the menu or with Alt+1 through Alt+0 while emulation is stopped, and edit multiple slots in one Settings session.
* Timed source reads and direct-page reads and writes within instructions that use the active T flag.
* Corrected the erroneous application of T-flag memory operations to SBC, removing unintended extra memory accesses and waits.
* Corrected a repeated sync-width delay after external horizontal sync release, preventing lost raster interrupts and corrupted backgrounds or upper-screen displays with long sync-width settings.
* **Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** Start normally after updating. In-game saves and settings remain usable.
