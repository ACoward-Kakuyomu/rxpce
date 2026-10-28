# ビルド手順 / Building

[English translation](#english) follows the Japanese text.

## 日本語

### 必要な環境

* Windows 11 x64
* Visual Studio 2022 以降の MSVC、Windows SDK、「C++ によるデスクトップ開発」、x64 用 ATL
* CMake 3.25 以上で、使用する Visual Studio のバージョンに対応するもの

WTL、MCS6502、picojson、libchdr とその依存ソースは同梱しています。ビルド時の追加ダウンロードや Python は不要です。

### Release ビルド

ソースのルートで PowerShell またはコマンドプロンプトから実行します。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target rxpce cue_materialize rxpce_cd_dump --parallel
```

実行ファイルは `build/Release/rxpce.exe`、`build/Release/cue_materialize.exe`、`build/Release/rxpce_cd_dump.exe` です。いずれか1つだけ必要なら `--target rxpce`、`--target cue_materialize` または `--target rxpce_cd_dump` とします。Debug は `--config Debug` でビルドできます。ARM64、Win32、MinGW は今回のビルド対象外です。

起動に必要な Visual C++ ランタイムは [README](README.md) を参照してください。ROM、BIOS、CD イメージは含まれません。

### 配布時の資料

実行ファイルと一緒に、出力先にコピーされた README.md、MANUAL.md、BUILDING.md、CUE_MATERIALIZE.md、CD_DUMP.md、LICENSE.md、LICENSES/、THIRD_PARTY_NOTICES.md、および third-party/ 内のライセンス・導入元資料を保持してください。ソース配布では各依存先の権利表示も維持します。CD取り込みは [CD_DUMP.md](CD_DUMP.md)、変換補助ツールの使い方は [CUE_MATERIALIZE.md](CUE_MATERIALIZE.md) を参照してください。

### GitHub Release の自動作成

公開リポジトリの `.github/workflows/release.yml` は、`v1.0.0` のような **vX.Y.Z 形式の正式版タグの push** で動作します。`v` から始まらないタグは対象外、プレリリース名を含むタグは検証で拒否します。タグの版は CMakeLists.txt の `project VERSION` と rxpce.exe のバージョンに一致する必要があります。版を上げる際は `src/rxpce.rc` のバージョン情報も更新してください。

Windows 2022 のランナーで x64 Release の3つの実行ファイルをビルドし、必要な文書とライセンスを含む `rxpce-v1.0.0-windows-x64.zip` と `.zip.sha256` を Release に添付します。ZIP の中には同名のルートフォルダーがあります。PDB、静的ライブラリ、ROM、BIOS、設定・セーブは含めません。Visual C++ 再頒布可能パッケージは同梱しません。

ワークフローを含むコミットを公開リポジトリへ push した後、リリースするコミットにタグを付け、明示的にそのタグを push します。以下は公開リポジトリ内で利用者が実行する例です。

```powershell
git tag -a v1.0.0 -m "rxpce 1.0.0"
git push origin v1.0.0
```

GitHub Actions が有効で、リポジトリのポリシーがワークフローの `contents: write` を許可する必要があります。認証は標準の `GITHUB_TOKEN` を使うため、個人用トークンの追加登録は不要です。ビルド成功後に Release を自動公開し、説明は GitHub の自動生成ノートを使います。既存の同名 Release は上書きしません。既存 Release がある状態での再実行は失敗するため、その場合は状態を確認して手動で対応してください。

ローカルで同じ ZIP を作る場合は Release ビルド後に次を実行します（PowerShell 7）。

```powershell
pwsh -NoProfile -File tools/package_release.ps1 -Tag v1.0.0
```

出力先は `build/packages` です。`-OutputDirectory` で別の出力先を指定できます。既存の ZIP／チェックサムは上書きしません。作業用 `.stage-*` フォルダーは出力先に残りますが、Release には添付しません。パッケージ生成だけでは GitHub への送信は行いません。

## English

### Requirements

* Windows 11 x64
* Visual Studio 2022 or later with MSVC, Windows SDK, Desktop development with C++, and ATL for x64
* CMake 3.25 or later, with support for your Visual Studio version

WTL, MCS6502, picojson, libchdr and its dependencies are vendored. No additional downloads or Python are required for building.

### Release build

Run from the source root in PowerShell or a command prompt:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target rxpce cue_materialize rxpce_cd_dump --parallel
```

The executables are `build/Release/rxpce.exe`, `build/Release/cue_materialize.exe` and `build/Release/rxpce_cd_dump.exe`. To build only one, use `--target rxpce`, `--target cue_materialize`, or `--target rxpce_cd_dump`. Use `--config Debug` for a Debug build. ARM64, Win32 and MinGW are outside this build's scope.

See [README](README.md#english) for runtime requirements. ROMs, BIOS files and CD images are not included.

### Distribution documents

Keep the copied README.md, MANUAL.md, BUILDING.md, CUE_MATERIALIZE.md, CD_DUMP.md, LICENSE.md, LICENSES/, THIRD_PARTY_NOTICES.md and the license/provenance documents under third-party/ with the executables. Source distributions must also retain dependency notices. See [CD_DUMP.md](CD_DUMP.md#english) for CD capture and [CUE_MATERIALIZE.md](CUE_MATERIALIZE.md#english) for the conversion helper.

### Automated GitHub Releases

In the public repository, `.github/workflows/release.yml` runs on pushes of **stable vX.Y.Z tags**, for example `v1.0.0`. Tags without the `v` prefix do not trigger it; prerelease tag names fail validation. The version must match both the CMake `project VERSION` and rxpce.exe. Update `src/rxpce.rc` version metadata when changing versions.

A Windows 2022 runner builds all three x64 Release executables. The workflow attaches `rxpce-v1.0.0-windows-x64.zip` and `.zip.sha256` to the Release. The ZIP contains a folder of the same base name, the executables and required documentation/licenses. PDBs, static libraries, game/BIOS images, settings, saves and the Visual C++ Redistributable are not bundled.

First push the commit containing the workflow to the public repository. Then tag the intended release commit and explicitly push that tag, for example:

```powershell
git tag -a v1.0.0 -m "rxpce 1.0.0"
git push origin v1.0.0
```

Actions must be enabled and repository policy must allow the workflow's `contents: write` permission. The standard `GITHUB_TOKEN` is used; no personal access token is required. After a successful build, the Release is published automatically with GitHub-generated notes. Existing Releases are not overwritten; rerunning when a Release already exists fails and requires manual review.

To build the same ZIP locally after a Release build, use PowerShell 7:

```powershell
pwsh -NoProfile -File tools/package_release.ps1 -Tag v1.0.0
```

The default output is `build/packages`; use `-OutputDirectory` to select another location. Existing archives/checksums are not overwritten. Temporary `.stage-*` folders remain under the output directory but are not attached to the Release. Packaging locally does not send anything to GitHub.
