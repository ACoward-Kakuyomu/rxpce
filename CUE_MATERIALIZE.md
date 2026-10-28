# CUE 変換補助ツール / CUE conversion helper

[English translation](#english) follows the Japanese text.

## 日本語

### 目的

`cue_materialize.exe` は、CUE＋BIN のプリギャップを実データに展開するコマンドラインツールです。CUE の `PREGAP` と記録済みの `INDEX 00`／`INDEX 01` を整理し、トラックごとの BIN と `disc.cue` を新しいフォルダーに出力します。生成ギャップは音声なら無音、MODE1 データならゼロのデータ領域として生成します。MODE1/2352 ではヘッダー・EDC・ECC も生成します。

`PREGAP` と記録済みギャップが併存する CUE をそのまま CHD に変換すると、変換方法によっては `INDEX 01` の位置が失われる場合があります。このツールは事前にギャップを実体化し、位置を保持して変換するためのものです。元 CUE を rxpce で直接開く場合には不要です。CHD を直接作成する機能はありません。

### 変換手順

`cue_materialize.exe` と MAME の `chdman.exe` が現在のフォルダーにある場合の PowerShell の例です。chdman は別途用意してください。同梱・自動実行はしません。

```powershell
.\cue_materialize.exe "D:\CD\game.cue" "D:\CD\game-materialized"
```

成功したら、生成された CUE から CHD を作成・検証します。各コマンドが成功したことを確認してから次へ進んでください。

```powershell
.\chdman.exe createcd -i "D:\CD\game-materialized\disc.cue" -o "D:\CD\game.chd"
.\chdman.exe verify -i "D:\CD\game.chd"
```

`createcd` と `verify` のオプションは [MAME の公式 chdman 文書](https://docs.mamedev.org/tools/chdman.html)を参照してください。rxpce では親 CHD を必要としない CD 用 CHD v5 を使用します。変換後は実際の起動・音声も確認してください。`verify` は元 CUE とのトラック位置の一致まで保証するものではありません。

### 入出力と制限

* 引数は「入力 CUE」「新規出力フォルダー」の2つです。空白を含むパスは引用符で囲んでください。
* 出力フォルダーは**未作成**で、親フォルダーは既存である必要があります。空の既存フォルダーでも拒否します。元 CUE と BIN は変更しません。
* 出力は `disc.cue` と `track01.bin` などのトラック別ファイルです。CUE 内の BIN パスは相対パスなので、出力フォルダー全体をまとめて移動できます。
* 入力は `FILE ... BINARY`、`AUDIO`／`MODE1/2048`／`MODE1/2352` に対応します。少なくとも1本のデータトラックが必要です。単一 BIN・複数 BIN、記録済みギャップ・生成ギャップを扱います。音楽CDのみの構成、WAV／MP3、MODE2、POSTGAP などは対象外です。汎用 CUE 編集・修復ツールではありません。
* 元の BIN と別に、全トラックおよび生成ギャップ分の容量が必要です。CHD の出力にも別途容量が必要です。タイトル等の説明用メタデータは出力 CUE に引き継ぎません。
* 通常の処理エラーでは作成途中の出力を削除しますが、強制終了時には残る場合があります。既存フォルダーを再利用せず、新しい出力先を指定してください。
* 終了コードは `0` が成功、`1` が読込み・書込み等の失敗、`2` が引数の誤りです。エラー内容は標準エラー出力へ表示します。

CUE 版と CHD 版の rxpce ステートは共有しません。変換後は CHD 用のステートを新しく保存してください。ビルド方法は [BUILDING.md](BUILDING.md)、利用条件は [LICENSE.md](LICENSE.md) を参照してください。

## English

### Purpose

`cue_materialize.exe` materializes pregaps in CUE/BIN images. It combines generated `PREGAP` and stored `INDEX 00`/`INDEX 01` gaps into per-track BIN files and a new `disc.cue`. Generated audio gaps contain silence; MODE1 gaps contain zero-filled data. MODE1/2352 gaps also receive headers, EDC and ECC.

Direct conversion of a CUE containing both generated and stored pregaps can lose `INDEX 01` positions, depending on the conversion path. This helper prepares explicit gap data before CHD conversion. It is unnecessary when opening the original CUE directly in rxpce. It does not create CHD files itself.

### Conversion

PowerShell example with `cue_materialize.exe` and MAME's `chdman.exe` in the current folder. Obtain chdman separately; it is not bundled or run automatically.

```powershell
.\cue_materialize.exe "D:\CD\game.cue" "D:\CD\game-materialized"
```

After success, create and verify the CHD. Check each command's result before proceeding:

```powershell
.\chdman.exe createcd -i "D:\CD\game-materialized\disc.cue" -o "D:\CD\game.chd"
.\chdman.exe verify -i "D:\CD\game.chd"
```

See the [official MAME chdman documentation](https://docs.mamedev.org/tools/chdman.html) for options. rxpce accepts standalone CD CHD v5 images. Also check startup and audio after conversion; `verify` does not establish that track positions match the original CUE.

### Inputs, outputs and limits

* Supply two arguments: input CUE and new output folder. Quote paths containing spaces.
* The output folder must **not exist**; its parent must exist. Even an empty existing folder is rejected. Source CUE/BIN files are unchanged.
* Output consists of `disc.cue` and per-track files such as `track01.bin`. BIN references are relative; move the output folder as a unit.
* Supports `FILE ... BINARY` with `AUDIO`, `MODE1/2048` and `MODE1/2352`, single/multiple BIN files and stored/generated gaps. At least one data track is required. Audio-only discs, WAV/MP3, MODE2, POSTGAP and other unsupported directives are rejected. This is not a general CUE editor or repair tool.
* Allow space for all tracks plus generated gaps, as well as the resulting CHD. Descriptive metadata such as titles is not retained in the output CUE.
* Ordinary failures clean up incomplete output. Forced termination may leave files behind; use a new output folder for another attempt.
* Exit codes: `0` success, `1` processing failure, `2` incorrect arguments. Errors go to standard error.

rxpce save states are not shared between CUE and CHD versions. Save a new state after conversion. See [BUILDING.md](BUILDING.md#english) for building and [LICENSE.md](LICENSE.md) for terms.
