$ErrorActionPreference='Stop'
function Release-Info($Release,$Directory) {
 New-Item -ItemType Directory $Directory -Force | Out-Null
 $asset=@($Release.assets | Where-Object name -Match '^enshrouded-mod-patches-[0-9.]+\.zip$')
 if($asset.Count -ne 1 -or $asset[0].size -gt 50000000) { throw 'Unexpected patch release artifact' }
 $zip=Join-Path $Directory 'release.zip'
 Invoke-WebRequest -Uri $asset[0].browser_download_url -OutFile $zip
 if((Get-Item $zip).Length -ne $asset[0].size) { throw 'Incomplete patch release' }
 if($asset[0].digest -and $asset[0].digest -ne ('sha256:'+(Get-FileHash $zip).Hash.ToLowerInvariant())) { throw 'Release digest mismatch' }
 Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $Directory 'content')
 Get-Content (Join-Path $Directory 'content/build-info.json') -Raw | ConvertFrom-Json
}
$headers=@{'User-Agent'='Enshrouded-release-notifier';Accept='application/vnd.github+json'}
$releases=Invoke-RestMethod -Uri 'https://api.github.com/repos/Aerox912/enshrouded-mod-patches/releases?per_page=20' -Headers $headers
$current=$releases | Where-Object tag_name -EQ $env:GITHUB_REF_NAME | Select-Object -First 1
if(!$current -or $current.draft -or $current.prerelease) { throw 'Current component release is not published' }
$previous=$releases | Where-Object { !$_.draft -and !$_.prerelease -and $_.tag_name -ne $env:GITHUB_REF_NAME -and [datetime]$_.published_at -lt [datetime]$current.published_at } | Sort-Object published_at -Descending | Select-Object -First 1
$latest=Release-Info $current './current-release'
$old=if($previous){Release-Info $previous './previous-release'}else{$null}
foreach($target in @(@{Repo='enshrouded-client-installer';Field='clientFingerprint'},@{Repo='enshrouded-server-compat';Field='serverFingerprint'})) {
 if($old -and $old.($target.Field) -and $old.($target.Field) -eq $latest.($target.Field)) { Write-Host "No changes affecting $($target.Repo); no update triggered.";continue }
 @{event_type='component-released';client_payload=@{component='patches';tag=$env:GITHUB_REF_NAME}} | ConvertTo-Json -Compress | Set-Content ./dispatch.json -Encoding utf8
 gh api --method POST "repos/Aerox912/$($target.Repo)/dispatches" --input ./dispatch.json
 if($LASTEXITCODE){throw 'Package notification failed'}
}
