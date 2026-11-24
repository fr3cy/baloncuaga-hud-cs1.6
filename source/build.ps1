param([Parameter(Mandatory=$true)][string]$Sdk,[Parameter(Mandatory=$true)][string]$MinHook,[string]$BinDir='')
$ErrorActionPreference='Stop'
$taskGcc=if($BinDir){Join-Path $BinDir 'i686-w64-mingw32-gcc.exe'}else{'gcc'}
$taskGxx=if($BinDir){Join-Path $BinDir 'i686-w64-mingw32-g++.exe'}else{'g++'}
$taskAr=if($BinDir){Join-Path $BinDir 'i686-w64-mingw32-ar.exe'}else{'ar'}
if($BinDir -and !(Test-Path -LiteralPath $taskAr)){$taskAr=Join-Path $BinDir 'ar.exe'}
$taskWindres=if($BinDir){Join-Path $BinDir 'i686-w64-mingw32-windres.exe'}else{'windres'}
$taskFt=(Join-Path $PSScriptRoot 'weave/freetype')
$taskBuild=Join-Path $PSScriptRoot '.build'
New-Item $taskBuild -ItemType Directory -Force | Out-Null
$taskSources=@('base/ftbase','base/ftinit','base/ftsystem','base/ftdebug','base/ftbitmap','base/ftglyph','base/ftsynth','base/ftstroke','base/ftbbox','base/ftbdf','base/ftcid','base/ftfstype','base/ftgasp','base/ftmm','base/ftotval','base/ftpatent','base/ftpfr','base/fttype1','base/ftwinfnt','autofit/autofit','truetype/truetype','type1/type1','cff/cff','cid/type1cid','pfr/pfr','type42/type42','winfonts/winfnt','pcf/pcf','bdf/bdf','psaux/psaux','psnames/psnames','pshinter/pshinter','sfnt/sfnt','smooth/smooth','raster/raster','sdf/sdf','svg/svg','gzip/ftgzip','lzw/ftlzw')
foreach($taskUnit in $taskSources){
 $taskName=($taskUnit -split '/')[-1]
 & $taskGcc -m32 -O2 -DFT2_BUILD_LIBRARY "-I$taskFt/include" -c "$taskFt/src/$taskUnit.c" -o "$taskBuild/$taskName.o"
 if($LASTEXITCODE -ne 0){throw "FreeType compilation: $taskUnit"}
}
& $taskAr rcs "$taskBuild/libfreetype.a" (Get-ChildItem "$taskBuild/*.o" | ForEach-Object FullName)
if($LASTEXITCODE -ne 0){throw 'FreeType archive failed'}

# Resources are separate from FreeType objects so repeated builds cannot archive them.
$taskResource=Join-Path $taskBuild 'skyboxes.res'
Push-Location (Join-Path $PSScriptRoot 'embedded_skyboxes')
try {
 & $taskWindres --target=pe-i386 -i skyboxes.rc -O coff -o $taskResource
 if($LASTEXITCODE -ne 0){throw 'Skybox resources compilation failed'}
} finally {Pop-Location}

$taskInstallerResource=Join-Path $taskBuild 'install.res'
Push-Location (Join-Path $PSScriptRoot 'embedded_install')
try {
 & $taskWindres --target=pe-i386 -i files.rc -O coff -o $taskInstallerResource
 if($LASTEXITCODE -ne 0){throw 'Installer resources compilation failed'}
} finally {Pop-Location}

$taskRoot=$PSScriptRoot
$taskOutput=Join-Path (Split-Path $PSScriptRoot -Parent) "CS16Customizer.asi"
$taskUnits=@("$taskRoot/customizer.cpp","$taskRoot/weave/imgui/imgui.cpp","$taskRoot/weave/imgui/imgui_draw.cpp","$taskRoot/weave/imgui/imgui_widgets.cpp","$taskRoot/weave/imgui/imgui_tables.cpp","$taskRoot/weave/imgui/imgui_freetype.cpp","$taskRoot/weave/ui/ui.cpp")
# Keep build-machine paths out of the ASI (assert messages embed __FILE__).
$taskParent=Split-Path $taskRoot -Parent
& $taskGxx -m32 -shared -mstackrealign -O2 -g0 -s "-ffile-prefix-map=$taskRoot=source" "-ffile-prefix-map=$($taskRoot -replace '\\','/')=source" "-ffile-prefix-map=$taskParent=." "-ffile-prefix-map=$($taskParent -replace '\\','/')=." -std=c++17 -static -static-libgcc -static-libstdc++ '-Wl,--kill-at' -DIMGUI_DEFINE_MATH_OPERATORS "-I$Sdk/common" "-I$Sdk/engine" "-I$Sdk/public" "-I$Sdk/pm_shared" "-I$MinHook/include" "-I$MinHook/src/hde" "-I$taskRoot/weave/freetype/include" $taskUnits $taskResource $taskInstallerResource "$MinHook/src/buffer.c" "$MinHook/src/hook.c" "$MinHook/src/trampoline.c" "$MinHook/src/hde/hde32.c" "$taskBuild/libfreetype.a" -o $taskOutput -lkernel32 -luser32 -lopengl32 -lgdi32 -ladvapi32 -lws2_32
if($LASTEXITCODE -ne 0){throw 'ASI compilation failed'}

