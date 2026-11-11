param([int]$X, [int]$Y, [string]$Shot = "")
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class M {
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
 public struct POINT { public int X, Y; }
}
"@
[M]::SetProcessDPIAware() | Out-Null
$scr = [System.Windows.Forms.Screen]::AllScreens | ForEach-Object { "$($_.DeviceName) $($_.Bounds)" }
"SCREENS(dpiaware): $($scr -join ' | ')"
$p = Get-Process -Id 49424
[M]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 400
if ($X -ne 0 -or $Y -ne 0) {
  [M]::SetCursorPos($X, $Y) | Out-Null; Start-Sleep -Milliseconds 200
  $pt = New-Object M+POINT; [M]::GetCursorPos([ref]$pt) | Out-Null; "CURSOR $($pt.X),$($pt.Y)"
  [M]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
  [M]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
if ($Shot) {
  Start-Sleep -Milliseconds 1500
  $b = ([System.Windows.Forms.Screen]::AllScreens | Where-Object { $_.Bounds.X -gt 0 } | Sort-Object { $_.Bounds.X } | Select-Object -First 1).Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.X, $b.Y, 0, 0, $bmp.Size)
  $bmp.Save($Shot); "SHOT $Shot $($b)"
}
