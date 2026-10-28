# CD取り込みツール / CD capture tool

[English translation](#english) follows the Japanese text.

## 日本語

`rxpce_cd_dump` は Windows 11 x64 用の独立した CUE＋BIN 取り込みツールです。リリースZIPに `rxpce_cd_dump.exe` と本書を同梱します。**基準USBドライブの43トラック媒体で、全範囲の取り込み・反復照合、生成CUEでの起動とCD音声を確認済みです。確認した機器は BUFFALO Optical Drive 1.05（USB接続）です。**

### このツールのライセンスと依存先

| 対象 | 表示・条件・原文 |
| --- | --- |
| 自前の取り込みコード・文書 | Copyright (C) 2026 ACoward。適用範囲は [LICENSE.md](LICENSE.md)、原文は [PolyForm Noncommercial 1.0.0](LICENSES/PolyForm-Noncommercial-1.0.0.txt) |
| picojson | Cybozu Labs, Inc. / Kazuho Oku。BSD-2-Clause。[ライセンスと著作権表示](third-party/picojson/LICENSE) |

エミュレーターのCPU・描画・音声出力やlibchdrにはリンクしません。既存のCUE読み込み層・libchdrを利用する結合テストは、取り込みツール本体とは別のターゲットです。CMakeでは既定で有効です。`RXPCE_BUILD_CD_DUMP=OFF` でビルド対象から外せますが、リリースZIPには必要です。

### 配布ZIPから実行

ZIPをフォルダーごと展開し、そのフォルダーでPowerShellを開きます。ドライブ文字と出力先は環境に合わせて変更してください。

```powershell
.\rxpce_cd_dump.exe list
.\rxpce_cd_dump.exe probe --drive E: --layout
.\rxpce_cd_dump.exe read --drive E: --output "D:\CD\new-capture"
```

### ビルドと調査

実行には Windows 11 x64、対応する光学ドライブと媒体、Visual C++ v14 の x64 ランタイムが必要です。ランタイムの導入先は [READMEの動作環境](README.md) を参照してください。実行ファイルの主な依存は Windows 標準 API と picojson です。ライセンスと著作権表示は [LICENSE.md](LICENSE.md)、[第三者表示](THIRD_PARTY_NOTICES.md)、[picojsonの原文](third-party/picojson/LICENSE)を保持します。

```powershell
cmake -S . -B build -A x64 -DRXPCE_BUILD_CD_DUMP=ON
cmake --build build --config Release --target rxpce_cd_dump --parallel
.\build\Release\rxpce_cd_dump.exe list
.\build\Release\rxpce_cd_dump.exe probe --drive E:
.\build\Release\rxpce_cd_dump.exe probe --drive E: --layout
```

`list` は光学ドライブの名前とデバイスパス、`probe` は媒体プロファイル、TOC、境界周辺のRAW/Q検証結果をJSONで表示します。`--drive` はドライブ文字または列挙したデバイスパスを指定できます。`--layout` はQを使ったINDEX 00/01の位置確認を追加します。

レポートを保存するときは標準出力をファイルへリダイレクトしてください。単なる `probe` の成功は全域を読み取れる保証ではありません。個別の失敗は `samples` 内の `error`、位置検出の失敗は `layout_error` に記録します。Qの非位置レコードとCRC不良は区別します。

### 取り込み

```powershell
.\build\Release\rxpce_cd_dump.exe read --drive E: --output "D:\CD\new-capture"
```

出力フォルダーは未作成、親フォルダーは既存である必要があります。空の既存フォルダーも拒否します。媒体に対する書き込みや自動イジェクトは行いません。利用可能なら取り出しを一時抑止し、終了時に解除を試みます。

INDEX 00/01の位置を確定した後、RAW2352バイトのトラック別BINへ読み取り、もう一度ドライブから全範囲を取得して比較します。初回は最大16セクター、照合時は最大15セクターで読み、要求境界を変えます。要求サイズが扱えない場合は小さく分割し、1セクターの失敗や不一致は追加3回まで再取得します。各パスの最初の音声と、データから音声へ切り替わる箇所では、別の既知の音声位置を1セクター事前取得してから対象位置へ戻します。事前取得したバイトは捨て、回数を `audio_prime_reads` に記録します。音声の開始位相を整えるための機器操作で、PCMの加工やオフセット校正は行いません。音声があるにもかかわらず別の音声位置がない構成は拒否します。再読込が保存済みの内容と一致しなければ完成品にしません。無音・ゼロの補完、多数決、原データの修復はしません。

作業中は出力先の親に `.rxpce-cd-partial-*` を作り、検証とフラッシュがすべて成功した後に正式なフォルダー名へ変更します。通常の失敗・中断では自分で作った一時ファイルだけを削除します。強制終了や後始末の失敗で残った一時フォルダーは完成品ではありません。出力先と同じボリュームに、約2352バイト×取り込みセクター数の空き容量が必要です。

完成出力は `disc.cue`、`track01.bin` 等、`capture.json` です。CUEとBINはフォルダー単位で扱ってください。JSONには位置取得の根拠、各BINのサイズ・SHA-256、再取得回数、照合状態、未対応範囲を記録します。基準媒体では全BINのサイズ・SHA-256を独立確認し、既存イメージとの共通本体、生成CUEでの起動・音声・ステート保存復帰と再起動後の復帰を確認しました。

### 範囲と制限

* 単一セッション、MODE1＋CD-DAの混在、最大99トラックを対象とします。PRE／DCP／4CH等、現在保持できないトラック制御フラグは拒否します。
* プログラム領域のLBA 0からリードアウト手前までを対象とします。先頭トラック前、隠し音声、リードイン／アウト全体、全サブチャンネルの保存は対象外です。
* 音声はPCM16 little-endianのステレオとしてそのまま保持します。音量調整・再サンプリング・ドライブ別オフセット校正は行いません。
* 読み取り時はMODE1／CD-DAを明示します。ギャップの形式はCRCを検証したQのCONTROLから事前取得し、データ読み取り失敗を音声として読み直して隠すことはしません。
* MODE1本体はヘッダー・EDC・P/Qパリティを検証します。ギャップ内のRAWバイトは実データを保持し、異なるモードを合成データへ置き換えません。
* `repeated_reads_match` は対応範囲の反復読込が一致したことを示します。ドライブのキャッシュを完全に回避した保証や、音声オフセットまで校正した保存を意味しません。
* CHD作成、外部ツールの起動、rxpce本体への実ドライブ接続は行いません。

進捗は標準エラー出力へ表示します。Ctrl+C／Ctrl+Breakで中止できます。処理中の機器要求が返るまで待つ場合があり、要求のタイムアウトは10秒です。終了コードは0＝成功、1＝処理・読込・照合の失敗、2＝引数誤り、3＝キャンセルです。

## English

`rxpce_cd_dump` is a standalone CUE/BIN capture tool for Windows 11 x64. The release ZIP includes `rxpce_cd_dump.exe` and this manual. **Full-disc capture and repeated-read verification, booting the generated CUE and CD audio have been confirmed on the 43-track reference disc and USB drive. The tested drive is a BUFFALO Optical Drive 1.05 connected over USB.**

### Licensing and dependencies of this tool

| Component | Notice, terms and full text |
| --- | --- |
| Original capture code and documentation | Copyright (C) 2026 ACoward. Scope: [LICENSE.md](LICENSE.md). Full text: [PolyForm Noncommercial 1.0.0](LICENSES/PolyForm-Noncommercial-1.0.0.txt) |
| picojson | Cybozu Labs, Inc. / Kazuho Oku. BSD-2-Clause. [License and copyright notices](third-party/picojson/LICENSE) |

The executable does not link the emulator's CPU, video/audio output or libchdr. Integration tests that use the existing CUE reader and libchdr are separate targets. The CMake target is enabled by default. Set `RXPCE_BUILD_CD_DUMP=OFF` to omit it from a build; it is required for release packaging.

### Run from the release ZIP

Extract the whole ZIP and open PowerShell in that folder. Adjust the drive letter and destination for your environment.

```powershell
.\rxpce_cd_dump.exe list
.\rxpce_cd_dump.exe probe --drive E: --layout
.\rxpce_cd_dump.exe read --drive E: --output "D:\CD\new-capture"
```

### Build and probe

Requires Windows 11 x64, a supported optical drive and disc, and the Visual C++ v14 x64 runtime linked in the [README](README.md#english). The executable uses Windows APIs and picojson. Retain [LICENSE.md](LICENSE.md), [third-party notices](THIRD_PARTY_NOTICES.md) and the [picojson license](third-party/picojson/LICENSE).

```powershell
cmake -S . -B build -A x64 -DRXPCE_BUILD_CD_DUMP=ON
cmake --build build --config Release --target rxpce_cd_dump --parallel
.\build\Release\rxpce_cd_dump.exe list
.\build\Release\rxpce_cd_dump.exe probe --drive E:
.\build\Release\rxpce_cd_dump.exe probe --drive E: --layout
```

`list` prints optical-drive names and device paths. `probe` prints JSON containing the media profile, TOC and sample RAW/Q checks. Select a drive letter or an enumerated device path with `--drive`. `--layout` additionally verifies INDEX 00/01 boundaries. Redirect standard output to save a report. A successful basic probe does not establish full-disc readability. Per-sample failures appear in `error` fields; layout failures appear in `layout_error`.

### Capture

```powershell
.\build\Release\rxpce_cd_dump.exe read --drive E: --output "D:\CD\new-capture"
```

The destination folder must not exist, even if empty; its parent must exist. The tool does not write to or automatically eject media. It attempts to prevent removal while working, if supported, and releases the lock on exit.

After establishing the layout, it captures 2352-byte RAW sectors into per-track BIN files, then reads the complete range again from the drive and compares it with the captured bytes. The capture uses batches of up to 16 sectors; verification uses up to 15 to change request boundaries. Failed batches are split; single-sector failures and mismatches allow three additional attempts. At the first audio range of each pass and after a data-to-audio transition, it reads one sector at a different known audio address, discards those bytes, then returns to the capture address. The report counts these reads as `audio_prime_reads`. This establishes audio decoding without editing PCM or calibrating drive offsets. If audio is present but no distinct audio address exists, the layout is rejected. It never substitutes silence, zero-fills errors, uses majority voting, or repairs original bytes.

Work is staged in a `.rxpce-cd-partial-*` folder beside the destination. Only a flushed and verified result is renamed to the final folder. Normal failures/cancellation clean up owned temporary files. A leftover stage after forced termination or cleanup failure is not a completed capture. Allow approximately 2352 bytes per captured sector on the output volume.

Output consists of `disc.cue`, per-track files such as `track01.bin`, and `capture.json`. Keep the folder together. The report includes boundary evidence, BIN sizes and SHA-256 hashes, retries, verification status and excluded scope. For the reference disc, BIN sizes and SHA-256 hashes were checked independently, all common track bodies matched the existing image, and the generated CUE passed boot, audio, state save/load and restoration after restarting the application.

### Scope and status

Supports single-session MODE1/CD-DA mixed-mode discs, up to 99 tracks. Unsupported track control flags, such as PRE/DCP/4CH, are rejected. The scope is program LBA 0 up to, but not including, leadout. Earlier/hidden audio, full lead-in/out and complete subchannels are excluded.

Audio is kept as stereo PCM16 little-endian, without volume changes, resampling or offset calibration. MODE1 or CD-DA is explicitly selected for each read. Gap read types are collected in advance from CRC-checked Q CONTROL fields; a data-read error is not hidden by retrying as audio. MODE1 body headers, EDC and P/Q parity are checked. Actual RAW gap bytes are preserved rather than replaced with synthesized sectors.

`repeated_reads_match` means repeated reads of the supported range matched. It does not guarantee bypassing drive caches or calibrated audio offsets. CHD creation, external conversion and direct-drive emulation are outside this tool.

Progress goes to standard error. Ctrl+C or Ctrl+Break requests cancellation; an in-flight device request may take up to its ten-second timeout to return. Exit codes: 0 success, 1 processing/read/verification failure, 2 invalid arguments, 3 cancellation.
