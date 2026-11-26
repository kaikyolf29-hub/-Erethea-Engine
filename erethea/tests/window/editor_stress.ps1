# Investigation of the fast fail (0xC0000409) of the editor in the window test.
#
# Opens the editor on a fresh copy of a project many times, as the window test
# does (the project manager first, then the editor with the first import, on
# ANGLE), each time under ProcDump, and keeps the logs and the dump of each run
# that ends by itself with an error.
param(
	[Parameter(Mandatory = $true)][string]$Editor,
	[Parameter(Mandatory = $true)][string]$Project,
	[Parameter(Mandatory = $true)][string]$OutDir,
	[Parameter(Mandatory = $true)][string]$ProcDump,
	[int]$Runs = 25,
	[int]$Seconds = 25
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$renderer = @("--rendering-method", "gl_compatibility", "--rendering-driver", "opengl3_angle")
$crashes = 0

for ($i = 1; $i -le $Runs; $i++) {
	$run = Join-Path $OutDir ("run{0:D2}" -f $i)
	New-Item -ItemType Directory -Force -Path $run | Out-Null
	$dir = Join-Path $run "project"
	Copy-Item -Recurse -Path $Project -Destination $dir
	# Every other run is like the first one on the machine: no editor data yet.
	$fresh = ($i % 2) -eq 1
	if ($fresh) {
		Remove-Item -Recurse -Force (Join-Path $env:APPDATA "Erethea") -ErrorAction SilentlyContinue
		Remove-Item -Recurse -Force (Join-Path $env:LOCALAPPDATA "Erethea") -ErrorAction SilentlyContinue
	}

	$pm = Start-Process -FilePath $Editor -ArgumentList (@("--project-manager") + $renderer) -PassThru `
		-RedirectStandardOutput (Join-Path $run "project-manager.log") -RedirectStandardError (Join-Path $run "project-manager.err.log")
	Start-Sleep -Seconds 8
	if (-not $pm.HasExited) { Stop-Process -Id $pm.Id -Force }
	Start-Sleep -Seconds 1

	$start = Get-Date
	$editor = Start-Process -FilePath $Editor -ArgumentList (@("--editor", "--verbose", "--path", $dir) + $renderer) -PassThru `
		-RedirectStandardOutput (Join-Path $run "editor.log") -RedirectStandardError (Join-Path $run "editor.err.log")
	$dumps = Join-Path $run "dumps"
	New-Item -ItemType Directory -Force -Path $dumps | Out-Null
	# -e: on an unhandled (second chance) exception, which is how a fast fail
	# reaches a debugger.
	$pd = Start-Process -FilePath $ProcDump -ArgumentList "-accepteula", "-mp", "-e", $editor.Id, $dumps -PassThru -WindowStyle Hidden `
		-RedirectStandardOutput (Join-Path $run "procdump.log") -RedirectStandardError (Join-Path $run "procdump.err.log")

	$deadline = $start.AddSeconds($Seconds)
	while (-not $editor.HasExited -and (Get-Date) -lt $deadline) {
		Start-Sleep -Milliseconds 250
	}
	$crashed = $false
	if ($editor.HasExited) {
		$code = $editor.ExitCode
		$after = ((Get-Date) - $start).TotalSeconds
		Write-Output ("RUN {0:D2} (fresh: {1}): the editor exited with code 0x{2:X8} after {3:N1} s" -f $i, $fresh, $code, $after)
		$crashed = $code -ne 0
	} else {
		Write-Output ("RUN {0:D2} (fresh: {1}): still running after {2} s" -f $i, $fresh, $Seconds)
		Stop-Process -Id $editor.Id -Force
	}
	# ProcDump writes the dump and ends with the process.
	for ($w = 0; $w -lt 40 -and -not $pd.HasExited; $w++) {
		Start-Sleep -Milliseconds 500
	}
	if (-not $pd.HasExited) { Stop-Process -Id $pd.Id -Force }

	if ($crashed) {
		$crashes++
		Get-Content (Join-Path $run "procdump.log") -ErrorAction SilentlyContinue | Select-Object -Last 20
		Get-Content (Join-Path $run "editor.log") -ErrorAction SilentlyContinue | Select-Object -Last 30
		Remove-Item -Recurse -Force (Join-Path $dir ".godot") -ErrorAction SilentlyContinue
	} else {
		Remove-Item -Recurse -Force $run
	}
}

Write-Output "CRASHES $crashes of $Runs"
