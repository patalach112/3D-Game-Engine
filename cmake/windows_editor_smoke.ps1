param([Parameter(Mandatory=$true)][string]$BinaryDirectory)
$ErrorActionPreference = 'Stop'
$BinaryDirectory = (Resolve-Path $BinaryDirectory).Path
$work = Join-Path $BinaryDirectory 'startup-smoke-logs'
$layout = Join-Path $BinaryDirectory 'editor_hud.layout'
# This is an isolated CI test, never destroy a developer's existing document.
if (Test-Path $layout) { throw "Smoke test requires a clean binary directory: $layout already exists" }
New-Item -ItemType Directory -Force $work | Out-Null
$script:runNumber = 0
function Test-Editor([string]$name, [string[]]$arguments) {
    $exe = Join-Path $BinaryDirectory $name
    $script:runNumber++
    $case = Join-Path $work ('{0:00}-{1}' -f $script:runNumber, $name.Replace('.exe',''))
    New-Item -ItemType Directory -Force $case | Out-Null
    # A previous invocation must never supply this invocation's success marker.
    Get-ChildItem $case -Filter '*.log' | Remove-Item
    # Exercise the real GUI executable, its workers, Vulkan swapchain and an
    # explicitly hidden launch. A CPU-only widget test cannot cover this path.
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $case -WindowStyle Hidden -PassThru
    try {
        if (!$p.WaitForExit(60000)) { $p.Kill(); throw "$name startup timed out" }
        if ($p.ExitCode -ne 0) { throw "$name failed with exit code $($p.ExitCode)" }
        $log = Join-Path $case ($name.Replace('.exe','.log'))
        if (!(Select-String -Path $log -SimpleMatch 'Vulkan window smoke test passed' -Quiet)) { throw "$name did not finish three Vulkan frames" }
    } finally {
        Get-ChildItem $case -Filter '*.log' | ForEach-Object { Write-Host "--- $($_.Name) ---"; Get-Content $_ }
    }
}
try {
    Test-Editor 'engine_editor.exe' @('--smoke-test')
    Test-Editor 'editor_hud.exe' @('--smoke-test','--save-test-layout')
    # Runtime must also consume an actual designer-saved docking layout.
    Test-Editor 'engine_editor.exe' @('--smoke-test')
    Test-Editor 'engine_editor.exe' @('--smoke-test','--safe-mode')
} finally {
    Remove-Item $layout -ErrorAction SilentlyContinue
}
