<#
.SYNOPSIS
    Turn an OpenCppCoverage cobertura report into a markdown summary, and post
    it as a single self-updating pull request comment.

.DESCRIPTION
    Kept as a script rather than inline workflow steps so it can be run locally
    against the same report CI produces:

        pwsh tests/tools/coverage-report.ps1 -ReportPath coverage.xml

    Merging matters. OpenCppCoverage emits one <package> per module, so a header
    compiled into several test binaries appears several times. Coverage is the
    union of the covered lines, not the best single run, so the line numbers are
    unioned rather than the totals compared.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ReportPath,
    # Reported but not enforced. The rollout is incomplete, so a gate here would
    # fail every PR that adds a saver before its tests.
    [double]$Threshold = 60.0,
    [string]$CommentMarker = '<!-- saver-coverage-report -->'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ReportPath)) {
    throw "coverage report not found: $ReportPath"
}

[xml]$report = Get-Content $ReportPath

# file path -> hashtable of covered line numbers, plus the full line set
$covered = @{}
$known = @{}

foreach ($package in $report.coverage.packages.package) {
    foreach ($class in $package.classes.class) {
        $file = $class.filename
        if (-not $file) { continue }
        if (-not $known.ContainsKey($file)) {
            $known[$file] = @{}
            $covered[$file] = @{}
        }
        foreach ($line in $class.lines.line) {
            $n = [int]$line.number
            $known[$file][$n] = $true
            if ([int]$line.hits -gt 0) { $covered[$file][$n] = $true }
        }
    }
}

$rows = @()
$totalHit = 0
$totalLines = 0

foreach ($file in ($known.Keys | Sort-Object)) {
    $lineCount = $known[$file].Count
    if ($lineCount -eq 0) { continue }   # data-only headers generate no code
    $hitCount = $covered[$file].Count
    $totalHit += $hitCount
    $totalLines += $lineCount
    $rows += [pscustomobject]@{
        Name    = Split-Path $file -Leaf
        Hit     = $hitCount
        Lines   = $lineCount
        Percent = [math]::Round(100.0 * $hitCount / $lineCount, 1)
    }
}

if ($totalLines -eq 0) {
    throw "report contains no counted lines - did --sources match anything? Note the MSVC CRT's own paths contain \src\, so a bare 'src' filter matches the runtime too."
}

$overall = [math]::Round(100.0 * $totalHit / $totalLines, 1)

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine($CommentMarker)
[void]$sb.AppendLine("## Saver test coverage: **$overall%**")
[void]$sb.AppendLine()
[void]$sb.AppendLine("$totalHit of $totalLines counted lines in ``src/``, target $Threshold%.")
[void]$sb.AppendLine()
[void]$sb.AppendLine('| Module | Covered | Lines | Coverage |')
[void]$sb.AppendLine('|---|---:|---:|---:|')

foreach ($row in ($rows | Sort-Object Percent)) {
    $mark = if ($row.Percent -ge $Threshold) { 'ok' } else { 'below target' }
    [void]$sb.AppendLine("| ``$($row.Name)`` | $($row.Hit) | $($row.Lines) | **$($row.Percent)%** ($mark) |")
}

[void]$sb.AppendLine("| **Total** | **$totalHit** | **$totalLines** | **$overall%** |")
[void]$sb.AppendLine()
[void]$sb.AppendLine('<sub>Only savers with a test binary appear. Coverage is measured in Debug, since Release inlining makes line attribution meaningless.</sub>')

$markdown = $sb.ToString()
Write-Output $markdown

if ($env:GITHUB_STEP_SUMMARY) {
    $markdown | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Encoding utf8 -Append
}

# --- pull request comment --------------------------------------------------

if (-not $env:GITHUB_REPOSITORY -or -not $env:PR_NUMBER) {
    Write-Host 'Not a pull request build; skipping the comment.'
    return
}

$bodyFile = Join-Path ([System.IO.Path]::GetTempPath()) 'coverage-comment.md'
$markdown | Out-File -FilePath $bodyFile -Encoding utf8

$existing = gh api "repos/$env:GITHUB_REPOSITORY/issues/$env:PR_NUMBER/comments" --paginate |
    ConvertFrom-Json |
    Where-Object { $_.body -like "*$CommentMarker*" } |
    Select-Object -First 1

if ($existing) {
    Write-Host "Updating existing comment $($existing.id)."
    gh api --method PATCH "repos/$env:GITHUB_REPOSITORY/issues/comments/$($existing.id)" -F "body=@$bodyFile" | Out-Null
} else {
    Write-Host 'Posting a new coverage comment.'
    gh api --method POST "repos/$env:GITHUB_REPOSITORY/issues/$env:PR_NUMBER/comments" -F "body=@$bodyFile" | Out-Null
}
