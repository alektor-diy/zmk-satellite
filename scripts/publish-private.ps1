# Requires GitHub CLI and an authenticated account. Creates only a private repo.
param([string]$Owner = '')
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
Push-Location $appRoot
try {
    & gh auth status
    if ($LASTEXITCODE) { throw 'GitHub login required: gh auth login' }
    if (-not $Owner) {
        $Owner = & gh api user --jq .login
        if ($LASTEXITCODE -or -not $Owner) { throw 'Could not resolve authenticated owner' }
    }
    if ($Owner -notmatch '^[A-Za-z0-9-]+$') { throw 'Invalid GitHub owner' }
    $repo = "$Owner/zmk-satellite-view"
    $ErrorActionPreference = 'Continue'
    $existing = & gh api "repos/$repo" 2>$null
    $repoStatus = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($repoStatus -eq 0) {
        $metadata = ($existing -join "`n") | ConvertFrom-Json
        if (-not $metadata.private) { throw 'Existing repository is public. Refusing to push.' }
    } else {
        & gh repo create $repo --private --description 'Independent Zephyr BLE status display for ZMK keyboards: XIAO nRF52840 + ST7789'
        if ($LASTEXITCODE) { throw 'Private repository creation failed' }
    }
    $isPrivate = & gh api "repos/$repo" --jq .private
    if ($LASTEXITCODE -or $isPrivate -ne 'true') { throw 'Privacy verification failed' }
    $remote = "https://github.com/$repo.git"
    $ErrorActionPreference = 'Continue'
    $origin = & git remote get-url origin 2>$null
    $originStatus = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($originStatus -eq 0) {
        if ($origin -ne $remote) { throw 'Existing origin differs; inspect it before publishing' }
    } else {
        & git remote add origin $remote
        if ($LASTEXITCODE) { throw 'Could not set origin' }
    }
    & git push -u origin main
    if ($LASTEXITCODE) { throw 'Push failed' }
    & gh repo view $repo --json url,visibility
    if ($LASTEXITCODE) { throw 'Final repository verification failed' }
} finally { Pop-Location }
