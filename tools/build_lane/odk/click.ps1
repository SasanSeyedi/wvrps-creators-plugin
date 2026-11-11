param([int]$X, [int]$Y, [string]$Shot = "")
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class M {
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
}
"@
if ($X -ne 0 -or $Y -ne 0) {
  [M]::SetCursorPos($X, $Y) | Out-Null; Start-Sleep -Milliseconds 150
  [M]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [M]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  "CLICK $X,$Y"
}
if ($Shot) {
  Start-Sleep -Milliseconds 1200
  $bmp = New-Object System.Drawing.Bitmap 2048, 1152
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen(2048, 0, 0, 0, $bmp.Size)
  $bmp.Save($Shot); "SHOT $Shot"
}
