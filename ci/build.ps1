$ErrorActionPreference='Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
python -m unittest -v
if($LASTEXITCODE) { throw 'Patch tests failed' }
python -m pip install --disable-pip-version-check pyinstaller==6.16.0
if($LASTEXITCODE) { throw 'Build dependency install failed' }
python -m PyInstaller --clean --noconfirm --onefile --name PatchTool --add-data 'patches;patches' --add-data 'catalog.json;.' --add-data 'profiles;profiles' --add-data 'defaults;defaults' patcher.py
if($LASTEXITCODE) { throw 'Patch utility build failed' }
$version=(Get-Content ./catalog.json -Raw | ConvertFrom-Json).version
& ./dist/PatchTool.exe --help
if($LASTEXITCODE) { throw 'Frozen Windows utility did not start' }
$stage='build/package'
New-Item -ItemType Directory $stage -Force | Out-Null
foreach($name in @('catalog.json','client-files.json','client-imports.json','CREDITS.md','LICENSE','README.md','patcher.py','profiles','defaults','patches')) {
  Copy-Item -LiteralPath $name -Destination $stage -Recurse
}
Copy-Item dist/PatchTool.exe $stage
$pythonLicense=Join-Path (Split-Path (Get-Command python).Source) 'LICENSE.txt'
if(Test-Path $pythonLicense) { Copy-Item $pythonLicense "$stage/PYTHON-LICENSE.txt" }
python -c 'import importlib.metadata,pathlib,shutil; p=pathlib.Path(importlib.metadata.distribution("pyinstaller").locate_file("PyInstaller"))/".."/"pyinstaller-6.16.0.dist-info"/"licenses"; shutil.copytree(p,"build/package/PYINSTALLER-LICENSES",dirs_exist_ok=True)'
if($LASTEXITCODE) { throw 'Runtime license collection failed' }
python ./tools/release-info.py
if($LASTEXITCODE) { throw 'Release provenance failed' }
Compress-Archive -Path "$stage/*" -DestinationPath "dist/enshrouded-mod-patches-$version.zip"
Get-ChildItem dist -File | ForEach-Object { '{0}  {1}' -f (Get-FileHash $_.FullName).Hash.ToLowerInvariant(),$_.Name } | Set-Content dist/SHA256SUMS.txt -Encoding ascii
