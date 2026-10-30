param([Parameter(Mandatory=$true)][string]$BinaryDirectory)
$ErrorActionPreference = 'Stop'
$BinaryDirectory = (Resolve-Path $BinaryDirectory).Path
$work = Join-Path $BinaryDirectory 'startup-smoke-logs'
New-Item -ItemType Directory -Force $work | Out-Null
function Test-Editor([string]$name, [string[]]$arguments) {
    $exe = Join-Path $BinaryDirectory $name
    # Exercise the real GUI executable, its workers, Vulkan swapchain and an
    # explicitly hidden launch. A CPU-only widget test cannot cover this path.
    $p = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $work -WindowStyle Hidden -PassThru
    try {
        if (!$p.WaitForExit(60000)) { $p.Kill(); throw "$name startup timed out" }
        if ($p.ExitCode -ne 0) { throw "$name failed with exit code $($p.ExitCode)" }
        $log = Join-Path $work ($name.Replace('.exe','.log'))
        if (!(Select-String -Path $log -SimpleMatch 'Vulkan window smoke test passed' -Quiet)) { throw "$name did not finish three Vulkan frames" }
    } finally {
        Get-ChildItem $work -Filter '*.log' | ForEach-Object { Write-Host "--- $($_.Name) ---"; Get-Content $_ }
    }
}
Test-Editor 'engine_editor.exe' @('--smoke-test')
Test-Editor 'editor_hud.exe' @('--smoke-test','--save-test-layout')
try {
    # Runtime must also consume an actual designer-saved docking layout.
    Test-Editor 'engine_editor.exe' @('--smoke-test')
    Test-Editor 'engine_editor.exe' @('--smoke-test','--safe-mode')
} finally {
    Remove-Item (Join-Path $BinaryDirectory 'editor_hud.layout') -ErrorAction SilentlyContinue
}
