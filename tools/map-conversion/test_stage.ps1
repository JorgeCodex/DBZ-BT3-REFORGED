param(
    [string]$Map = 'earth-tree.unk',
    [string]$Run = 'earth-tree',
    [string]$Renderer = 'none',
    [int]$Seconds = 20
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$test = Join-Path $root '../flight-test'
$env:BT3_DATA = Join-Path $root 'gamedata'
$env:BT3_STAGES = Join-Path $root 'stage'
$env:BT3_DEMO = '1'
$env:BT3_TEST_STAGE = '36'
$env:BT3_TEST_SCREEN = '0'
$env:BT3_TRACE = '60'
$env:BT3_GS = $Renderer
$env:BT3_VERIFY = '0'
$env:BT3_NOSOUND = '1'
$env:BT3_NOMOVIE = '1'
$env:BT3_UPDATE_CHECK = '0'
$env:BT3_UNCAPPED = '1'
$env:BT3_SHOT = '37'
$env:BT3_SHOT_DIR = Join-Path $root "$Run-shots"
New-Item -ItemType Directory -Force -Path $env:BT3_SHOT_DIR | Out-Null
$mapPath = if ([System.IO.Path]::IsPathRooted($Map)) { $Map } else { Join-Path $root $Map }
Copy-Item -LiteralPath $mapPath -Destination (Join-Path $env:BT3_STAGES 'dragon-adventure-earth.unk') -Force
Copy-Item -LiteralPath $mapPath -Destination (Join-Path $env:BT3_DATA '70000.bin') -Force
$o=Join-Path $root "$Run.out"
$e=Join-Path $root "$Run.err"
$p=Start-Process -FilePath (Join-Path $test 'Tenkaichi3Decomp.exe') -WorkingDirectory $test -WindowStyle Hidden -RedirectStandardOutput $o -RedirectStandardError $e -PassThru
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); $p.WaitForExit(); Write-Output 'Probe reached its time limit' }
Write-Output "Exit $($p.ExitCode)"
Get-Content -LiteralPath $e -TotalCount 17
Get-Content -LiteralPath $o -Tail 10
