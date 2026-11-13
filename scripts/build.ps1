param(
    [ValidateSet('display','ble','status')][string]$Stage = 'display',
    [string]$BuildDir = '',
    [string]$ExtraConfig = ''
)
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $appRoot "build-$Stage" }
$westArgs = @('build', '-b', 'xiao_ble/nrf52840', $appRoot, '-d', $BuildDir,
              '--', "-DSATELLITE_STAGE=$Stage")
if ($ExtraConfig) {
    $westArgs += "-DEXTRA_CONF_FILE=$((Resolve-Path -LiteralPath $ExtraConfig).Path)"
}
& west @westArgs
if ($LASTEXITCODE) { throw "Build failed ($LASTEXITCODE)" }
Write-Host "UF2: $BuildDir\zephyr\zephyr.uf2"
