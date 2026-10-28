# rxpce 1.0.0

PCエンジンエミュレーター **rxpce** の導入案内です。アプリケーションのUIは英語です。[English translation](#english) follows the Japanese text.

詳しい操作・設定・保存ファイルの説明は[操作説明書](MANUAL.md)を参照してください。

ソースからのビルドは[ビルド手順](BUILDING.md)、プリギャップ付きCUE＋BINからCHDへの変換準備は[CUE変換補助ツール](CUE_MATERIALIZE.md)を参照してください。

## 日本語

### rxpceについて

rxpceは、かつて公開されていたxpceをWindows 11向けに改修したPCエンジンエミュレーターです。

1.0.0の主な対応範囲は次のとおりです。

| 項目 | 対応内容 |
| --- | --- |
| 本体 | PCエンジンのHuCard、CD-ROM²、SUPER CD-ROM² |
| HuCardファイル | `.pce`、ZIP内の最初の`.pce`。特殊HuCardのPopulous／Street Fighter II'にも対応 |
| CDイメージ | CUE＋BIN、親ファイルを必要としないCD用CHD v5 |
| 入力 | キーボード、XInput、DirectInput、1～5人、2／6ボタン |
| 表示 | Direct3D 11、整数倍・補間なし、ウィンドウ／ボーダーレス全画面 |
| 音声 | WASAPI／DirectSound、PSG・CD-DA・ADPCM |
| 保存 | ゲーム内セーブ用RAM、ステート保存・復帰 |

アーケードカード、スーパーグラフィックス、実CDドライブは対象外です。ROM、BIOS、CDイメージは同梱しません。

### 動作環境と導入

* **Windows 11 x64**。ARM64と32bit版Windowsは動作保証の対象外です。
* Direct3D 11を利用できる環境。設定の`Automatic`はハードウェアを優先し、失敗時はWARPによるソフトウェア描画を試します。
* Microsoft Visual C++ v14再頒布可能パッケージ **x64版**。未導入・古い場合は[Microsoftの公式配布案内](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)から最新の対応版を導入してください。
* キーボード。ゲームパッドは任意です。

配布ファイルをフォルダーごと展開し、`rxpce.exe`を起動してください。設定と共通のゲーム内セーブを書き込むため、展開先には書込み権限が必要です。`Program Files`などの保護された場所を避け、自分で書き込めるフォルダーを使ってください。

README、操作説明書、`LICENSE`、`LICENSES`、`THIRD_PARTY_NOTICES.md`と`third-party`内のライセンス文書も一緒に保管してください。ROMやBIOSをアプリのフォルダーへ置く必要はありません。

### 最初のゲームを始める

1. `Game → Open...`でHuCardの`.pce`または`.zip`を選びます。
2. 初期設定では方向キーで移動、`Z`／`X`でI／II、`Enter`でRun、`Tab`でSelectを操作します。
3. CDゲームでは、先に`View → Settings... → Folders → System Card BIOS (CD only)`で対応BIOSを指定し、`.cue`または`.chd`を開きます。BIOS画面ではRun（初期設定は`Enter`）を押してゲームを開始します。
4. ゲームパッドを使う場合は`Settings → Input`でプレイヤーの`Input device`を選択します。初期状態の入力デバイスは`Keyboard`です。

設定変更の多くは**次のエミュレーション開始時**に反映されます。`Esc`で停止し、ゲームを開き直してください。`Window scale`だけは`OK`確定時に反映されます。F10のResetは設定を適用し直す操作ではありません。

### よく使う操作

| キー | 操作 |
| --- | --- |
| F5／F6 | ステート保存／復帰（1スロット） |
| F8 | ウィンドウ倍率を1→2→3→4倍と切り替え |
| Alt+Enter | 全画面／ウィンドウ切替（ゲーム開始後） |
| F10 | 実行中ゲームのリセット |
| Esc | エミュレーションを停止し、開始前の画面へ戻る |
| Alt+F4 | アプリケーション終了 |

通常のゲーム速度で遊ぶ場合は`Automatic frame skip (60 Hz)`を有効にしてください。F2でこれを無効にすると速度制限が外れ、ゲームが高速に進みます。

### セーブデータと更新

* `rxpce.json`は設定、実行ファイルと同じフォルダーの`backup.dat`は全ゲーム共通の2 KiBバックアップRAMです。
* ステートとPopulousの`.ram`は、初期設定ではゲームイメージの隣に保存します。保存名・保存先は[操作説明書](MANUAL.md)に記載しています。
* **ステート復帰はゲーム内セーブ用RAMも巻き戻します。** 最新のゲーム内セーブを残す場合は、先に正常終了して保存ファイルをバックアップしてください。
* 更新・移動前にはアプリを終了し、`rxpce.json`、`backup.dat`、各ゲームの`.st0`／`.ram`をコピーして保管してください。

### 著作者・第三者ソフトウェア

Copyright (C) 2026 ACoward

Original fpce: BERO / Windows port xpce: hmmx

rxpceは非商用条件でソースを公開します。ACowardが権利を持つ追加・改修部分にはPolyForm Noncommercial 1.0.0、fpce／xpce由来部分には原許諾、外部コードには各ライセンスが適用されます。詳しくは[LICENSE](LICENSE)を参照してください。

依存ライブラリの著作権とライセンスは[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)および同梱のライセンス文書を参照してください。

## English

### About rxpce

rxpce is a PC Engine emulator adapted from the original xpce for Windows 11. The UI is in English. See the [user manual](MANUAL.md#english) for complete instructions.

See [Building](BUILDING.md#english) for source builds and the [CUE conversion helper](CUE_MATERIALIZE.md#english) for preparing CUE/BIN pregaps before CHD conversion.

| Feature | Support in 1.0.0 |
| --- | --- |
| Systems | PC Engine HuCard, CD-ROM² and SUPER CD-ROM² |
| HuCard files | `.pce`, or the first `.pce` inside a ZIP; special HuCard mappings for Populous and Street Fighter II' |
| CD images | CUE/BIN and standalone CD CHD v5 images |
| Input | Keyboard, XInput, DirectInput; 1–5 players; 2- and 6-button controllers |
| Video | Direct3D 11, integer scaling without filtering, windowed and borderless fullscreen |
| Audio | WASAPI or DirectSound; PSG, CD-DA and ADPCM |
| Saving | Game-save RAM and save states |

Arcade Card, SuperGrafx and physical CD drives are not supported. ROMs, BIOS files and CD images are not included.

### Requirements and installation

* **Windows 11 x64**. ARM64 and 32-bit Windows are outside the supported target.
* Direct3D 11 support. `Automatic` prefers hardware rendering and tries WARP software rendering if hardware initialization fails.
* Microsoft Visual C++ v14 Redistributable, **x64**. If missing or outdated, install the latest supported package from [Microsoft's official download page](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).
* A keyboard; a gamepad is optional.

Extract the complete distribution into a writable folder and run `rxpce.exe`. Avoid protected locations such as `Program Files`: settings and shared game-save RAM are written beside the executable. Keep the README, manual, `LICENSE`, `LICENSES`, `THIRD_PARTY_NOTICES.md` and the license documents in `third-party` together with the application. Game images and BIOS files may be kept elsewhere.

### Quick start

1. Choose a HuCard `.pce` or `.zip` through `Game → Open...`.
2. Default controls: arrow keys for directions, `Z`/`X` for I/II, `Enter` for Run and `Tab` for Select.
3. For a CD game, first select a supported BIOS under `View → Settings... → Folders → System Card BIOS (CD only)`, then open its `.cue` or `.chd`. Press Run (`Enter` by default) on the BIOS screen.
4. To use a gamepad, choose its `Input device` in `Settings → Input`. The default device is `Keyboard`.

Most settings apply at the **next emulation start**. Press `Esc` and reopen the image. `Window scale` applies when you confirm with `OK`. F10 Reset does not reapply pending settings.

| Key | Action |
| --- | --- |
| F5 / F6 | Save / load the single save-state slot |
| F8 | Cycle window scale: 1×, 2×, 3×, 4× |
| Alt+Enter | Toggle fullscreen after a game starts |
| F10 | Reset the running game |
| Esc | Stop emulation and return to the idle screen |
| Alt+F4 | Quit |

Keep `Automatic frame skip (60 Hz)` enabled for normal play. Disabling it with F2 removes the speed limit and makes the game run faster.

### Save data and updates

`rxpce.json` contains settings. `backup.dat`, beside the executable, is the shared 2 KiB backup RAM. Save states and Populous `.ram` files are stored beside the game image by default; see the [manual](MANUAL.md#english) for naming and locations.

**Loading a state also rolls back game-save RAM.** To preserve newer game saves, exit normally and back up the save files first. Before updating or moving the application, close it and copy `rxpce.json`, `backup.dat` and each game's `.st0`/`.ram` files to a safe location.

### Credits and third-party software

Copyright (C) 2026 ACoward

Original fpce: BERO / Windows port xpce: hmmx

rxpce makes its source available under noncommercial terms. ACoward-owned additions and modifications use PolyForm Noncommercial 1.0.0; inherited fpce/xpce material retains the original permission, and external code retains its own licenses. See [LICENSE](LICENSE) for scope.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the accompanying license documents for third-party copyright and license information.
