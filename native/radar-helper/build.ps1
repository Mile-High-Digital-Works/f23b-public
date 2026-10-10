param([string]$OutputDirectory = "$PSScriptRoot/../../build/native-radar")
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -shared -static-libgcc -static-libstdc++ "$PSScriptRoot/Radar.cpp" -lbcrypt -o "$OutputDirectory/F23B_Radar.dll"
if ($LASTEXITCODE) { throw 'Native radar build failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/RadarTune_test.cpp" -o "$OutputDirectory/RadarTune_test.exe"
if ($LASTEXITCODE) { throw 'Native radar test build failed' }
& "$OutputDirectory/RadarTune_test.exe"
if ($LASTEXITCODE) { throw 'Native radar tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/SupportHook_test.cpp" -o "$OutputDirectory/SupportHook_test.exe"
if ($LASTEXITCODE) { throw 'Support hook test build failed' }
& "$OutputDirectory/SupportHook_test.exe"
if ($LASTEXITCODE) { throw 'Support hook tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/MissileProfile_test.cpp" -o "$OutputDirectory/MissileProfile_test.exe"
if ($LASTEXITCODE) { throw 'Missile profile test build failed' }
& "$OutputDirectory/MissileProfile_test.exe"
if ($LASTEXITCODE) { throw 'Missile profile tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/MaliceEnergy_test.cpp" -o "$OutputDirectory/MaliceEnergy_test.exe"
if ($LASTEXITCODE) { throw 'MALICE energy test build failed' }
& "$OutputDirectory/MaliceEnergy_test.exe"
if ($LASTEXITCODE) { throw 'MALICE energy tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/SupportDiag_test.cpp" -o "$OutputDirectory/SupportDiag_test.exe"
if ($LASTEXITCODE) { throw 'Support diagnostics test build failed' }
& "$OutputDirectory/SupportDiag_test.exe"
if ($LASTEXITCODE) { throw 'Support diagnostics tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/PayloadAlias_test.cpp" -o "$OutputDirectory/PayloadAlias_test.exe"
if ($LASTEXITCODE) { throw 'Payload alias test build failed' }
& "$OutputDirectory/PayloadAlias_test.exe"
if ($LASTEXITCODE) { throw 'Payload alias tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/LaunchZone_test.cpp" -o "$OutputDirectory/LaunchZone_test.exe"
if ($LASTEXITCODE) { throw 'Launch zone test build failed' }
& "$OutputDirectory/LaunchZone_test.exe"
if ($LASTEXITCODE) { throw 'Launch zone tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/PayloadAccessHook_test.cpp" -o "$OutputDirectory/PayloadAccessHook_test.exe"
if ($LASTEXITCODE) { throw 'Payload accessor test build failed' }
& "$OutputDirectory/PayloadAccessHook_test.exe"
if ($LASTEXITCODE) { throw 'Payload accessor tests failed' }
& g++ -std=c++17 -O2 -Wall -Wextra -Werror -static-libgcc -static-libstdc++ "$PSScriptRoot/SaRangeHook_test.cpp" -o "$OutputDirectory/SaRangeHook_test.exe"
if ($LASTEXITCODE) { throw 'SA range test build failed' }
& "$OutputDirectory/SaRangeHook_test.exe"
if ($LASTEXITCODE) { throw 'SA range tests failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath "$OutputDirectory/F23B_Radar.dll"
