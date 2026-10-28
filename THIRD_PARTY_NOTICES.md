# 著作者・第三者ソフトウェア / Third-party notices

[English translation](#english) follows the Japanese text. 全体の適用範囲は [LICENSE.md](LICENSE.md) を参照してください。

## 日本語

### fpce / xpce

原作 fpce: BERO（1998）。Windows 移植版 xpce: hmmx（1998以降）。[原許諾文・出典・確認資料のハッシュ](LICENSES/fpce-xpce.txt)を収録しています。

原作由来の処理は現在の複数のファイルに分割・改修されています。以下は由来を示す主な所在であり、ファイル全体を原作または新規作品と分類する表ではありません。

| 対象 | 由来と扱い |
| --- | --- |
| `src/Pce.cpp`、`src/machine_devices.cpp`、`src/vdc_render.cpp` と関連する本体処理・定義 | 原作のエミュレーション処理を含む。BERO・hmmx の表示を保持し、原作部分には原許諾を適用 |
| `src/Sound.cpp` および旧ソースから引き継いだホスト処理・UI・リソースの部分 | 移植・改修後も原作由来部分には原許諾を適用。新規ファイル名であることだけでは新規著作物と扱わない |
| `src/res/chars.bmp` | 原作由来の画面内表示用文字画像。原許諾の対象として記録 |
| `src/res/rxpce.png`、`src/res/rxpce.ico` | rxpce 用に生成AIで作成した画像とその形式変換。由来は [画像資料](src/res/README.md)。ACoward が有する権利に限り PolyForm の対象 |
| ACoward が権利を持つ追加・改修部分、文書、テスト、ビルド・補助ツール | LICENSE.md に記載した PolyForm Noncommercial 1.0.0。原作・第三者の権利を含めて再許諾しない |

原作アーカイブに含まれる Marat Fayzullin / Alex Krasivsky の旧 m6502 は、現在のビルド・配布ソースには含めません。現在使用する Ben Zotto の MCS6502 とは別の実装です。旧アーカイブや開発履歴の配布を本書によって許可するものではありません。

### 外部ライブラリ

各ライセンス原文と著作権表示をそのまま保持します。これらを PolyForm に変更するものではありません。

| ライブラリ | 条件・同梱文書 |
| --- | --- |
| MCS6502 | Ben Zotto、MIT。[全文](third-party/MCS6502/LICENSE.TXT) |
| libchdr と miniz / LZMA / Zstandard / xxHash / dr_flac | BSD-3-Clause、MIT、パブリックドメイン、MIT No Attribution 等。[集約した全文・表示](third-party/libchdr/LICENSES.rxpce.txt)、[導入元](third-party/libchdr/README.rxpce.md) |
| Windows Template Library (WTL) 10.01 | Microsoft Public License。[全文](third-party/wtl/MS-PL.txt) |
| picojson | Cybozu Labs, Inc. / Kazuho Oku、BSD-2-Clause。[全文](third-party/picojson/LICENSE) |

ソース配布では各依存先のソース内表示も保持してください。バイナリー配布にも本書と上記のライセンス文書を同梱します。開発用資料や Git 履歴を公開先へ持ち込むこととは別の作業です。

## English

See [LICENSE.md](LICENSE.md) for the overall scope.

### fpce / xpce

Original fpce: BERO (1998). Windows port xpce: hmmx (1998 onward). The [original permission record](LICENSES/fpce-xpce.txt) includes sources and hashes of examined evidence.

Inherited implementation has been split and modified across files. Principal locations include `src/Pce.cpp`, `src/machine_devices.cpp`, `src/vdc_render.cpp`, related machine definitions, and inherited portions of sound, host, UI and resource code. A new filename does not make inherited code a new work. Original portions retain their original permission; ACoward-owned additions and modifications use PolyForm as described in LICENSE. These are portions of files, not a blanket classification of complete files.

`src/res/chars.bmp` is an inherited on-screen font image recorded under the original permission. `src/res/rxpce.png` and `rxpce.ico` are an AI-generated rxpce icon and its format conversion; see [resource provenance](src/res/README.md). PolyForm applies only to rights held by ACoward, if any.

The former m6502 by Marat Fayzullin / Alex Krasivsky, found in the original archives, is absent from current build and distribution sources. It is distinct from the MCS6502 by Ben Zotto used today. This document does not authorize distributing old archives or development history.

### External libraries

MCS6502: Copyright (c) 2019 Ben Zotto.

MCS6502 is provided under the MIT License with attribution. Preserve this
copyright notice in documentation or credits accompanying binary distributions.
See [the full license](third-party/MCS6502/LICENSE.TXT).

libchdr v0.3.0: Copyright Romain Tisserand; MAME CHD utilities:
Copyright Aaron Giles. BSD-3-Clause.

The bundled miniz 3.1.1 is shared by ZIP loading and libchdr (MIT;
Copyright 2013-2014 RAD Game Tools and Valve Software;
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC).
libchdr also uses LZMA SDK 25.01 (public domain), Zstandard 1.5.7 with
embedded xxHash (BSD license selected; Copyright Meta Platforms, Inc. and
affiliates, and Yann Collet), and dr_flac 0.13.3 (MIT No Attribution selected;
Copyright 2023 David Reid).
See [the complete dependency notices](third-party/libchdr/LICENSES.rxpce.txt)
and [source provenance](third-party/libchdr/README.rxpce.md).

Windows Template Library (WTL) 10.01 is provided under the Microsoft Public
License. See [the full license](third-party/wtl/MS-PL.txt).

picojson: Copyright 2009-2010 Cybozu Labs, Inc.; Copyright 2011-2014 Kazuho Oku.
BSD-2-Clause. See [the full license](third-party/picojson/LICENSE).

External libraries retain their own licenses, not PolyForm. Preserve their source
notices in source distributions. Include this document and the referenced license
texts in binary distributions. Handling development materials and Git history for
the separate public repository is a separate process.
