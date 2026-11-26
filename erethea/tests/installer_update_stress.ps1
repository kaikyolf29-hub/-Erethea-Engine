# Investigation of the hang of the installer's "Update silently" step.
#
# Updates an installed Erethea many times the way the editor does (and the
# installer test), with or without /CLOSEAPPLICATIONS, and stops the editor
# that the installer starts again. Prints what runs before each update.
param(
	[Parameter(Mandatory = $true)][string]$Setup,
	[Parameter(Mandatory = $true)][string]$Dir,
	[Parameter(Mandatory = $true)][string]$OutDir,
	[int]$Runs = 10,
	[switch]$CloseApplications,
	# Without /update=1: the installer doesn't start the editor again.
	[switch]$NoReopen,
	# No installer: only starts the installed editor (the project manager, with
	# the default renderer, as the installer does) and stops it.
	[switch]$EditorOnly,
	# Before each update, uninstall, install again and open a project with the
	# installed editor, as the installer test does before its one update.
	[string]$FreshInstallProject = ""
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$exe = [IO.Path]::GetFullPath((Join-Path $Dir "erethea.exe"))
$close = if ($CloseApplications) { " /CLOSEAPPLICATIONS" } else { "" }
$update = if ($NoReopen) { "" } else { " /update=1" }

if ($EditorOnly) {
	for ($i = 1; $i -le $Runs; $i++) {
		$start = Get-Date
		Write-Output "$(Get-Date -Format o) EDITOR $i"
		$process = Start-Process -FilePath $exe -PassThru
		Start-Sleep -Seconds 4
		Write-Output ("      running: {0}, after {1:N1} s" -f (-not $process.HasExited), ((Get-Date) - $start).TotalSeconds)
		Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
		Start-Sleep -Seconds 1
	}
	Write-Output "$(Get-Date -Format o) DONE"
	exit 0
}

for ($i = 1; $i -le $Runs; $i++) {
	if ($FreshInstallProject -ne "") {
		$uninstaller = Join-Path $Dir "unins000.exe"
		if (Test-Path $uninstaller) {
			Start-Process -Wait -FilePath $uninstaller -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" | Out-Null
		}
		$p = Start-Process -Wait -PassThru -FilePath $Setup -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/CURRENTUSER", "/NORESTART", "/DIR=`"$Dir`""
		Write-Output "$(Get-Date -Format o) INSTALLED again (exit code $($p.ExitCode))"
		& $exe --headless --import --path $FreshInstallProject 2>&1 | Out-Null
		& $exe --headless --path $FreshInstallProject 2>&1 | Out-Null
		Write-Output "$(Get-Date -Format o) opened the project with the installed editor"
	}
	Write-Output "$(Get-Date -Format o) UPDATE $i$close$update"
	Get-CimInstance Win32_Process | Where-Object { $_.Name -like "erethea*" -or $_.Name -like "*setup*" -or ($_.ExecutablePath -and $_.ExecutablePath -like "$Dir*") } |
		Format-Table ProcessId, ParentProcessId, Name, ExecutablePath -AutoSize | Out-String -Width 300
	$log = Join-Path $OutDir "update-$i.log"
	$info = New-Object System.Diagnostics.ProcessStartInfo
	$info.FileName = $Setup
	$info.Arguments = "/SILENT /SUPPRESSMSGBOXES /NORESTART$close$update /LOG=`"$log`""
	$info.UseShellExecute = $true
	$start = Get-Date
	$p = [System.Diagnostics.Process]::Start($info)
	$editor = $null
	$exited = $null
	$deadline = $start.AddSeconds(240)
	while ($true) {
		foreach ($e in @(Get-Process -Name erethea -ErrorAction SilentlyContinue)) {
			if ($e.Path -and [IO.Path]::GetFullPath($e.Path) -eq $exe) {
				if (-not $editor) {
					$editor = $e.Id
					Write-Output ("      the installer started the editor again after {0:N1} s" -f ((Get-Date) - $start).TotalSeconds)
				}
				Stop-Process -Id $e.Id -Force -ErrorAction SilentlyContinue
			}
		}
		if ($p.HasExited -and -not $exited) {
			$exited = Get-Date
			Write-Output ("      the installer finished with exit code {0} after {1:N1} s" -f $p.ExitCode, ($exited - $start).TotalSeconds)
		}
		if ($exited -and ($editor -or $NoReopen -or (Get-Date) -gt $exited.AddSeconds(60))) {
			break
		}
		if ((Get-Date) -gt $deadline) {
			Write-Output "      HUNG: the installer didn't finish in 4 minutes. The end of its log:"
			if (Test-Path $log) { Get-Content $log -Tail 40 }
			break
		}
		Start-Sleep -Milliseconds 100
	}
	Get-CimInstance Win32_Process | Where-Object { $_.Name -like "erethea*" -or ($_.ExecutablePath -and $_.ExecutablePath -like "*\is-*.tmp\*") } |
		ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}
Write-Output "$(Get-Date -Format o) DONE"
