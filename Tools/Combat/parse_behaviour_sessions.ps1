# Prints each segment of run_behaviour_sessions.ps1's logs (Saved/Logs/Behaviour<Tag>-T<tier>.log): the pilot's style,
# start and stop lines, and the Startech.Combat.Report rows. Usage: parse_behaviour_sessions.ps1 -Tag A -Tiers 2,3
param([string]$Tag = 'A', [string[]]$Tiers = @('1','2','3','4'))
foreach ($t in $Tiers) {
  $path = Join-Path $PSScriptRoot "..\..\Saved\Logs\Behaviour$Tag-T$t.log"
  if (-not (Test-Path $path)) { continue }
  "===== tier $t ($path)"
  Select-String -Path $path -Pattern '\[Pilot\] (style|stopped|started)|\[CombatReport\] (\d|Tier|Total)|session reset|Engines|player died in|Warp refused|LogStartechRespawn' |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]','' -replace 'LogStartech\w+: (Warning: )?','' } |
    Where-Object { $_ -notmatch 'Tier Enemy' }
}
