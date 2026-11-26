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
	[switch]$CloseApplications
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$exe = [IO.Path]::GetFullPath((Join-Path $Dir "erethea.exe"))
$close = if ($CloseApplications) { " /CLOSEAPPLICATIONS" } else { "" }

for ($i = 1; $i -le $Runs; $i++) {
	Write-Output "$(Get-Date -Format o) UPDATE $i$close"
	Get-CimInstance Win32_Process | Where-Object { $_.Name -like "erethea*" -or $_.Name -like "*setup*" -or ($_.ExecutablePath -and $_.ExecutablePath -like "$Dir*") } |
		Format-Table ProcessId, ParentProcessId, Name, ExecutablePath -AutoSize | Out-String -Width 300
	$log = Join-Path $OutDir "update-$i.log"
	$info = New-Object System.Diagnostics.ProcessStartInfo
	$info.FileName = $Setup
	$info.Arguments = "/SILENT /SUPPRESSMSGBOXES /NORESTART$close /update=1 /LOG=`"$log`""
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
		if ($exited -and ($editor -or (Get-Date) -gt $exited.AddSeconds(60))) {
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
