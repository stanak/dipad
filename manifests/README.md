# manifests/

dipad は **Reg-Free COM (Side-by-Side Activation Context)** の標準パターンに従って **2 つのマニフェスト** を組み合わせて動作します:

1. **アプリケーションマニフェスト** (`<exe名>.manifest`)
   - ゲーム実行ファイルの隣に置く
   - 「このプロセスは `dipad` アセンブリに依存する」と宣言
2. **アセンブリマニフェスト** (`dipad.manifest`)
   - `dipad.dll` の隣に置く
   - 「`dipad` アセンブリは `dipad.dll` を含み、`CLSID_DirectInput8` を提供する」と宣言
   - **ビルド時に CMake が自動生成** (`build/Release/dipad.manifest`)

## 配置例 (非想天則)

```
[ゲームフォルダ]
├── th123.exe                ← 既存
├── th123.exe.manifest       ← 追加 (このフォルダの th123.exe.manifest)
├── dipad.dll                  ← 追加 (ビルド成果物)
├── dipad.manifest             ← 追加 (ビルド成果物、dipad.dll と同じフォルダ)
└── ...
```

## このフォルダのファイル

| ファイル | 用途 |
|---|---|
| `dipad.manifest.in` | アセンブリマニフェストのテンプレート (CMake 入力) |
| `th123.exe.manifest` | 東方非想天則 (`th123.exe`, 32-bit) 用アプリマニフェスト |
| `sample-x86.exe.manifest` | 32-bit ゲーム用テンプレート |
| `sample-x64.exe.manifest` | 64-bit ゲーム用テンプレート |

## 新しいゲームに対応させる

1. ゲームのビット数を確認 (タスクマネージャ→詳細→「\*32」が付いていれば 32-bit)
2. 対応するテンプレートをコピー
   - 32-bit ゲーム → `sample-x86.exe.manifest`
   - 64-bit ゲーム → `sample-x64.exe.manifest`
3. `<ゲームの実行ファイル名>.manifest` にリネーム
4. テンプレート内の `<assemblyIdentity name="application">` を `name="<実行ファイル名 (拡張子無し)>"` に差し替え (推奨だが必須ではない)
5. ゲームフォルダに置く

## 注意

- ファイル名は **正確に** 一致させてください (`th123.exe` → `th123.exe.manifest`)
- 文字コードは **UTF-8 (BOM なし)** に揃えてください
- `processorArchitecture` を間違えると **何のエラーも出ず、ただ動かない** ので注意 (32-bit ゲームに `amd64` を指定するなど)
- ゲーム本体に **埋め込みマニフェスト** がある場合は外部マニフェストが無視されます。本体側の README のトラブルシュート節を参照
