param(
    [string]$Exe = "build/cmake-release/try4_perft_bench.exe",
    [string]$Data = "tests/data/perft/reference.tsv",
    [int]$Depth = 5,
    [string]$Position = ""
)

$ErrorActionPreference = "Stop"

if ($Depth -lt 1 -or $Depth -gt 6) {
    throw "Depth must be between 1 and 6."
}
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Perft executable not found: $Exe"
}
if (-not (Test-Path -LiteralPath $Data)) {
    throw "Reference data not found: $Data"
}

$rows = Import-Csv -LiteralPath $Data -Delimiter "`t"
$failed = $false
$ran = 0

foreach ($row in $rows) {
    if ($Position -ne "" -and $row.name -ne $Position) {
        continue
    }

    $expected = [uint64]$row."d$Depth"
    $output = & $Exe $row.fen $Depth
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Perft failed for $($row.name) at depth $Depth"
        exit 1
    }

    $nodesLine = $output | Where-Object { $_ -like "nodes=*" } | Select-Object -First 1
    if ($null -eq $nodesLine) {
        Write-Error "No node count in output for $($row.name)"
        exit 1
    }
    $nodes = [uint64]($nodesLine -replace "^nodes=", "")
    $ran++

    if ($nodes -ne $expected) {
        Write-Host "FAIL $($row.name) depth $Depth expected $expected got $nodes"
        $failed = $true
    } else {
        Write-Host "PASS $($row.name) depth $Depth nodes $nodes"
    }
}

if ($ran -eq 0) {
    Write-Error "No reference positions matched '$Position'."
    exit 1
}
if ($failed) {
    exit 1
}
