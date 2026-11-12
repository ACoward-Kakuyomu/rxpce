# rxpce 1.5.3

[English translation](#english) follows the Japanese text.

## 日本語

1.5.2からのバグ修正です。

* CDステートの保存先に多階層の未作成フォルダーを指定した場合も、必要なフォルダーを作成して保存できるよう修正しました。
* CUE変換時に、記録済みRAWプリギャップをそのまま保持するよう修正しました。データ／音声が混在する境界にも対応し、プログラム領域の検証は維持しています。
* ROMのサイズ検証を修正し、ヘッダーなしまたは512バイトヘッダー付きとして扱えないサイズを拒否するようにしました。
* 設定画面での保存後にショートカットのF2／F3／F4／F7で変更した場合、後から選んだ値が保存されるよう修正しました。
* メニューが折り返される場合も、指定倍率に対応したクライアント領域のサイズになるよう修正しました。
* **1.5.0／1.5.1／1.5.2のステートは引き続き読み込めます。正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** 入力構成とCD BIOSの一致は引き続き必要です。ゲーム内セーブ・設定・チート定義・リプレイ形式は変更していません。

## English

Changes since 1.5.2. This is a bug-fix release.

* Fixed CD save states failing when the destination requires multiple missing parent folders. The required folders are now created.
* Fixed CUE conversion to preserve stored RAW pregap sectors, including mixed data/audio boundaries, while retaining validation of program sectors.
* Fixed ROM size validation to reject images that cannot be interpreted as headerless or as having a 512-byte header.
* Fixed settings persistence so later changes made with the F2/F3/F4/F7 shortcuts take precedence over values saved in the settings dialog.
* Fixed window sizing with wrapped menus so the client area matches the selected scale.
* **Save states from 1.5.0, 1.5.1 and 1.5.2 remain compatible. Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** Matching input configuration and CD BIOS are still required. In-game saves, settings, cheat definitions and replay formats are unchanged.
