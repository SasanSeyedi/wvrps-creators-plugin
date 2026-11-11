param([string]$target = 'git:https://github.com')
$ErrorActionPreference = 'Stop'
$sig = @'
using System;
using System.Runtime.InteropServices;
public class CredReadHelper {
    [DllImport("advapi32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool CredRead(string target, int type, int reservedFlag, out IntPtr credentialPtr);
    [DllImport("advapi32.dll", SetLastError=true)]
    public static extern bool CredFree(IntPtr cred);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct CREDENTIAL {
        public int Flags;
        public int Type;
        [MarshalAs(UnmanagedType.LPWStr)] public string TargetName;
        [MarshalAs(UnmanagedType.LPWStr)] public string Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public int CredentialBlobSize;
        public IntPtr CredentialBlob;
        public int Persist;
        public int AttributeCount;
        public IntPtr Attributes;
        [MarshalAs(UnmanagedType.LPWStr)] public string TargetAlias;
        [MarshalAs(UnmanagedType.LPWStr)] public string UserName;
    }
    public static string[] Read(string target) {
        IntPtr p;
        if (!CredRead(target, 1, 0, out p)) return null;
        CREDENTIAL c = (CREDENTIAL)Marshal.PtrToStructure(p, typeof(CREDENTIAL));
        byte[] blob = new byte[c.CredentialBlobSize];
        Marshal.Copy(c.CredentialBlob, blob, 0, c.CredentialBlobSize);
        string pass = System.Text.Encoding.Unicode.GetString(blob);
        CredFree(p);
        return new string[] { c.UserName, pass };
    }
}
'@
Add-Type -TypeDefinition $sig
$r = [CredReadHelper]::Read($target)
if (-not $r) { Write-Output "NOT_FOUND"; exit 2 }
$u = $r[0]; $p = $r[1]
Write-Output ("USER=" + $u)
Write-Output ("TOKEN_LEN=" + $p.Length + " PREFIX=" + $p.Substring(0, [Math]::Min(6, $p.Length)))
# Do the push with the token in the one-shot URL (never printed).
$env:GIT_TERMINAL_PROMPT = '0'
Push-Location 'F:\Estate\Claude\_scratch\CREATORS_HUB_RELEASE_CANDIDATE'
$url = "https://" + $u + ":" + $p + "@github.com/SasanSeyedi/wvrps-creators-plugin.git"
$out = git -c credential.helper= push --porcelain $url main 2>&1
$code = $LASTEXITCODE
Write-Output ("PUSH_EXIT=" + $code)
$out | ForEach-Object { ($_ -replace [regex]::Escape($p), '<TOKEN>') } | Select-Object -First 20
Pop-Location
