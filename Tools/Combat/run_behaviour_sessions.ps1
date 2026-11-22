# Phase 3 play sessions: one process per tier on test slot 3 with the expected gear. Each segment resets the combat
# log, heals, sets the pilot's style, warps to the tier's sector (fresh enemies), fights one type, and reports.
# Usage: run_behaviour_sessions.ps1 -Tiers 2,3 -Tag A [-Only 'chaser-*'] (about 10 min per tier, with the editor's
# Startech binaries built). Read the results with parse_behaviour_sessions.ps1 -Tag A.
param([string[]]$Tiers = @('1','2','3','4'), [string]$Tag = 'A', [string]$Only = '')

$exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$proj = '"' + (Resolve-Path (Join-Path $PSScriptRoot '..\..\Startech.uproject')).Path + '"'

function Upg($l, $h, $s, $e) {
  "Startech.Test.SetUpgrade Lasers $l;Startech.Test.SetUpgrade Hull $h;Startech.Test.SetUpgrade ShieldCapacity $s;Startech.Test.SetUpgrade Engines $e"
}
$tierData = @{
  '1' = @{ Sector = 'Terra';   Upg = (Upg 0 0 0 0); Ammo = 'Item.Ammo.LL1'; Give = '' }
  '2' = @{ Sector = 'Rust';    Upg = (Upg 2 1 1 0); Ammo = 'Item.Ammo.LL2'; Give = 'Startech.Test.SetAmmo Item.Ammo.LL2 9000;' }
  '3' = @{ Sector = 'Glacier'; Upg = (Upg 3 3 2 3); Ammo = 'Item.Ammo.LL3'; Give = 'Startech.Test.SetAmmo Item.Ammo.LL3 9000;' }
  '4' = @{ Sector = 'Jove';    Upg = (Upg 5 5 5 5); Ammo = 'Item.Ammo.LL3'; Give = 'Startech.Test.SetAmmo Item.Ammo.LL3 9000;' }
}
# (label, enemy type, style, dodge, distance m, seconds)
$segments = @{
  '1' = @( ,@('drifter-face', 'Drifter', 'face', 0, 40, 100) )
  '2' = @( @('chaser-face', 'Chaser', 'face', 0, 30, 90), @('chaser-circle', 'Chaser', 'circle', 0, 30, 90),
           @('chaser-backoff', 'Chaser', 'backoff', 0, 60, 90), @('chaser-backoff-near', 'Chaser', 'backoff', 0, 50, 90),
           @('ranged-still', 'Ranged', 'hold', 0, 90, 100), @('ranged-dodge', 'Ranged', 'face', 1, 90, 100) )
}
$segments['3'] = $segments['2']
$segments['4'] = $segments['2']

foreach ($t in $Tiers) {
  $s = $tierData[$t]
  $cmds = "$($s.Upg);$($s.Give)"
  foreach ($g in $segments[$t]) {
    $label, $type, $style, $dodge, $dist, $secs = $g
    if ($Only -and ($label -notlike $Only)) { continue }
    $warp = if ($s.Sector -eq 'Terra') { '' } else { "Startech.Test.Warp $($s.Sector);wait 10;" }
    $cmds += "Startech.Test.Undock;wait 6;${warp}Startech.Test.Heal;Startech.Combat.Reset;Startech.Test.PilotStyle $style $dodge 1;" +
             "Startech.Test.AutoFight $type $secs $dist $($s.Ammo) $($s.Sector);wait $($secs + 2);Startech.Test.AutoFight stop;" +
             "Startech.Combat.Report;Startech.Test.Upgrades;wait 1;"
  }
  $cmds += 'quit'
  $log = "Behaviour$Tag-T$t.log"
  $args = @($proj, '/Game/StarterMap', '-game', '-nullrhi', '-unattended', '-nosplash', "-log=$log",
            "`"-ini:Engine:[ConsoleVariables]:Startech.Flow.OnStartCommands=$cmds`"")
  $p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
  if (-not $p.WaitForExit(1500000)) { $p | Stop-Process -Force; Write-Output "tier ${t}: timed out" }
  Write-Output "tier ${t}: done"
}
