# tools/check_embedded_manifest.ps1
#
# 与えられた EXE に「埋め込みマニフェスト」があるかを表示します。
# 空マニフェストが入っている場合 (非想天則 等) は dipad を効かせるために
# install/install.reg の適用が必要です。
#
# 使い方:
#   .\check_embedded_manifest.ps1 'C:\Program Files (x86)\tasofro\th123\th123.exe'
#
# 「このスクリプトはデジタル署名されていないため実行できません」と出る場合:
#   Releases の ZIP から取り出した .ps1 にはインターネット由来マークが付くため、
#   既定の ExecutionPolicy では実行が止められます。以下のいずれかで回避できます。
#
#   (a) 一発実行 (ポリシーを永続的に変更しない・推奨):
#       powershell -ExecutionPolicy Bypass -File .\check_embedded_manifest.ps1 'C:\path\to\game.exe'
#
#   (b) ファイルのブロックを解除してから普通に実行:
#       Unblock-File .\check_embedded_manifest.ps1
#       .\check_embedded_manifest.ps1 'C:\path\to\game.exe'
#
#   (c) このシェルでだけポリシーを緩める (シェルを閉じれば元に戻る):
#       Set-ExecutionPolicy -Scope Process Bypass
#       .\check_embedded_manifest.ps1 'C:\path\to\game.exe'

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath
)

if (-not (Test-Path -LiteralPath $ExePath)) {
    Write-Error "Not found: $ExePath"
    return
}

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class DipadPE {
    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr LoadLibraryEx(string p, IntPtr h, uint f);
    [DllImport("kernel32")] public static extern bool FreeLibrary(IntPtr m);
    [DllImport("kernel32")] public static extern IntPtr FindResource(IntPtr m, IntPtr n, IntPtr t);
    [DllImport("kernel32")] public static extern IntPtr LoadResource(IntPtr m, IntPtr r);
    [DllImport("kernel32")] public static extern IntPtr LockResource(IntPtr r);
    [DllImport("kernel32")] public static extern uint   SizeofResource(IntPtr m, IntPtr r);
    public static string Get(string p, int id) {
        IntPtr h = LoadLibraryEx(p, IntPtr.Zero, 0x22);
        if (h == IntPtr.Zero) return "<<load failed: " + Marshal.GetLastWin32Error() + ">>";
        try {
            IntPtr r = FindResource(h, (IntPtr)id, (IntPtr)24);
            if (r == IntPtr.Zero) return null;
            IntPtr d = LoadResource(h, r);
            IntPtr q = LockResource(d);
            uint   s = SizeofResource(h, r);
            byte[] b = new byte[s];
            Marshal.Copy(q, b, 0, (int)s);
            return Encoding.UTF8.GetString(b);
        } finally { FreeLibrary(h); }
    }
}
"@ -ErrorAction SilentlyContinue | Out-Null

$found = $false
foreach ($id in 1..3) {
    $m = [DipadPE]::Get((Resolve-Path $ExePath).Path, $id)
    if ($m) {
        $found = $true
        $trimmed = $m.Trim()
        $bodyLen = ($trimmed -replace '\s', '').Length

        Write-Host ""
        Write-Host "EMBEDDED MANIFEST FOUND (resource ID=$id)" -ForegroundColor Yellow
        Write-Host "----- contents -----"
        Write-Host $m
        Write-Host "--------------------"

        if ($bodyLen -lt 200 -or $m -notmatch '<(dependency|trustInfo|file|comClass)\b') {
            Write-Host "Likely an EMPTY / placeholder manifest." -ForegroundColor Yellow
            Write-Host "This blocks external <exe>.manifest from being honored." -ForegroundColor Yellow
            Write-Host "→ install/install.reg の適用が必要です。" -ForegroundColor Cyan
        } else {
            Write-Host "Substantive embedded manifest detected." -ForegroundColor Yellow
            Write-Host "External manifests are still blocked unless install.reg is applied." -ForegroundColor Yellow
            Write-Host "→ install/install.reg の適用が必要です。" -ForegroundColor Cyan
        }
    }
}

if (-not $found) {
    Write-Host ""
    Write-Host "No embedded manifest in $ExePath." -ForegroundColor Green
    Write-Host "External manifest will be honored as-is." -ForegroundColor Green
    Write-Host "→ install/install.reg は不要です。" -ForegroundColor Cyan
}
