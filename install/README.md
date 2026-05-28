# install/

dipad のインストールに必要なレジストリスクリプトを格納するフォルダです。

## ファイル一覧

| ファイル | 用途 |
|---|---|
| `install.reg` | `PreferExternalManifest` を有効化 (dipad 動作に必須) |
| `uninstall.reg` | `PreferExternalManifest` を削除 |

## install.reg を実行する必要があるかどうか

`th123.exe` のように **空の埋め込みマニフェスト** を含むゲームに対しては、`install.reg` の実行が必須です。dipad の外部マニフェストが読み込まれず動作しません。

| ゲーム | install.reg 必須 |
|---|---|
| 東方非想天則 (th123.exe) | **必須** |
| その他、埋め込みマニフェストの無いゲーム | 不要 |

確認方法: `tools\check_embedded_manifest.ps1` (本リポジトリ同梱) を実行すると判別できます。

## 適用方法

エクスプローラから `install.reg` をダブルクリックし、UAC プロンプトで「はい」を押してください。
あるいは管理者 PowerShell から:

```powershell
reg import install.reg
```

## アンインストール

ゲームフォルダから `dipad.dll` / `dipad.manifest` / `<exe名>.manifest` (および `dipad.ini`) を削除した後、`uninstall.reg` を実行してください。

> **注意**: 他の互換シム (古いインストーラ用など) で `PreferExternalManifest` を使っている場合、削除すると他ツールが動かなくなる可能性があります。心当たりがある場合は値を残しておいても害はありません。
