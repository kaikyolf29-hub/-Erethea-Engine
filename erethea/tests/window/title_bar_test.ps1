# Erethea: checks the integrated title bar of the editor on a real Windows desktop.
#
# Runs the project manager and the editor, and checks that:
# - the system title bar is gone, but the window keeps its resize borders and
#   the styles that Aero Snap needs;
# - the top edge resizes the window and the rest of the title bar is client area;
# - dragging the title bar to the left edge of the screen snaps the window to
#   the left half (compared with a native window, which shows if the desktop
#   snaps at all);
# - a maximized window covers the work area exactly;
# - the editor's minimize, maximize and close buttons work, and Windows knows
#   where the maximize button is, to show the snap layouts.
#
# Screenshots of every step are saved in -OutDir.

param(
	[Parameter(Mandatory = $true)][string]$Editor,
	[Parameter(Mandatory = $true)][string]$Project,
	[Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class Win {
	[StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
	[StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
	[StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor; public RECT rcWork; public uint dwFlags; }

	[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
	[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
	[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
	[DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
	[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
	[DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr hWnd);
	[DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
	[DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
	[DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hWnd, int index);
	[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
	[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int w, int h, uint flags);
	[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
	[DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
	[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
	[DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr hWnd, uint flags);
	[DllImport("user32.dll")] public static extern bool GetMonitorInfo(IntPtr monitor, ref MONITORINFO info);
	[DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint action, uint param, ref int value, uint winIni);
	[DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint action, uint param, IntPtr value, uint winIni);
	[DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hWnd, int attribute, out RECT rect, int size);

	public static IntPtr MakePoint(int x, int y) {
		return (IntPtr)((y << 16) | (x & 0xFFFF));
	}
}
"@

[void][Win]::SetProcessDPIAware()
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$WS_CAPTION = 0x00C00000
$WS_THICKFRAME = 0x00040000
$WM_NCHITTEST = 0x0084
$HTCLIENT = 1
$HTMAXBUTTON = 9
$HTTOP = 12
$HTTOPLEFT = 13
$HTTOPRIGHT = 14
$SW_MAXIMIZE = 3
$SW_RESTORE = 9
$DWMWA_EXTENDED_FRAME_BOUNDS = 9
$MOUSEEVENTF_LEFTDOWN = 0x0002
$MOUSEEVENTF_LEFTUP = 0x0004

$script:failures = 0
$script:step = 0

function Check([string]$name, [bool]$ok, [string]$details = "") {
	if ($ok) {
		Write-Output "PASS  $name $details"
	} else {
		Write-Output "FAIL  $name $details"
		$script:failures++
	}
}

function Save-Screenshot([string]$name) {
	$script:step++
	$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
	$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
	$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
	$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
	$file = Join-Path $OutDir ("{0:D2}-{1}.png" -f $script:step, $name)
	$bitmap.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
	$graphics.Dispose()
	$bitmap.Dispose()
}

function Get-Rect([IntPtr]$hwnd) {
	$r = New-Object Win+RECT
	[void][Win]::GetWindowRect($hwnd, [ref]$r)
	return $r
}

function Get-FrameRect([IntPtr]$hwnd) {
	$r = New-Object Win+RECT
	[void][Win]::DwmGetWindowAttribute($hwnd, $DWMWA_EXTENDED_FRAME_BOUNDS, [ref]$r, 16)
	return $r
}

function Get-ClientScreenRect([IntPtr]$hwnd) {
	$c = New-Object Win+RECT
	[void][Win]::GetClientRect($hwnd, [ref]$c)
	$p = New-Object Win+POINT
	[void][Win]::ClientToScreen($hwnd, [ref]$p)
	$r = New-Object Win+RECT
	$r.Left = $p.X; $r.Top = $p.Y; $r.Right = $p.X + $c.Right; $r.Bottom = $p.Y + $c.Bottom
	return $r
}

function Get-WorkArea([IntPtr]$hwnd) {
	$info = New-Object Win+MONITORINFO
	$info.cbSize = [System.Runtime.InteropServices.Marshal]::SizeOf($info)
	$monitor = [Win]::MonitorFromWindow($hwnd, 2)
	[void][Win]::GetMonitorInfo($monitor, [ref]$info)
	return $info.rcWork
}

function Format-Rect($r) {
	return "($($r.Left), $($r.Top), $($r.Right), $($r.Bottom))"
}

function Hit-Test([IntPtr]$hwnd, [int]$x, [int]$y) {
	return [int]([Win]::SendMessage($hwnd, $WM_NCHITTEST, [IntPtr]::Zero, [Win]::MakePoint($x, $y)).ToInt64())
}

function Press-Escape {
	# Dismisses Snap Assist, if Windows shows it after a snap.
	[Win]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)
	[Win]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 500
}

function Move-Mouse([int]$x, [int]$y) {
	# Absolute move through the input queue, like a real mouse.
	$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
	$ax = [int]($x * 65535 / ($bounds.Width - 1))
	$ay = [int]($y * 65535 / ($bounds.Height - 1))
	[Win]::mouse_event(0x8001, $ax, $ay, 0, [UIntPtr]::Zero) # MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
}

function Click([int]$x, [int]$y) {
	Move-Mouse $x $y
	Start-Sleep -Milliseconds 300
	[Win]::mouse_event($MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 100
	[Win]::mouse_event($MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 300
}

function Drag([int]$x1, [int]$y1, [int]$x2, [int]$y2) {
	Move-Mouse $x1 $y1
	Start-Sleep -Milliseconds 300
	[Win]::mouse_event($MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
	# Give the engine time to receive the click and start the move loop.
	Start-Sleep -Milliseconds 800
	$steps = 30
	for ($i = 1; $i -le $steps; $i++) {
		$x = [int]($x1 + ($x2 - $x1) * $i / $steps)
		$y = [int]($y1 + ($y2 - $y1) * $i / $steps)
		Move-Mouse $x $y
		Start-Sleep -Milliseconds 30
	}
	Start-Sleep -Milliseconds 800
	[Win]::mouse_event($MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 1500
}

function Wait-MainWindow($process, [int]$timeoutSeconds) {
	$deadline = (Get-Date).AddSeconds($timeoutSeconds)
	while ((Get-Date) -lt $deadline) {
		if ($process.HasExited) {
			throw "The process exited with code $($process.ExitCode) before showing a window."
		}
		$process.Refresh()
		if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
			return $process.MainWindowHandle
		}
		Start-Sleep -Milliseconds 500
	}
	throw "No window after $timeoutSeconds seconds."
}

# Waits until the window has no caption (the editor sets the flag after loading).
function Wait-Extended([IntPtr]$hwnd, [int]$timeoutSeconds) {
	$deadline = (Get-Date).AddSeconds($timeoutSeconds)
	while ((Get-Date) -lt $deadline) {
		$w = Get-Rect $hwnd
		$c = Get-ClientScreenRect $hwnd
		if ($c.Top -eq $w.Top) {
			return $true
		}
		Start-Sleep -Milliseconds 500
	}
	return $false
}

function Near([int]$a, [int]$b, [int]$tolerance = 2) {
	return [Math]::Abs($a - $b) -le $tolerance
}

function Test-SnapLeft([IntPtr]$hwnd, [int]$grabX, [int]$grabY, [string]$name) {
	$work = Get-WorkArea $hwnd
	Drag $grabX $grabY ($work.Left + 1) $grabY
	Save-Screenshot "$name-snap-left"
	$f = Get-FrameRect $hwnd
	$halfRight = $work.Left + [int](($work.Right - $work.Left) / 2)
	$snapped = (Near $f.Left $work.Left) -and (Near $f.Top $work.Top) -and (Near $f.Bottom $work.Bottom) -and (Near $f.Right $halfRight 16)
	return @{ Snapped = $snapped; Details = "frame $(Format-Rect $f), work area $(Format-Rect $work)" }
}

function Start-Engine([string[]]$arguments, [string]$name) {
	$log = Join-Path $OutDir "$name.log"
	$process = Start-Process -FilePath $Editor -ArgumentList $arguments -PassThru `
		-RedirectStandardOutput $log -RedirectStandardError (Join-Path $OutDir "$name.err.log")
	return $process
}

# Restores the window and gives it a known size, with the client area starting
# at the given offset from the top left corner of the work area. The editor has
# a minimum size, so it may stay larger than asked.
function Place-Window([IntPtr]$hwnd, [int]$offset = 60) {
	[void][Win]::ShowWindow($hwnd, $SW_RESTORE)
	Start-Sleep -Milliseconds 500
	$work = Get-WorkArea $hwnd
	$w = Get-Rect $hwnd
	$c = Get-ClientScreenRect $hwnd
	$borderLeft = $c.Left - $w.Left
	$borderTop = $c.Top - $w.Top
	$width = [int](($work.Right - $work.Left) * 0.7)
	$height = [int](($work.Bottom - $work.Top) * 0.7)
	[void][Win]::SetWindowPos($hwnd, [IntPtr]::Zero, $work.Left + $offset - $borderLeft, $work.Top + $offset - $borderTop, $width, $height, 0x0004)
	[void][Win]::SetForegroundWindow($hwnd)
	Start-Sleep -Milliseconds 1500
	Write-Output "      $(Format-Rect (Get-ClientScreenRect $hwnd)) client area after placing the window"
}

function Test-TitleBar([IntPtr]$hwnd, [string]$name) {
	$style = [Win]::GetWindowLong($hwnd, -16)
	Check "$name keeps WS_CAPTION and WS_THICKFRAME (needed by Aero Snap)" ((($style -band $WS_CAPTION) -eq $WS_CAPTION) -and (($style -band $WS_THICKFRAME) -eq $WS_THICKFRAME)) ("style=0x{0:X8}" -f $style)

	$w = Get-Rect $hwnd
	$c = Get-ClientScreenRect $hwnd
	$f = Get-FrameRect $hwnd
	Check "$name has no system title bar" ($c.Top -eq $w.Top) "window $(Format-Rect $w), client $(Format-Rect $c)"
	Check "$name keeps the side resize borders" (($c.Left - $w.Left) -gt 0) "left border $($c.Left - $w.Left) px"
	Check "$name visible frame starts at the client area" (Near $f.Top $c.Top 1) "frame $(Format-Rect $f)"

	$midX = [int](($c.Left + $c.Right) / 2)
	$hitTop = Hit-Test $hwnd $midX ($c.Top + 2)
	$hitTopLeft = Hit-Test $hwnd ($c.Left + 2) ($c.Top + 2)
	$hitTopRight = Hit-Test $hwnd ($c.Right - 3) ($c.Top + 2)
	$hitTitle = Hit-Test $hwnd $midX ($c.Top + 20)
	Check "$name top edge resizes" ($hitTop -eq $HTTOP) "hit=$hitTop"
	Check "$name top left corner resizes" ($hitTopLeft -eq $HTTOPLEFT) "hit=$hitTopLeft"
	Check "$name top right corner resizes" ($hitTopRight -eq $HTTOPRIGHT) "hit=$hitTopRight"
	Check "$name title bar is client area" ($hitTitle -eq $HTCLIENT) "hit=$hitTitle"
}

function Test-Maximize([IntPtr]$hwnd, [string]$name) {
	[void][Win]::ShowWindow($hwnd, $SW_MAXIMIZE)
	Start-Sleep -Milliseconds 2000
	Save-Screenshot "$name-maximized"
	$c = Get-ClientScreenRect $hwnd
	$work = Get-WorkArea $hwnd
	$exact = ($c.Left -eq $work.Left) -and ($c.Top -eq $work.Top) -and ($c.Right -eq $work.Right) -and ($c.Bottom -eq $work.Bottom)
	Check "$name maximized covers the work area exactly" ([Win]::IsZoomed($hwnd) -and $exact) "client $(Format-Rect $c), work area $(Format-Rect $work)"
	$hit = Hit-Test $hwnd ([int](($c.Left + $c.Right) / 2)) ($c.Top + 1)
	Check "$name maximized top edge is client area" ($hit -eq $HTCLIENT) "hit=$hit"

	[void][Win]::ShowWindow($hwnd, $SW_RESTORE)
	Start-Sleep -Milliseconds 1500
	$w = Get-Rect $hwnd
	$c = Get-ClientScreenRect $hwnd
	Check "$name restored has no system title bar" ((-not [Win]::IsZoomed($hwnd)) -and ($c.Top -eq $w.Top)) "window $(Format-Rect $w), client $(Format-Rect $c)"
}

# Positions of the caption buttons drawn by the editor (46 px wide at 100%,
# at the end of the title bar, which has a 4 px margin).
function Get-ButtonPoint([IntPtr]$hwnd, [int]$index) {
	$c = Get-ClientScreenRect $hwnd
	$x = $c.Right - 4 - 23 - 46 * $index
	return @{ X = $x; Y = $c.Top + 20 }
}

function Test-Buttons([IntPtr]$hwnd, $process, [string]$name) {
	Place-Window $hwnd 0
	$p = Get-ButtonPoint $hwnd 1
	$hit = Hit-Test $hwnd $p.X $p.Y
	Check "$name maximize button is reported to Windows for the snap layouts" ($hit -eq $HTMAXBUTTON) "hit=$hit"
	$hitMinimize = Hit-Test $hwnd (Get-ButtonPoint $hwnd 2).X $p.Y
	Check "$name minimize button is client area" ($hitMinimize -eq $HTCLIENT) "hit=$hitMinimize"

	# Hover: the button must still highlight, and Windows 11 may show its snap layouts.
	Move-Mouse $p.X $p.Y
	Start-Sleep -Milliseconds 2500
	Save-Screenshot "$name-maximize-hover"
	Move-Mouse ($p.X - 200) ($p.Y + 200)
	Start-Sleep -Milliseconds 800

	Click $p.X $p.Y
	Start-Sleep -Milliseconds 1500
	Save-Screenshot "$name-maximize-button"
	Check "$name maximize button maximizes" ([Win]::IsZoomed($hwnd))

	$p = Get-ButtonPoint $hwnd 1
	Click $p.X $p.Y
	Start-Sleep -Milliseconds 1500
	Check "$name maximize button restores" (-not [Win]::IsZoomed($hwnd))

	$p = Get-ButtonPoint $hwnd 2
	Click $p.X $p.Y
	Start-Sleep -Milliseconds 1500
	Check "$name minimize button minimizes" ([Win]::IsIconic($hwnd))
	[void][Win]::ShowWindow($hwnd, $SW_RESTORE)
	Start-Sleep -Milliseconds 1500

	# Throw the mouse into the top right corner of the maximized window.
	[void][Win]::ShowWindow($hwnd, $SW_MAXIMIZE)
	Start-Sleep -Milliseconds 1500
	[void][Win]::SetForegroundWindow($hwnd)
	$c = Get-ClientScreenRect $hwnd
	Click ($c.Right - 1) $c.Top
	$exited = $process.WaitForExit(30000)
	Check "$name close button in the screen corner closes the window" $exited
}

# Snapping on this desktop, with a native window as a reference.
$arranging = 0
[void][Win]::SystemParametersInfo(0x0082, 0, [ref]$arranging, 0) # SPI_GETWINARRANGING
if ($arranging -eq 0) {
	Write-Output "Window arranging is off on this desktop, turning it on."
	[void][Win]::SystemParametersInfo(0x0083, 1, [IntPtr]::Zero, 0) # SPI_SETWINARRANGING
}
$formScript = "Add-Type -AssemblyName System.Windows.Forms; `$f = New-Object System.Windows.Forms.Form; `$f.Text = 'Snap reference'; [System.Windows.Forms.Application]::Run(`$f)"
$reference = Start-Process powershell.exe -ArgumentList "-NoProfile", "-Command", $formScript -PassThru
$referenceHwnd = Wait-MainWindow $reference 60
Place-Window $referenceHwnd
$rf = Get-FrameRect $referenceHwnd
$referenceSnap = Test-SnapLeft $referenceHwnd ([int](($rf.Left + $rf.Right) / 2)) ($rf.Top + 12) "reference"
Write-Output "      A native window snapped to the left half: $($referenceSnap.Snapped), $($referenceSnap.Details)"
Stop-Process -Id $reference.Id -Force -ErrorAction SilentlyContinue
Press-Escape

$renderer = @("--rendering-method", "gl_compatibility", "--rendering-driver", "opengl3_angle")

# Project manager.
$pm = Start-Engine (@("--project-manager") + $renderer) "project-manager"
$pmHwnd = Wait-MainWindow $pm 120
Check "Project manager extends to the title" (Wait-Extended $pmHwnd 60)
Place-Window $pmHwnd
Save-Screenshot "project-manager"
Test-TitleBar $pmHwnd "Project manager"
Test-Maximize $pmHwnd "Project manager"

Place-Window $pmHwnd
$c = Get-ClientScreenRect $pmHwnd
# An empty spot of the title bar, between the logo and the centered tabs.
$snap = Test-SnapLeft $pmHwnd ($c.Left + [int](($c.Right - $c.Left) * 0.25)) ($c.Top + 20) "project-manager"
if ($referenceSnap.Snapped) {
	Check "Project manager snaps to the left half when dragged to the edge" $snap.Snapped $snap.Details
} else {
	Write-Output "SKIP  Project manager snapping: native windows don't snap on this desktop either. $($snap.Details)"
}
Press-Escape
Test-Buttons $pmHwnd $pm "Project manager"
if (-not $pm.HasExited) { Stop-Process -Id $pm.Id -Force }

# Editor, with a plain Godot project.
$editorProcess = Start-Engine (@("--editor", "--path", $Project) + $renderer) "editor"
$editorHwnd = Wait-MainWindow $editorProcess 180
Check "Editor extends to the title" (Wait-Extended $editorHwnd 120)
Start-Sleep -Seconds 10
Place-Window $editorHwnd
Save-Screenshot "editor"
Test-TitleBar $editorHwnd "Editor"
Test-Maximize $editorHwnd "Editor"
Test-Buttons $editorHwnd $editorProcess "Editor"
if (-not $editorProcess.HasExited) { Stop-Process -Id $editorProcess.Id -Force }

$errors = Select-String -Path (Join-Path $OutDir "*.log") -Pattern "SCRIPT ERROR|Parse Error|CrashHandlerException|Program crashed"
if ($errors) {
	$errors
	Check "No crash or script error in the logs" $false
}

Write-Output ""
if ($script:failures -gt 0) {
	Write-Output "$($script:failures) check(s) failed. Screenshots and logs: $OutDir"
	exit 1
}
Write-Output "All title bar checks passed."
