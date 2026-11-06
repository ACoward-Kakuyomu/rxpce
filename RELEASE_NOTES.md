# rxpce 1.5.1

[English translation](#english) follows the Japanese text.

## 日本語

1.5.0からの主な変更です。

* 入力リプレイを追加しました。F11で記録を開始・保存し、停止後にF12で再生できます。HuCard・CUE・CHDに対応します。
* 最近開いた最大10件のゲームを、`Game → Recent Games` から開き直せます。
* ゲームごとに入力プロファイルを割り当てられます。次回そのゲームを開くと割り当てた枠を適用し、普段選択している枠は維持します。
* ReleaseビルドのIPOを有効にしました。検証した速度制限なしの場面では、同じ区間の処理時間が約8〜11%短縮しました。改善幅は利用環境や場面によって異なります。
* **1.5.0のステートは引き続き読み込めます。正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** ゲーム内セーブと設定も引き続き利用できます。

## English

Changes since 1.5.0.

* Added input replay. Press F11 to start recording and save it, then press F12 while emulation is stopped to play it back. HuCard, CUE and CHD are supported.
* Reopen up to ten recent games from `Game → Recent Games`.
* Assign input profiles to individual games. Opening a game applies its assigned profile for that session while preserving the normally selected slot.
* Enabled interprocedural optimization (IPO) in Release builds. Processing time for the tested unlimited-speed scenarios decreased by approximately 8–11%. Results vary by system and scene.
* **Save states from 1.5.0 remain compatible. Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** In-game saves and settings remain usable.
