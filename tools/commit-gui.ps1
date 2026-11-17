$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $repoRoot
try
{
    & git diff --cached --quiet
    if ($LASTEXITCODE -ne 0)
    {
        throw 'The index already contains changes. Review and commit or unstage them before running this script.'
    }
    $groups = @(
        @{
            Message = 'build(gui): vendor pinned raylib 5.5 and raygui 4.0'
            Paths = @('.gitignore', 'third_party')
        },
        @{
            Message = 'feat(gui): integrate racing screens and algorithm playback'
            Paths = @('CMakeLists.txt', 'src/app')
        },
        @{
            Message = 'docs(gui): document controls verification and backend handoff'
            Paths = @('README.md', 'docs/gui-backend-handoff.md', 'docs/gui-verification.md', 'tools/commit-gui.ps1')
        }
    )
    foreach ($group in $groups)
    {
        & git add -- @($group.Paths)
        if ($LASTEXITCODE -ne 0) { throw 'Git add failed. No further commits attempted.' }
        & git diff --cached --quiet
        if ($LASTEXITCODE -eq 0) { continue }
        if ($LASTEXITCODE -ne 1) { throw 'Could not inspect staged changes.' }
        & git diff --cached --check
        if ($LASTEXITCODE -ne 0) { throw 'Staged diff check failed.' }
        & git commit -m $group.Message
        if ($LASTEXITCODE -ne 0) { throw 'Commit failed. Review the staged changes before retrying.' }
    }
    & git log -3 --oneline
}
finally
{
    Pop-Location
}
