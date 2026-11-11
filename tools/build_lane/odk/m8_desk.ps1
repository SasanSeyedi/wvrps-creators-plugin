Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$v=[System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp=New-Object System.Drawing.Bitmap $v.Width,$v.Height
$g=[System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($v.Left,$v.Top,0,0,$bmp.Size)
$bmp.Save($args[0]); "VS $($v.Left) $($v.Top) $($v.Width) $($v.Height)"
