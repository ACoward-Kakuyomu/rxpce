# rxpce 1.5.1 操作説明書 / User Manual

rxpce 1.5.1では、入力リプレイ、最近使ったゲーム、ゲーム別の入力プロファイルを追加し、Releaseビルドの最適化を有効にしました。 **1.5.0のステートは引き続き読み込めます。正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** ゲーム内セーブと設定も引き続き利用できます。

rxpce 1.5.1 adds input replay, recent games and game-specific input profiles, and enables Release build optimization. **Save states from 1.5.0 remain compatible. Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** In-game saves and settings remain usable.

導入と動作環境は[README](README.md)を参照してください。UIの英語表記をそのまま使って説明します。[English translation](#english) follows the Japanese text.

## 日本語

実CDからCUE＋BINを作成する独立ツールの操作は [CD_DUMP.md](CD_DUMP.md) を参照してください。

### 1. 起動とゲームの開き方

`rxpce.exe`を起動し、`Game → Open...`でイメージを選択します。別のイメージを開くと、現在のエミュレーションを停止して新しいゲームを開始します。ファイル選択のキャンセルは、現在のゲームを切り替えません。

| 形式 | 開くファイルと注意点 |
| --- | --- |
| HuCard | `.pce` |
| HuCardのZIP | `.zip`。アーカイブ内で最初に見つかる`.pce`だけを使用します。複数ゲームの選択画面はありません |
| CUE＋BIN | `.cue`を開きます。参照されるBINも必要です。CUEとBINの位置・名前の対応を保ってください |
| CHD | 単独で読み込めるCD用CHD v5の`.chd`。親CHDを必要とするものやハードディスク用CHDは対象外です |

CUEの対応トラックは`AUDIO`、`MODE1/2352`、`MODE1/2048`、参照ファイルは`BINARY`形式です。WAV/MP3を参照するCUEや、CDイメージを入れたZIPは対象外です。生のBINだけではトラック構成が分からないため、CUEを使用してください。

コマンドラインではイメージのパスを1つ指定できます。空白を含むパスは引用符で囲みます。

```powershell
.\rxpce.exe "D:\Games\Example.pce"
.\rxpce.exe "D:\Games\CD\Example.cue"
.\rxpce.exe "D:\Games\CD\Example.chd"
```

### 2. CDゲームとSystem Card BIOS

1. `View → Settings...`を開きます。
2. `Folders → System Card BIOS (CD only)`の`...`からBIOSを選択し、`OK`で保存します。
3. `Game → Open...`でCUEまたはCHDを開きます。
4. `Opening CD image`画面でBIOSとディスク全体の検証が終わるまで待ちます。中止するときはキャンセルします。

CDの検証を始める前に現在のエミュレーションを停止します。検証のキャンセルや失敗後は停止状態になるため、続ける場合はイメージを開き直してください。
5. BIOS画面でRun（初期設定は`Enter`）を押します。

新しい`backup.dat`では、ゲーム内セーブを使う前にBIOS側でバックアップRAMの初期化を求められる場合があります。システムカード3.0では、BIOS画面でSelect（初期設定は`Tab`）を押し、「全体の初期化」を選択します。この操作は共有バックアップRAM内のゲーム内セーブをすべて消去するため、既存データを残す場合は実行しないでください。

対応BIOSは、日本向けSystem Card 1.0／2.0／2.1とSUPER System Card 3.0です。SUPER CD-ROM²には3.0を使用してください。本体256 KiBの対応BIOSを内容で照合します。512バイトのヘッダー付きも受け付けますが、384 KiBのオーバーダンプは受け付けません。BIOS自体は同梱しません。

CUE版とCHD版のステートは共有できません。CHDで問題が出る一方で元のCUEが動く場合は、まず元のCUEで確認してください。特に`PREGAP`と`INDEX 00/01`を併記したCUEは、外部変換時にトラック位置が変わる場合があります。rxpceにイメージの自動修復・CHD変換機能はありません。

### 3. メニューと操作キー

| メニュー | キー | 動作 |
| --- | --- | --- |
| Game → Open... | — | イメージを選択して開始 |
| Game → Save State | F5 | 現在の状態を保存。既存スロットを上書き |
| Game → Load State | F6 | 保存済み状態へ復帰 |
| Game → Reset | F10 | 実行中ゲームをリセット |
| Game → Stop Emulation | Esc | 停止し、開始前の黒い画面へ戻る |
| Game → Exit | Alt+F4 | アプリケーション終了 |
| Game → Replay → Start / Stop Recording | F11 | リプレイの記録を開始・終了 |
| Game → Replay → Play / Stop Replay | F12 | 停止中に再生を開始、再生中に停止 |
| View → Window Scale | F8 | ウィンドウ倍率を1→2→3→4倍と循環 |
| View → Fullscreen | Alt+Enter | 全画面／ウィンドウ切替 |
| View → Input Profiles | Alt+1〜Alt+0 | 停止中に入力プロファイル1〜10を選択 |
| View → Settings... | — | 設定画面 |
| View → About rxpce... | — | バージョン・著作者表示 |

| その他のキー | 動作 |
| --- | --- |
| F2 | 自動フレームスキップの有効／無効 |
| F3／F4 | フレームスキップ値を1減らす／増やす（0～15） |
| F7 | FPS表示の有効／無効 |

F8は開始前でも使えますが、全画面中は無効です。保存・復帰・リセット・停止・全画面切替はゲーム開始後に使えます。操作キーの押しっぱなしは繰り返し処理しません。設定などのダイアログ中は、これらのゲーム操作を行いません。Alt+F10ではリセットしません。

メニューは`Alt+G`でGame、`Alt+V`でViewを開けます。開いたメニュー内では下線付きの文字で項目を選べます。表示される下線はWindowsの設定やAltキーの状態によって変わります。

F10は現在のゲームをリセットする操作です。別のイメージを開いたり、設定の未適用項目を適用し直す操作ではありません。設定を適用するにはEscで停止してからゲームを開き直してください。

### 4. 既定のキーボード操作

| ゲームの操作 | 既定キー | 使用するモード |
| --- | --- | --- |
| 方向 | 矢印キー | 2／6ボタン |
| Run | Enter | 2／6ボタン |
| Select | Tab | 2／6ボタン |
| I／II | Z／X | 2／6ボタン |
| I rapid／II rapid（連射） | A／S | 2ボタン |
| III | C | 6ボタン |
| IV／V／VI | A／S／D | 6ボタン |

`Input`タブで変更できます。6ボタンモードでは連射を使わず、A/SはIV/Vになります。方向の上下、または左右を同時に入力した場合は、その軸を中立として扱います。

### 5. ゲームパッドと複数人設定

`View → Settings... → Input`で設定します。

1. `Players`をゲームに合わせて1～5にします。
2. `Player`で設定するプレイヤーを選びます。
3. `Buttons`を`2 buttons`または`6 buttons`にします。6ボタン対応ゲームで使用するときに6を選択してください。
4. `Input device`で`Keyboard`、`XInput 1`～`XInput 4`、または`DirectInput: デバイス名`を選びます。
5. 各割当欄を選び、キー／パッドボタンを押すか、一覧から選びます。押下による割当後は次の欄へ移動します。
6. `OK`で保存し、ゲームを開き直します。

初期設定は`Keyboard`です。`Advanced`の`XInput support`は有効、`DirectInput support`は無効です。DirectInputの機器を選ぶには同チェックを有効にしてください。両方式を有効にすると同じ実機が両方の一覧に出る場合があるため、各プレイヤーで使う入力を選択します。

XInputは製品名ではなく`XInput 1 (Connected)`のように接続状態を表示します。XInputだけで同時に使える枠は4つです。DirectInputはデバイス名を表示します。XInputの方向入力は十字キーと左スティック、DirectInputはX/Y軸とハットスイッチです。

| ゲームの操作 | 既定のXInputボタン |
| --- | --- |
| I／II | A／B |
| Run／Select | Start／Back |
| I rapid／II rapid | LB／RB（2ボタン時） |
| III／IV／V／VI | X／Y／LB／RB（6ボタン時） |

割当欄の`Left stick`／`Right stick`はスティックの押込みボタンです。LT/RTも割り当てられます。複数プレイヤーに同じキーボードの初期配置を使うと、同じキーで複数人が反応します。プレイヤーごとにキーやデバイスを分けてください。

### 6. 設定の適用時期

`OK`は設定を保存し、`Cancel`は変更を破棄します。倍率とアスペクト比を除く設定は、次のエミュレーション開始時に反映します。Escで停止してから、同じイメージを開き直せます。F2/F3/F4/F7での実行中変更は、その場で反映します。

#### Video

| 項目 | 内容 |
| --- | --- |
| D3D11 device | 初期値は`Automatic (hardware / WARP fallback)`。描画の問題を切り分ける場合は`WARP (software)`でも確認できます |
| Window scale | 1～4倍。OK時に反映。全画面中はウィンドウへ戻る際の倍率として保持 |
| Aspect ratio | Square Pixels（既定）／TV (4:3)。OK時に反映 |
| Automatic frame skip (60 Hz) | 初期値は有効。約60 Hzを目標に速度を制御し、処理が間に合わないときに描画を追加スキップ |
| Frame skip (0-15) | 次に描画するまでにスキップする枚数。自動時は最低スキップ枚数、非自動時は固定枚数 |
| Palette file (empty = linear RGB) | 外部パレット。512色×RGB各1バイト、1536バイト。未指定・サイズ不正・読込み失敗では標準RGBを使用。ゲームが白黒出力を選択した場合はパレット適用後のRGBを白黒化 |

**自動フレームスキップを無効にすると速度制限が外れます。** スキップ値0でもゲームが高速に進みます。通常のプレイでは有効にしてください。負荷が高すぎる場合は自動でも通常速度を維持できないことがあります。

`Square Pixels` は元の画素を正方形として整数倍に拡大します。ウィンドウは320×240以上で小さい映像を中央配置し、512画素幅などでは領域を広げます。全画面は収まる最大の整数倍率です。

`TV (4:3)` はコアが出力する可視画像全体を4:3へ補正します。ウィンドウは320×240を基準とした1～4倍で、ゲーム側の解像度が変わっても同じ大きさを保ちます。全画面は領域に収まる最大の4:3矩形へ表示し、余白は黒です。1920×1080では1440×1080の映像と左右240画素の黒帯になります。

どちらも補間なしです。TV表示は元の画素の整数倍ではなく、512画素幅の1倍表示などでは縮小が入り、細い線や文字の見え方が変わります。必要に応じて倍率を上げてください。画像内の黒枠の自動除去、追加のオーバースキャン、実機映像信号の厳密な再現やCRTフィルターは行いません。表示設定はステート復帰では巻き戻しません。

#### Audio

| 項目 | 内容 |
| --- | --- |
| Enable audio | 音声出力の有効／無効 |
| WASAPI (shared)／DirectSound | 初期値はWASAPI。出力方式を選択 |
| Sample rate (Hz) | 8000、11025、12000、16000、22050、24000、32000、44100、48000。初期値44100 |
| Channels | Mono／Stereo。初期値Stereo |
| Latency (20-160 ms) | 初期値40 ms。小さくすると遅延が減りますが、途切れる場合は大きくして確認 |
| DirectSound level | DirectSound専用。0: Write primary、1: Exclusive、2: Priority。初期値2 |

Monoは左右の音声の平均です。音声のレート変換による遅延が出力バッファとは別に加わります。追加分は44100 Hzで約0.73 ms、8000 Hzで最大4 msです。Latencyを小さくする場合は、音切れがないか確認してください。

#### Input

ゲームごとに使い分ける入力設定を10枠まで保存できます。ゲームを実行中ならEscで停止し、`View > Input Profiles` または `Alt+1`〜`Alt+0` で選びます。`Alt+0` はスロット10です。選択中の枠にはチェックが付き、次回起動時にも復元されます。未使用枠を初めて選ぶと、直前の枠の入力設定がコピーされます。

`Settings > Input` の `Profile` で編集する枠を選び、`Name`（最大30文字）、プレイヤー数、入力機器、2／6ボタン、ボタンと連射の割り当てを編集します。ここでの枠移動ではゲームで使用する枠は変わりません。移動しても編集中の変更は保持され、OKでまとめて保存、Cancelで破棄します。空欄には `Profile 1` などの既定名を使います。保存した名前はメニューにすぐ反映され、割り当ては次回のエミュレーション開始時に適用されます。

映像・音声、XInput／DirectInputサポート、保存先などは全枠で共通です。ゲーム内セーブとステートのファイル名はプロファイルを切り替えても同じです。ステートを復帰するには、保存時と同じプレイヤー数と2／6ボタン構成を選んでください。構成が異なる枠でもF5は同じファイルを上書きします。

パッド設定の手順と初期配置は5節を参照してください。

#### 最近使ったゲーム

`Game → Recent Games` から、最近開いた最大10件のゲームを再度開けます。停止中に入力プロファイルを選び、`Game → Assign Selected Profile to Last Game` を選ぶと、最後に開いたゲームにその枠を割り当てます。次回そのゲームを開くと割り当てた枠が使われます。`Game → Clear Last Game Profile` で解除できます。関連付けはファイルの場所ごとに保存されます。

#### Folders

| 項目 | 内容 |
| --- | --- |
| ROM folder | ファイル選択の開始フォルダー |
| State / RAM folder | ステートとPopulous RAMの保存先。空欄または`.`はイメージの隣 |
| System Card BIOS (CD only) | CD用BIOSのパス |
| Allow network CD images | 既定OFF。次のCD読込みから、画像本体とCUEの参照先をネットワーク上から開くことを許可 |

`.`以外の相対フォルダーは、プロセスの作業フォルダーを基準にします。保存先を一定にしたい場合は、書込み可能な絶対パスを指定してください。`backup.dat`の場所はこの設定では変わりません。

CDの入力画像とCUEの参照先は、既定では通常のローカルドライブだけを開きます。ローカルの親相対/絶対パス、対応するジャンクション/シンボリックリンクは利用できます。遠隔の割当ドライブ、UNC、不明なデバイス/リンク形式は拒否します。信頼したネットワーク媒体を使う場合だけ`Allow network CD images`を有効にしてください。許可はCUE内の全参照先へ適用され、最初のサーバだけに限定されません。現在マウント中の媒体は切り替わりません。Windowsのファイルダイアログ自体、BIOSやROM/ZIP、出力先のネットワークアクセスを制御する設定ではありません。

#### Advanced

| 項目 | 内容 |
| --- | --- |
| Base clock (Hz) | 初期値7,170,000。通常は変更しません。範囲1,000,000～85,920,000、1000 Hz単位に切捨て |
| VDC emulation | `Accurate`（既定）は段階DMA・CPU競合待ちを再現。`Fast`は一括転送と簡略タイミングで速度を優先。次回エミュレーション開始時に反映 |
| Enforce hardware sprite limit | 既定はチェックあり。外すとSATB内の最大64スプライトを描画対象とし、取得枠による欠落を減らします。IRQ・衝突・DMA待機は変わりません。次回エミュレーション開始時に反映。ステートは両設定で共用します |
| Enable diagnostic log | 詳細ログを有効化。初期値は無効 |
| Show FPS | FPS表示。初期値は無効。実行中はF7でも変更可能 |
| XInput support／DirectInput support | パッド入力方式の有効／無効。初期値はXInputのみ有効 |

詳細ログが無効でも、音声の初回障害・エラーの変化・回復は記録します。同じWASAPIエラーは繰返し件数を要約します。ログを書き込めない間は再試行を抑制し、一部の記録を破棄するため、完全な履歴は保証しません。

VDC emulationは処理負荷とタイミングモデルの選択です。FastでもAutomatic frame skipが有効なら通常速度（約60 Hz）を維持します。ゲームの表示や動作に差がある場合はAccurateを選び、ゲームを開き直してください。

Accurateは走査線の途中のパレット変更を追加遅延0の近似で反映します。Fastは行頭の色をその行全体へ使います。ステート復帰時は不完全な表示フレームを破棄し、次の完全なフレームから表示を再開します。表示区間がフレーム境界をまたぐ場合は、その区間が完成するまで提示を待ちます。

通常速度の待機は、映像時計21,477,270 Hz・1行1365クロックで実際に完了したフレーム時刻に従います。VCEの指定により262行（約60.054 Hz）または263行（約59.826 Hz）となり、途中で行数が変わる場合も時刻の端数を累積します。映像時計はBase clockから独立しています。

Base clockはCPU基準の進行速度を設定します。映像時計との相対速度が変わり、PSG・タイマーの進行にも影響します。CD・音声出力はCPU時刻とこの周波数から実時間へ換算します。通常は初期値を使用してください。ホストPCの性能を上げるための項目ではありません。

### 7. 保存・復帰とファイルの場所

#### 入力リプレイ

HuCardまたはCDのプレイ中にF11で記録を開始し、もう一度F11を押して `.rpl` に保存します。保存ダイアログを取り消した場合は、`Game > Replay > Save Pending Recording...` から再試行できます。停止中にF12を押し、リプレイと対応するゲームを選ぶと再生します。再生を中止するにはF12またはEscを押します。

記録時と同じROM、または同じ形式・内容のディスクと同じSystem Card BIOSが必要です。CUEとCHDはそれぞれ別のリプレイとして扱います。再生中は記録された操作を使い、ゲーム内セーブは一時コピーで扱います。共有するリプレイには記録開始時のゲーム内セーブ内容が含まれます。再現に差が出た場合は `rxpce.log` に照合点を記録します。

ステートは**VDCモードごとに1スロット**です。F5は上書き、F6は復帰です。ゲームを開いて実行可能な状態にしてから操作します。再起動後も同じゲームを開いてF6で復帰できます。

| ファイル | 内容 | 保存先／名前 |
| --- | --- | --- |
| rxpce.json | 設定 | 実行ファイルの隣 |
| backup.dat | 全ゲーム共通のバックアップRAM（2 KiB） | 実行ファイルの隣 |
| HuCard／ZIP／CUEのステート | 実行状態 | `Example.pce`／`Example.zip`／`Example.cue` → `Example.st0` |
| CHDのステート | 実行状態 | `Example.chd` → `Example.chd.st0` |
| Fastのステート | 速度優先モードの実行状態 | `.st0`の代わりに`.fst0`。CHDは`Example.chd.fst0` |
| Populous RAM | カートリッジRAM（32 KiB） | `Example.pce`／`Example.zip` → `Example.ram` |
| rxpce.log／rxpce.log.bak | 診断ログ／前回のログ | 実行ファイルの隣 |

ステートと`.ram`の保存先は`State / RAM folder`に従います。ZIPの保存名はZIP自体の名前を使い、内部PCEの名前は使いません。同じ保存先に同じベース名のPCE/ZIP/CUEを置くとステート名が重複するため、別のフォルダーや名前を使ってください。CUEとCHDは保存名も識別も別で、相互復帰はできません。

ゲーム内セーブとステートは別の機能です。ゲーム内で保存したRAMは、Esc停止・ゲーム切替・正常終了の際にファイルへ保存します。強制終了では最新の内容が残らない場合があります。

**F6による復帰は、共有バックアップRAMとPopulous RAMも保存時点へ戻します。** その後に停止・終了すると、戻った内容が永続ファイルに保存されます。共有RAMには他のゲームのセーブも含まれます。新しいセーブを残す場合は、先に正常終了して保存ファイルをバックアップしてください。

AccurateとFastは別々のステートを保存し、相互に復帰できません。ファイル名を変更しても形式GUIDで拒否します。実行中にVDC emulationを変更しても、現在のモードと保存先は次回開始まで変わりません。`backup.dat`と`.ram`のゲーム内セーブは両モードで共通です。

CDの復帰には同じBIOS・同じディスク・同じ構成が必要です。プレイヤー数、2／6ボタン、Base clockなどを変えると復帰を拒否する場合があります。ファイル名を変えて別のゲームやCUE/CHDへ流用しないでください。旧xpceや形式の異なる開発版ステートの移行は行いません。

### 8. 更新・設定の初期化

**正式版1.4.0以前、および保存形式が異なる開発版のステートは読み込めません。** 該当する版から更新する場合はゲームを新規起動して新しいステートを作成してください。変更点は[リリースノート](RELEASE_NOTES.md)を参照してください。

転送途中の保存・復帰とHuCardのPSG位相保存に対応し、ステートはHuCard/CD別の固定GUIDで形式を識別します。VDC DMA・CPUのVRAM保留アクセス・VCE位相の保存に伴いHuCard/CDの形式GUIDを更新しています。異なるGUIDのステートは復帰できません。`backup.dat`と`.ram`のゲーム内セーブは引き続き利用できます。

特殊描画モード・VRAM書込み規則・行内パレット反映と復帰表示の改善でも、Accurate/FastそれぞれのHuCard/CD形式GUIDを更新しています。この変更前のステートは読み込めないため、更新後はゲームを新規起動してステートを作成してください。

更新前にはアプリを終了し、`rxpce.json`、`backup.dat`、ゲームごとの`.st0`／`.fst0`／`.ram`をバックアップします。実行ファイルを別フォルダーへ移すと、設定と共通バックアップRAMを探す場所も変わります。

設定だけを初期化する場合は、終了後に`rxpce.json`を別名で退避して起動します。正常終了時に新しい設定ファイルを保存します。ゲーム内セーブを残す場合は`backup.dat`や`.ram`を削除しないでください。旧`xpce.ini`／`xpce.json`は自動読込みしません。

### 9. 困ったとき

| 症状 | 確認すること |
| --- | --- |
| MSVCP140.dll／VCRUNTIME140.dllなどの不足で起動しない | [READMEの動作環境](README.md)からMicrosoft公式のVisual C++ v14 x64ランタイムを導入 |
| ゲームが速すぎる | F2またはVideo設定でAutomatic frame skipを有効にする |
| パッドが反応しない | Input device、Players、Advancedの入力方式、Connected表示を確認。OK後にゲームを開き直す |
| 6ボタンが使えない | 対応ゲームであることと、各PlayerのButtons設定を確認 |
| 設定を変えても反映しない | Window scaleとAspect ratio以外はEsc停止後に開き直す。F10のResetだけでは未適用設定は反映しない |
| 音が出ない／途切れる | Enable audio、Windowsの音量/出力先を確認。Latencyを増やして開き直す。別の出力方式でも確認 |
| CDが開けない | 対応BIOSとサイズ、CUEが参照するBIN、対応CHDかを確認。検証完了まで待つ |
| CHDだけ起動しない／音声位置がおかしい | 元CUEで確認。変換時のトラック位置とプリギャップの保持を確認 |
| 設定やRAMを保存できない | 実行ファイルのフォルダーとState / RAM folderの書込み権限・空き容量を確認 |
| ステートを読めない | 同じゲーム/BIOS/形式と、作成時の入力構成・Base clockを確認 |
| backup.datを読めない | 原本を退避してサイズを確認。2 KiB以外は読み込まず、そのセッションでは既存ファイルを上書きしない |

再現状況を記録するときは、rxpceの版、Windows環境、ゲーム名・版、イメージ形式、操作手順、入力/音声/倍率/速度設定を揃えてください。詳細ログが必要な場合はAdvancedで有効にし、ゲームを開き直して再現します。ログにはファイルパスが含まれるため、共有前に内容を確認してください。ROMやBIOSのファイルを送る必要はありません。

## English

See [CD_DUMP.md](CD_DUMP.md#english) for the standalone tool that captures physical CDs as CUE/BIN images.

See the [README](README.md#english) for installation and requirements. Menu and setting names below match the English UI.

### 1. Starting and opening games

Run `rxpce.exe` and select an image through `Game → Open...`. Opening another valid image stops the current session and starts the new game. Cancelling the file picker leaves the current game selected.

| Format | What to open |
| --- | --- |
| HuCard | A `.pce` file |
| Zipped HuCard | A `.zip`; only the first `.pce` found in the archive is used. There is no multi-game selection screen |
| CUE/BIN | Open the `.cue`. Its referenced BIN files must also be available at the paths specified by the CUE |
| CHD | A standalone CD CHD v5 `.chd`. Parent-dependent and hard-disk CHDs are unsupported |

CUE supports `AUDIO`, `MODE1/2352` and `MODE1/2048` tracks in `BINARY` files. WAV/MP3-based CUEs and zipped CD images are unsupported. A raw BIN alone does not describe the disc layout; open its CUE instead.

You may also pass one image path on the command line. Quote paths containing spaces.

```powershell
.\rxpce.exe "D:\Games\Example.pce"
.\rxpce.exe "D:\Games\CD\Example.cue"
.\rxpce.exe "D:\Games\CD\Example.chd"
```

### 2. CD games and System Card BIOS

1. Open `View → Settings...`.
2. Use the `...` button beside `Folders → System Card BIOS (CD only)` to select a BIOS, then confirm with `OK`.
3. Open a CUE or CHD through `Game → Open...`.
4. Wait while `Opening CD image` validates the BIOS and complete disc. You can cancel preparation.

The current emulation stops before CD validation begins. After cancellation or validation failure, reopen an image to resume playing.
5. Press Run (`Enter` by default) on the BIOS screen.

A newly created `backup.dat` may require initialization through the BIOS before in-game saves can be used. With System Card 3.0, press Select (`Tab` by default) on the BIOS screen and choose the full initialization option (「全体の初期化」). This erases all in-game saves in the shared backup RAM; do not initialize it when keeping existing data.

Supported BIOS images are the Japanese System Card 1.0, 2.0 and 2.1, and SUPER System Card 3.0. Use 3.0 for SUPER CD-ROM². The emulator checks the contents of supported 256 KiB BIOS images. A 512-byte header is accepted; a 384 KiB overdump is not. BIOS files are not included.

CUE and CHD states cannot be exchanged. If a CHD fails while the original CUE works, check the original CUE first. External conversion of a CUE containing both `PREGAP` and `INDEX 00/01` can change track positions. rxpce does not automatically repair images or convert them to CHD.

### 3. Menus and shortcuts

| Menu | Key | Action |
| --- | --- | --- |
| Game → Open... | — | Select an image and start |
| Game → Save State | F5 | Save the current state, overwriting the slot |
| Game → Load State | F6 | Restore the saved state |
| Game → Reset | F10 | Reset the running game |
| Game → Stop Emulation | Esc | Stop and return to the idle black screen |
| Game → Exit | Alt+F4 | Quit |
| Game → Replay → Start / Stop Recording | F11 | Start or finish recording a replay |
| Game → Replay → Play / Stop Replay | F12 | Start playback while stopped or stop playback |
| View → Window Scale | F8 | Cycle 1×, 2×, 3× and 4× window scale |
| View → Fullscreen | Alt+Enter | Toggle fullscreen/windowed mode |
| View → Input Profiles | Alt+1 through Alt+0 | Select input profile 1–10 while stopped |
| View → Settings... | — | Open settings |
| View → About rxpce... | — | Show version and credits |

| Additional key | Action |
| --- | --- |
| F2 | Toggle automatic frame skip |
| F3 / F4 | Decrease / increase frame skip by one, within 0–15 |
| F7 | Toggle FPS display |

F8 works before a game starts, but is disabled in fullscreen. Saving, loading, reset, stop and fullscreen require a running game. Holding a shortcut does not repeatedly execute it. Game shortcuts are suppressed in application dialogs. Alt+F10 does not reset the game.

Use `Alt+G` for Game and `Alt+V` for View, then an underlined menu letter to select an item. Windows controls when underlines are shown.

F10 resets the current game. It does not open another image or apply pending settings. Press Esc and reopen the image to apply those settings.

### 4. Default keyboard controls

| Game action | Default key | Mode |
| --- | --- | --- |
| Directions | Arrow keys | 2/6 buttons |
| Run | Enter | 2/6 buttons |
| Select | Tab | 2/6 buttons |
| I / II | Z / X | 2/6 buttons |
| I rapid / II rapid | A / S | 2 buttons |
| III | C | 6 buttons |
| IV / V / VI | A / S / D | 6 buttons |

Change these in `Input`. Six-button mode disables rapid fire; A/S become IV/V. Simultaneous up/down or left/right inputs cancel each other on that axis.

### 5. Gamepads and multiple players

Open `View → Settings... → Input`.

1. Set `Players` to 1–5, as appropriate for the game.
2. Select the `Player` to configure.
3. Choose `2 buttons` or `6 buttons`. Use six-button mode for games that support it.
4. Select `Keyboard`, `XInput 1`–`XInput 4`, or a `DirectInput: device name` as the `Input device`.
5. Focus a binding and press a key/gamepad button, or choose from the list. Button/key capture advances to the next field.
6. Confirm with `OK` and reopen the game.

The default device is `Keyboard`. Under `Advanced`, `XInput support` is enabled and `DirectInput support` is disabled by default. Enable DirectInput to select its devices. Enabling both backends may list the same physical controller twice; choose the intended device for each player.

XInput displays slot and connection status, such as `XInput 1 (Connected)`, rather than a product name. It provides four slots. DirectInput displays device names. Directions use the XInput D-pad/left stick, or DirectInput X/Y axes and hat switches.

| Game action | Default XInput button |
| --- | --- |
| I / II | A / B |
| Run / Select | Start / Back |
| I rapid / II rapid | LB / RB in two-button mode |
| III / IV / V / VI | X / Y / LB / RB in six-button mode |

`Left stick` and `Right stick` in the binding list mean stick-click buttons. LT/RT can also be assigned. If multiple players use the same default keyboard bindings, the same key controls all of them; assign separate keys or devices.

### 6. Settings and when they apply

`OK` saves settings; `Cancel` discards edits. Except for window scale and aspect ratio, settings apply at the next emulation start. Press Esc and reopen the image. F2/F3/F4/F7 changes take effect immediately during play.

#### Video

| Setting | Description |
| --- | --- |
| D3D11 device | Defaults to `Automatic (hardware / WARP fallback)`. Try `WARP (software)` when diagnosing rendering issues |
| Window scale | 1×–4×; applies on OK. In fullscreen, this is the scale retained for returning to a window |
| Aspect ratio | Square Pixels (default) / TV (4:3); applies on OK |
| Automatic frame skip (60 Hz) | Enabled by default; targets approximately 60 Hz and skips additional rendering when behind |
| Frame skip (0-15) | Frames skipped before the next rendered frame. A minimum in automatic mode, a fixed count otherwise |
| Palette file (empty = linear RGB) | External palette: 512 RGB entries, 1536 bytes. Empty, invalid-size or unreadable files use standard RGB. When the game selects monochrome output, grayscale is applied after the RGB palette |

**Disabling automatic frame skip removes the speed limit**, even with frame skip set to zero. Keep it enabled for normal play. Excessive host load can still prevent normal speed.

`Square Pixels` enlarges source pixels equally in both directions using integer scales. The window uses at least 320×240 and centers smaller images; wider modes expand it. Fullscreen uses the largest fitting integer scale.

`TV (4:3)` maps the complete visible image from the core to a 4:3 rectangle. Window sizes are 1×–4× a fixed 320×240 base, even when the game changes resolution. Fullscreen uses the largest fitting 4:3 rectangle with black borders: on a 1920×1080 display, the image is 1440×1080 with 240-pixel side borders.

Both modes use nearest-neighbor sampling. TV scaling is not restricted to source-pixel integer scales; a 512-pixel-wide image at 1× is downscaled, which can change fine lines and text. Increase the window scale if needed. There is no automatic trimming of embedded black borders, additional overscan, exact signal reconstruction or CRT filter. Loading a state does not restore display preferences.

#### Audio

| Setting | Description |
| --- | --- |
| Enable audio | Enable/disable audio output |
| WASAPI (shared) / DirectSound | Output backend; WASAPI is the default |
| Sample rate (Hz) | 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100 or 48000; default 44100 |
| Channels | Mono or Stereo; default Stereo |
| Latency (20-160 ms) | Default 40 ms. Lower values reduce delay; increase it if audio breaks up |
| DirectSound level | DirectSound only: 0 Write primary, 1 Exclusive, 2 Priority; default 2 |

Mono averages the left and right signals. Audio rate conversion adds a delay separate from the output buffer: about 0.73 ms at 44100 Hz and up to 4 ms at 8000 Hz. Check for dropouts when reducing Latency.

#### Input

You can save up to ten input profiles for different games. Press Esc to stop a running game, then select a profile through `View > Input Profiles` or with `Alt+1` through `Alt+0`. `Alt+0` selects slot 10. The active slot is checked in the menu and restored on the next launch. Selecting an unused slot for the first time copies the previous slot's input settings.

In `Settings > Input`, use `Profile` to choose a slot to edit. You can change its `Name` (up to 30 characters), player count, input devices, two- or six-button mode, and button and rapid-fire mappings. Choosing a slot here keeps the active profile selected for gameplay. Edits stay in the dialog as you move between slots. Click OK to save all edits or Cancel to discard them. A blank name displays the default, such as `Profile 1`. Saved names appear in the menu immediately, and mappings apply the next time emulation starts.

Video, audio, XInput and DirectInput support, and save locations are shared across profiles. In-game saves and save state filenames stay the same when you switch profiles. To load a state, use the same player count and two- or six-button configuration as when it was saved. F5 overwrites the same file even when the current profile uses a different controller configuration.

See section 5 for gamepad setup and default mappings.

#### Recent games

Open one of the last ten games from `Game → Recent Games`. While emulation is stopped, select an input profile and choose `Game → Assign Selected Profile to Last Game`. The next time you open that game, rxpce uses the assigned slot for that session. Choose `Game → Clear Last Game Profile` to remove the assignment. Assignments are stored by file location.

#### Folders

| Setting | Description |
| --- | --- |
| ROM folder | Initial folder for the file picker |
| State / RAM folder | State and Populous RAM folder; empty or `.` means beside the image |
| System Card BIOS (CD only) | Path to the CD BIOS |
| Allow network CD images | Off by default. Allows network image files and all CUE references on the next CD load |

Other relative folder paths use the process working directory. Use a writable absolute path for a fixed location. This setting does not relocate `backup.dat`.

CD inputs and CUE references use ordinary local drives by default. Local parent-relative/absolute paths and supported junctions/symbolic links remain usable. UNC paths, remote mapped drives and unclassified devices/reparse tags are rejected. Enable `Allow network CD images` only for trusted network media. Permission covers every CUE reference, not just the initial server. It applies to the next load and does not remount an active disc. It does not control the Windows file picker itself, BIOS/ROM/ZIP paths or output destinations.

#### Advanced

| Setting | Description |
| --- | --- |
| Base clock (Hz) | Default 7,170,000; normally leave unchanged. Range 1,000,000–85,920,000, rounded down to 1000 Hz units |
| VDC emulation | `Accurate` (default) uses progressive DMA and CPU contention waits. `Fast` prioritizes speed with bulk transfers and simplified timing. Applied on the next emulation start |
| Enforce hardware sprite limit | Enabled by default. Disable to draw all eligible sprites in the 64-entry SATB, bypassing drawing search/fetch limits. IRQs, collision detection and DMA waits stay unchanged. Applied on the next emulation start; save states are shared between both settings |
| Enable diagnostic log | Enable detailed logging; disabled by default |
| Show FPS | FPS display; disabled by default. F7 changes it during play |
| XInput support / DirectInput support | Enable input backends; only XInput is enabled by default |

Initial audio failures, changes in error code and recovery are logged even when detailed logging is disabled. Repeated WASAPI errors are summarized. Log write failures throttle retries and may drop records; the log is best effort.

VDC emulation selects processing cost and timing behavior. Fast still runs at the normal speed (approximately 60 Hz) when Automatic frame skip is enabled. If a game shows display or behavior differences, select Accurate and reopen the game.

Accurate applies palette changes within a scanline using a zero additional delay approximation. Fast uses the colors at the start of the line for the entire line. Loading a state discards the unfinished display frame; display resumes with the next complete frame. If the VDC display interval crosses a frame boundary, presentation waits until that interval is complete. The palette-timing update changes all four state format GUIDs; create new states after reopening the game. In-game saves remain usable.

Normal-speed pacing follows completed frame timestamps at a video reference of 21,477,270 Hz and 1365 clocks per line. VCE selects 262 lines (about 60.054 Hz) or 263 lines (about 59.826 Hz). Fractional time accumulates across changes in frame length. The video clock is independent of Base clock.

Base clock sets CPU-based timing relative to the video clock and also affects PSG/timer progression. CD and audio output convert CPU timestamps to real time using this frequency. Normally leave it at the default. It is not a host-PC performance tuning setting.

### 7. Save states and file locations

#### Input replays

Press F11 during HuCard or CD play to start recording, then press it again to save an `.rpl` file. If you cancel the save dialog, use `Game > Replay > Save Pending Recording...` to try again. While emulation is stopped, press F12 and select the replay and matching game. Press F12 or Esc to stop playback.

Playback requires the same ROM, or a disc image with the same format and contents and the same System Card BIOS. CUE and CHD use separate replays. Playback uses the recorded inputs and temporary copies of in-game saves. Shared replays contain the in-game save data present when recording began. If playback diverges, the checkpoint is reported in `rxpce.log`.

There is **one save-state slot per VDC mode**. F5 overwrites it; F6 restores it. Open the game and let emulation start before using these commands. After restarting the application, reopen the same game and press F6.

| File | Contents | Location/name |
| --- | --- | --- |
| rxpce.json | Settings | Beside the executable |
| backup.dat | Shared 2 KiB game-save RAM | Beside the executable |
| HuCard/ZIP/CUE state | Emulation state | `Example.pce` / `Example.zip` / `Example.cue` → `Example.st0` |
| CHD state | Emulation state | `Example.chd` → `Example.chd.st0` |
| Fast state | Speed-priority emulation state | Uses `.fst0` instead of `.st0`; CHD uses `Example.chd.fst0` |
| Populous RAM | 32 KiB cartridge RAM | `Example.pce` / `Example.zip` → `Example.ram` |
| rxpce.log / rxpce.log.bak | Diagnostic / previous log | Beside the executable |

States and `.ram` files follow `State / RAM folder`. ZIP saves use the ZIP's name, not its internal PCE name. PCE/ZIP/CUE files with the same base name and save folder share a state filename; use distinct names or folders. CUE and CHD have different state names and identities and cannot exchange states.

Game saves and save states are separate features. Game-save RAM is written to disk when stopping with Esc, switching games or exiting normally. A forced termination may lose recent changes.

**F6 also rolls shared backup RAM and Populous RAM back to the saved point.** Stopping or exiting afterward writes the rolled-back data to disk. Shared backup RAM may include other games' saves. To preserve newer saves, exit normally and back up the files first.

Accurate and Fast keep separate states and cannot load each other's states. Renaming a file does not bypass its format GUID. Changing VDC emulation during play leaves the active mode and save path unchanged until the next emulation start. In-game saves in `backup.dat` and `.ram` are shared between both modes.

CD restoration requires the same BIOS, disc and configuration. Changing player count, 2/6-button modes or Base clock can cause a state to be rejected. Do not rename a state to reuse it with another game or image format. Old xpce states and development states with different formats are not migrated.

### 8. Updating and resetting settings

**Save states from official releases 1.4.0 and earlier, or development builds with a different state format, cannot be loaded.** When updating from one of those builds, start games normally and create new states. See the [release notes](RELEASE_NOTES.md#english) for changes.

Save states support resumable block transfers and HuCard PSG phase restoration. Separate fixed format GUIDs identify HuCard and CD states. HuCard and CD format GUIDs were updated to store VDC DMA, pending CPU VRAM accesses and VCE phase. States with different GUIDs cannot be loaded. In-game saves in `backup.dat` and `.ram` remain usable.

Special rendering modes, VRAM write rules, palette timing and restored presentation changes also update the HuCard/CD format GUIDs for both Accurate and Fast. States made before this change cannot be loaded; start the game normally and create new states after updating.

Before updating, close the application and back up `rxpce.json`, `backup.dat` and each game's `.st0`/`.fst0`/`.ram`. Moving the executable changes where it looks for settings and shared backup RAM.

To reset settings only, close the app, rename `rxpce.json` as a backup and restart. New settings are saved on normal exit. Keep `backup.dat` and `.ram` files if you want to retain game saves. Old `xpce.ini`/`xpce.json` files are not imported automatically.

### 9. Troubleshooting

| Symptom | Check |
| --- | --- |
| Missing MSVCP140.dll / VCRUNTIME140.dll at startup | Install the official Visual C++ v14 x64 runtime linked in the [README](README.md#english) |
| Game runs too fast | Enable Automatic frame skip with F2 or in Video settings |
| Gamepad does not respond | Check Input device, Players, enabled backends and Connected status; confirm with OK and reopen the game |
| Six-button controls do not work | Check game support and each Player's Buttons setting |
| Settings appear unchanged | Except for scale and aspect ratio, stop with Esc and reopen. F10 Reset does not apply pending settings |
| No audio / broken audio | Check Enable audio and Windows volume/output device; increase Latency and reopen, or try the other backend |
| Cannot open a CD | Check the supported BIOS/size, referenced BIN files and CHD type; allow validation to complete |
| Only the CHD fails / audio positions are wrong | Check the original CUE, and whether conversion preserved track positions and pregaps |
| Cannot save settings or RAM | Check write permissions and free space in the application and State / RAM folders |
| State cannot be loaded | Check game/BIOS/format and the original input configuration and Base clock |
| Cannot load backup.dat | Preserve the original and check its size. Non-2-KiB files are rejected and are not overwritten during that session |

When describing a problem, include rxpce version, Windows environment, game/version, image format, reproduction steps and input/audio/video/speed settings. To collect detailed logs, enable the Advanced option, reopen the game and reproduce the issue. Logs contain file paths; review them before sharing. You do not need to send ROM or BIOS files.
