# dipad — DirectInput 互換シム (POV / 軸 → ボタンブリッジ)

古い `DirectInput8` ゲームで、XInput 系コントローラ (Xbox / Vader 4 Pro 等) の **十字キー** や **L2/R2 トリガー** が反応しない問題を解消する Windows 互換シムです。

> ゲーム本体には **一切** 手を加えません。ゲームフォルダに `dinput8.dll` (推奨) または `dipad.dll` + マニフェストを置くだけで動作します。

## このツールが解決する問題

`東方非想天則 / 緋想天` (CoCreateInstance 型) や `心綺楼 / 憑依華` など (DirectInput8Create 型) をはじめとする、`DirectInput8` 経由でコントローラを読み込む古いゲームでは、以下のような症状が起きがちです。

- 左スティックは効くが、**十字キーが効かない** (POV ハットを X/Y 軸として読まない)
- **L2 / R2 トリガーがアナログ軸**として OS から上がってくるため、ボタンとして割り当てできない

`dipad` は **OS とゲームの間に挟まる薄い DirectInput8 互換レイヤ** として動作し、`DirectInput8` の応答をその場で書き換えます。

- **POV ハット → X / Y 軸** 変換 (十字キーで方向入力可能になる)
- **アナログ軸 → 仮想ボタン** 変換 (L2/R2 などをキーコンフィグでボタン番号として登録可能になる)

## 「ゲーム MOD」ではありません

`dipad` はゲーム本体のバイナリ・メモリ・データを一切改変しません。導入方法は 2 通りあります。

| 方式 | 概要 |
|---|---|
| **方式A: dinput8.dll プロキシ** (推奨) | ゲームフォルダに `dinput8.dll` を置くだけ。レジストリ変更・管理者権限・マニフェスト不要 |
| **方式B: マニフェスト (Reg-Free COM)** | Microsoft の **Side-by-Side (SxS) Activation Context** で COM クラスの実装先を上書き。非想天則・緋想天など一部のゲーム向け |

| 改変対象 | dipad による改変 |
|---|---|
| ゲームバイナリ | **なし** |
| ゲームデータ・セーブデータ | **なし** |
| システム DLL | **なし** (本物は `%SystemRoot%\System32\dinput8.dll` をフルパスで読み込み) |
| 同フォルダの他 MOD (`d3d9.dll` 系ローダ等) | 共存可能 |
| HKLM レジストリ | **方式B のみ** (`PreferExternalManifest` 1 個。詳細は後述) |

## インストール

### どちらの方式を選ぶか

迷ったら **まず方式A** を試してください。

1. 方式A の手順で `dinput8.dll` を配置してゲームを起動
2. `%LOCALAPPDATA%\dipad\dipad_load.log` にエントリが出るか確認
   - **出る** → 方式A で動作しています
   - **出ない** → 方式B を試す (または配置ミスを確認)

### 方式A: dinput8.dll プロキシ方式 (推奨・ほとんどのゲーム向け)

`DirectInput8Create()` を直接呼ぶゲーム (心綺楼 / 憑依華など大半の DirectInput8 ゲーム) 向けです。**レジストリ変更・管理者権限・マニフェストは一切不要**です。

#### 手順

1. [Releases](https://github.com/stanak/dipad/releases) から最新の ZIP をダウンロードして展開
2. ゲームのビット数を確認 (タスクマネージャ → 詳細 → プロセス名に `*32` が付いていれば 32-bit)
3. ZIP 内の **`x86\dinput8.dll`** または **`x64\dinput8.dll`** を、ゲームの実行ファイル (`*.exe`) と **同じフォルダ** にコピー
4. (L2/R2 を使う場合) `dipad.ini.sample` を `dipad.ini` にリネームして同じフォルダにコピー
5. ゲームを起動

#### 注意点

- ゲームフォルダに **既に `dinput8.dll` がある** (他の MOD が使用中) 場合、この方式は使えません → **方式B** を使ってください
- `d3d9.dll` 系 MOD (ReShade, dgVoodoo 等) とは **共存可能** です
- 設定ファイルは引き続き **`dipad.ini`** という名前で、シム DLL (`dinput8.dll` または `dipad.dll`) と同じフォルダ (ゲームフォルダ) に置きます

#### ゲームフォルダ構成例 (方式A)

```
[ゲームフォルダ]
├── game.exe                 ← 既存
├── dinput8.dll              ← 追加 (dipad 同梱)
├── dipad.ini                ← 追加 (任意)
└── ...                      ← 既存の MOD (d3d9.dll 等) はそのまま
```

### 方式B: マニフェスト方式 (非想天則・緋想天など)

`CoCreateInstance(CLSID_DirectInput8, ...)` で DirectInput を生成するゲーム (東方非想天則 / 緋想天など) 向けです。方式A ではこれらのゲームには効きません (import テーブルに `DirectInput8Create` が無く、プロキシ DLL がロードされないため)。

ゲームフォルダに **既に `dinput8.dll` が他 MOD に使われている** 場合も、こちらを使います。

#### 必要なもの (非想天則の例)

- 非想天則 (`th123.exe`)
- 管理者権限 (1 回のレジストリ設定でのみ必要)

#### 手順 (非想天則)

1. [Releases](https://github.com/stanak/dipad/releases) から最新の ZIP をダウンロードして展開
2. ZIP 内の **`install\install.reg`** をダブルクリック → UAC で「はい」
   - これで `PreferExternalManifest` レジストリ値が有効になります。理由は後述
3. ZIP 内から以下のファイルを `th123.exe` と同じフォルダにコピー
   - `x86\dipad.dll`
   - `x86\dipad.manifest`
   - `manifests\th123.exe.manifest`
   - (L2/R2 を使う場合) `dipad.ini.sample` を `dipad.ini` にリネームしてコピー
4. 非想天則を起動

#### ゲームフォルダ構成例 (方式B / 非想天則)

```
[ゲームフォルダ]
├── th123.exe                ← 既存
├── th123.exe.manifest       ← 追加 (dipad 同梱)
├── dipad.dll                ← 追加 (dipad 同梱)
├── dipad.manifest           ← 追加 (dipad 同梱)
├── dipad.ini                ← 追加 (任意)
└── ...                      ← 既存の MOD はそのまま
```

#### なぜ `PreferExternalManifest` を有効にする必要があるのか

非想天則の `th123.exe` には **空の埋め込みマニフェスト** が含まれています。Windows は EXE に埋め込みマニフェストがあると、外部の `th123.exe.manifest` を完全に無視するというデフォルト挙動を取ります。

`PreferExternalManifest` は Microsoft が公式に文書化しているスイッチで、これを `1` にすると Windows は外部マニフェストを優先するようになります。

- 出典: [Application Manifests — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests)
- 影響範囲: システム全体 (HKLM)
- 副作用: 他に `<exe名>.manifest` を提供しているツール (古いインストーラ用の互換シム等) も適用されるようになります。問題が出るケースはまずありませんが、心配なら `install/uninstall.reg` で元に戻せます

#### 非想天則以外のゲームに使う場合 (方式B)

1. ゲームのビット数を確認 (タスクマネージャ → 詳細 → プロセス名に `*32` が付いていれば 32-bit)
2. `tools\check_embedded_manifest.ps1 <ゲームのexeパス>` で埋め込みマニフェストの有無を確認
   - 埋め込みあり → `install\install.reg` を実行
   - 埋め込みなし → `install.reg` 不要
3. `dipad.dll` と `dipad.manifest` をゲームフォルダにコピー (ビット数に対応する方を使用)
4. `manifests\sample-x86.exe.manifest` または `manifests\sample-x64.exe.manifest` を **ゲームの実行ファイル名+`.manifest`** にリネームしてコピー
   - 例: `game.exe` (32-bit) → `game.exe.manifest`

### アンインストール

**方式A:**

1. ゲームフォルダから `dinput8.dll` (および `dipad.ini`) を削除

**方式B:**

1. ゲームフォルダから `dipad.dll` `dipad.manifest` `<exe名>.manifest` (および `dipad.ini`) を削除
2. もう他に dipad を使うゲームが無い場合は `install\uninstall.reg` を実行してレジストリ値も削除

## 設定 `dipad.ini`

`dipad.dll` (または `dinput8.dll`) と同じフォルダ (= ゲームフォルダ) に置きます。すべて省略可能で、書かなければデフォルト値が使われます。

非想天則 + L2/R2 を使う場合の推奨設定:

```ini
[General]
PovMode = pov_priority

[AxisToButton]
; Vader 4 Pro / Xbox 系の Z 軸共有トリガーを想定:
;   R2 (Z 正方向) → ボタン 10
;   L2 (Z 負方向) → ボタン 11
AxisButton1 = z:+:500:10
AxisButton2 = z:-:500:11
```

天則を起動して **CONFIG → KEY CONFIG → JOYPAD** で R2/L2 を押すと `BUTTON 10`/`BUTTON 11` として登録できます。

詳細なオプション一覧と書式は同梱の [`dipad.ini.sample`](dipad.ini.sample) を参照してください。

### L2/R2 が反応しない / 別の軸に割り当てたい

コントローラによっては L2/R2 が `Z` 以外の軸 (`Rx`, `Ry`, `Rz` など) で上がってくる場合があります。
診断モードを使って、実際にどの軸が動くか調べられます:

```ini
[General]
EnableLog = 1
DebugAxisDump = 1
```

この状態でゲームを起動して L2/R2 を押すと、`%LOCALAPPDATA%\dipad\dipad.log` に各軸の値の遷移が記録されます。L2/R2 を押した瞬間に変化している軸を見つけて、`[AxisToButton]` 側を書き換えてください。

書式詳細は [`dipad.ini.sample`](dipad.ini.sample) を参照。

## ログの場所

`EnableLog = 1` で出力されるログは:

```
%LOCALAPPDATA%\dipad\dipad.log         (動作ログ)
%LOCALAPPDATA%\dipad\dipad_load.log    (DLL ロード履歴 / EnableLog 不要)
```

ゲームフォルダ (Program Files) には書き込み権限が無い場合があるため、ログは常に `%LOCALAPPDATA%` 配下に作成されます。

## 仕組み

### 方式A: dinput8.dll プロキシ

```
[Game.exe] ──DirectInput8Create()──> 同フォルダの dinput8.dll (dipad プロキシ)
                                              │
                                              ├── LoadLibrary("%SystemRoot%\System32\dinput8.dll")
                                              │         │
                                              │         v
                                              │   本物の DirectInput8Create → 本物 IDirectInput8
                                              │
                                              v
                                    本物を内包した wrapper を Game に返す
                                              │
                              GetDeviceState 呼び出し時に POV → X/Y 変換
                              および 軸 → 仮想ボタン変換を適用
                                              │
                                              v
                                    [Game] が方向入力 / 仮想ボタンを認識
```

Windows の DLL 検索順では **アプリケーションのフォルダが System32 より優先** されるため、ゲームは同フォルダの `dinput8.dll` を読み込みます。プロキシは本物をフルパスで読み込むため、再帰的に自分自身をロードすることはありません。

### 方式B: マニフェスト (Reg-Free COM)

```
[Game.exe] ──CoCreateInstance(CLSID_DirectInput8, IID_IDirectInput8A)──> COM
                                                                          │
                          [th123.exe.manifest] = 「dipad アセンブリに依存」 │
                          [dipad.manifest]       = 「dipad は dipad.dll、       │
                                                  CLSID_DirectInput8 を提供」
                                                                          │
                                            この 2 つで activation context が決まり、
                                            CoCreateInstance はレジストリではなく
                                            こちらを優先する
                                                                          v
                                                    [dipad.dll!DllGetClassObject]
                                                                          │
                                                              ┌──────────┴──────────┐
                                                              v                     v
                                          IClassFactory                 LoadLibrary("System32\dinput8.dll")
                                                              │                     │
                                                              │              real DllGetClassObject 経由で
                                                              │              本物の IDirectInput8 を取得
                                                              │                     │
                                                              v                     v
                                                  本物を内包した wrapper を Game に返す
                                                                          │
                                              GetDeviceState 呼び出し時に POV → X/Y 変換
                                              および 軸 → 仮想ボタン変換を適用
                                                                          │
                                                                          v
                                                              [Game] が方向入力 / 仮想ボタンを認識
```

詳細は `src/` 配下のソースを参照してください。

## マニフェスト方式 (方式B) の利点

- **ゲームバイナリ無改変** — チェックサム検査をパスする
- **`dinput8.dll` ファイル名を奪わない** — 他 MOD が `dinput8.dll` を使っている場合でも共存可能
- **他ツールと完全共存** — Giuroll, DPadFix, ReShade, dgVoodoo 等と同居可能
- **プロセス局所** — システム DirectInput には一切干渉しない
- **公式仕様** — Microsoft の SxS Activation Context (Reg-Free COM)

## ビルド

### 必要なもの

- Windows 10 / 11
- Visual Studio 2022 (C++ workload)
- CMake 3.21+

### 手順

```powershell
git clone https://github.com/stanak/dipad.git
cd dipad

# 32-bit (非想天則など大半の旧作)
cmake -S . -B build/x86 -A Win32
cmake --build build/x86 --config Release

# 64-bit
cmake -S . -B build/x64 -A x64
cmake --build build/x64 --config Release
```

> PowerShell 5 (Windows 標準) では `&&` が使えません。1 行ずつ実行するか、PowerShell 7 (`pwsh`) を使ってください。

成果物:

- `build/x86/Release/dipad.dll` (+ `dinput8.dll` — 同一バイナリのプロキシ名コピー + `dipad.manifest`)
- `build/x64/Release/dipad.dll` (+ `dinput8.dll` — 同一バイナリのプロキシ名コピー + `dipad.manifest`)

### 診断用ヘルパー

`build/x86/Release/test_actctx.exe` は activation context が正しく成立するかを検証するためのコンソールアプリです。任意のフォルダに `dipad.dll`, `dipad.manifest`, `test_actctx.exe.manifest` と一緒に置いて実行すると、`CoCreateInstance(CLSID_DirectInput8)` の結果と、解決された COM プロバイダのパスが表示されます。

## 制限事項

> v0.2.0 から、ゲームが `SetDataFormat` に渡したデータフォーマットを解析してリマップするため、標準の DIJOYSTATE / DIJOYSTATE2 だけでなく **カスタムデータフォーマットのゲームにも対応** しています。また、リマップ対象はゲームコントローラ (GetCapabilities で判定) のみで、マウス / キーボードには干渉しません。

- **バッファモード入力 (`GetDeviceData`)** は素通しです。一部の古いゲームで効果が薄い可能性があります
- **DirectInput 7 (`CLSID_DirectInput`)** には未対応。dipad は DirectInput **8** のみ対象
- 32-bit ゲームには 32-bit DLL、64-bit ゲームには 64-bit DLL を使用してください
- **`LoadLibrary` に System32 直指定のフルパスを渡す** 稀なゲームでは、方式A (プロキシ) が効きません → 方式B を試してください
- `Steam Input` を有効にしているゲームでは Steam がコントローラを先に拾うため本ツールが効かない場合があります。Steam の「コントローラ設定」で当該デバイスのサポートを切ってください
- 一部のオンライン対戦ゲームで、アンチチートが DLL のロードを警戒することがあります。dipad はゲームバイナリには触れませんが、判定は各ゲームの仕様次第です

## トラブルシュート

### `dipad.log` が生成されない / `dipad_load.log` にエントリが出ない

`%LOCALAPPDATA%\dipad\dipad_load.log` を見てください。**このファイルにエントリが出ていれば、dipad は確実にゲームプロセスにロードされています**。

```powershell
Get-Content "$env:LOCALAPPDATA\dipad\dipad_load.log" -Encoding Unicode
```

#### エントリが出ている場合

- `dipad.ini` に `EnableLog = 1` を書き忘れている可能性 → 設定し直してリトライ

#### エントリが出ない場合 — 方式A (dinput8.dll プロキシ)

- `dinput8.dll` が **ゲームの exe と同じフォルダ** にあるか確認
- 32-bit ゲームに `x64\dinput8.dll` (またはその逆) を置いていないか確認
- ゲームが `LoadLibrary` で System32 直指定のフルパスを使っていないか確認 → 該当する場合は **方式B** を試す

#### エントリが出ない場合 — 方式B (マニフェスト)

- ファイル配置を確認 (`dipad.dll`, `dipad.manifest`, `<exe>.manifest` が揃っているか)
- 埋め込みマニフェスト対策が必要か確認:
  ```powershell
  .\tools\check_embedded_manifest.ps1 'C:\path\to\game.exe'
  ```
  > Releases の ZIP から取り出した `.ps1` が ExecutionPolicy で弾かれた場合は、ポリシーを変更せずに
  > 一発実行できる以下を使ってください:
  > ```powershell
  > powershell -ExecutionPolicy Bypass -File .\tools\check_embedded_manifest.ps1 'C:\path\to\game.exe'
  > ```
- `install\install.reg` を実行したか確認

### 十字キーは効くが左スティックが効かない

`dipad.ini` の `PovMode = pov_only` になっていませんか？ デフォルトの `pov_priority` に変えると両立します。

### Profile / Replay の保存先が変わってしまった

`<exe名>.manifest` に `<trustInfo>` を追加しないでください。`<trustInfo>` を入れると Windows がそのアプリを「UAC-aware」と判定し、ファイル仮想化 (`%LOCALAPPDATA%\VirtualStore`) が無効化されます。dipad 同梱のマニフェストは意図的に `<trustInfo>` を含んでいません。

## ライセンス

[MIT License](LICENSE)

## 開発・コントリビュート

- バグ報告 / 動作確認 / 機能要望は GitHub Issues へ
- 新しいゲーム向けマニフェストの追加は `manifests/` に PR でお願いします

## 名前について

`dipad` = **D**irect**I**nput Game**Pad**。短く打てて覚えやすい名前として採用しています。

## 関連プロジェクト / 参考

- [Application Manifests — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests) — `PreferExternalManifest` の出典
- [Side-by-Side Activation Contexts](https://learn.microsoft.com/en-us/windows/win32/sbscs/activation-contexts) — dipad が使う公式 API
- [vJoy](https://github.com/njz3/vJoy) — 仮想ジョイスティックドライバ
- [XOutput](https://github.com/csutorasa/XOutput) — DirectInput → XInput 変換
- [JoyToKey](https://joytokey.net/) — ジョイスティック → キーボード変換
- [reWASD](https://www.rewasd.com/) — 商用の高機能マッピングツール
