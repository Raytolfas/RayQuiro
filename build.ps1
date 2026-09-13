$cxxCompiler = "clang++"
if (-not (Get-Command $cxxCompiler -ErrorAction SilentlyContinue)) {
    $cxxCompiler = "g++"
}

$cCompiler = "clang"
if (-not (Get-Command $cCompiler -ErrorAction SilentlyContinue)) {
    $cCompiler = "gcc"
}

$isWindows = $IsWindows -or ($PSVersionTable.PSEdition -eq "Desktop") -or ([System.Environment]::OSVersion.Platform -eq "Win32NT")
$isLinux   = $IsLinux
$isMacOS   = $IsMacOS

if ($isWindows) {
    $platformName  = "windows"
    $output        = "rqio-next.exe"
    $coreOutput    = "rqio_core.dll"
    $platformLibs  = @("-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32", "-ladvapi32", "-luser32")
    $sharedFlag    = "-shared"
} elseif ($isLinux) {
    $platformName  = "linux"
    $output        = "rqio"
    $coreOutput    = "rqio_core.so"
    $platformLibs  = @("-lGL", "-lX11", "-lXrandr", "-lXinerama", "-lXi", "-lXcursor", "-lpthread", "-lm", "-ldl")
    $sharedFlag    = "-shared"
} elseif ($isMacOS) {
    $platformName  = "macos"
    $output        = "rqio"
    $coreOutput    = "rqio_core.dylib"
    $platformLibs  = @("-framework", "OpenGL", "-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreFoundation", "-framework", "CoreVideo", "-pthread", "-lm")
    $sharedFlag    = "-shared"
} else {
    Write-Error "Unsupported platform."
    exit 1
}

$vulkanSdkBin = $null
if ($isWindows) {
    if ($env:VULKAN_SDK) {
        $candidate = Join-Path $env:VULKAN_SDK "Bin"
        if (Test-Path $candidate) {
            $vulkanSdkBin = $candidate
        }
    }
    if (-not $vulkanSdkBin -and (Test-Path "G:\VulkanSDK\Bin")) {
        $vulkanSdkBin = "G:\VulkanSDK\Bin"
    }
}

$glslc = $null
if ($vulkanSdkBin) {
    $glslcCandidate = Join-Path $vulkanSdkBin "glslc.exe"
    if (Test-Path $glslcCandidate) {
        $glslc = $glslcCandidate
    }
}
if (-not $glslc) {
    $glslcCommand = Get-Command glslc -ErrorAction SilentlyContinue
    if ($glslcCommand) {
        $glslc = $glslcCommand.Source
    }
}

$supportsLto = $false
if ($cxxCompiler -eq "g++" -or $cxxCompiler -like "*\\g++.exe" -or $cxxCompiler -like "*/g++") {
    $supportsLto = $true
}


$raylibObjectRoot = Join-Path ".cache" ("rqio-raylib-" + [DateTime]::UtcNow.ToString("yyyyMMddHHmmssfff"))
New-Item -ItemType Directory -Force -Path $raylibObjectRoot | Out-Null

if ($glslc) {
    $shaderSourceRoot = "assets/vulkan"
    $shaderOutputs = @(
        @{ Source = (Join-Path $shaderSourceRoot "preview.vert"); Output = (Join-Path $shaderSourceRoot "preview.vert.spv") },
        @{ Source = (Join-Path $shaderSourceRoot "preview.frag"); Output = (Join-Path $shaderSourceRoot "preview.frag.spv") }
    )
    foreach ($shader in $shaderOutputs) {
        & $glslc $shader.Source "-o" $shader.Output
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    Write-Host "Compiled Vulkan preview shaders with $glslc"
} else {
    Write-Warning "glslc was not found. Vulkan graphics pipeline will stay disabled until preview shaders are compiled."
}

$commonNativeCompileFlags = @(
    "-O2",
    "-DNDEBUG",
    "-fvisibility=hidden",
    "-ffunction-sections",
    "-fdata-sections",
    "-fstack-protector-strong",
    "-fno-ident"
)

if ($supportsLto) {
    $commonNativeCompileFlags += "-flto"
}

if ($isWindows) {
    $commonNativeLinkFlags = @(
        "-s",
        "-static",
        "-static-libstdc++",
        "-static-libgcc",
        "-Wl,--gc-sections",
        "-Wl,--strip-all",
        "-Wl,--dynamicbase",
        "-Wl,--nxcompat",
        "-Wl,--high-entropy-va"
    )
} elseif ($isMacOS) {
    $commonNativeLinkFlags = @("-dead_strip")
} else {
    $commonNativeLinkFlags = @("-s", "-Wl,--gc-sections", "-Wl,--strip-all")
}

if ($supportsLto) {
    $commonNativeLinkFlags += "-flto"
}

# Runs a compiler/linker command and filters out known harmless MinGW duplicate-section
# warnings that appear when mixing clang++/g++ with MSYS2 libstdc++.a
function Invoke-Compiler {
    param([string]$Exe, [string[]]$CompilerArgs)

    $combined = & $Exe @CompilerArgs 2>&1
    $exitCode = $LASTEXITCODE

    foreach ($line in $combined) {
        $text = $line.ToString()
        if (-not $text.Contains("duplicate section ")) {
            Write-Host $text
        }
    }

    return $exitCode
}

function Invoke-OptionalStrip {
    param([string]$Target)

    if ($cxxCompiler -ne "g++" -and $cxxCompiler -notlike "*\\g++.exe" -and $cxxCompiler -notlike "*/g++") {
        return
    }

    $stripTool = $null
    $gnuStrip = Get-Command strip -ErrorAction SilentlyContinue
    if ($gnuStrip) {
        $stripTool = $gnuStrip.Source
    }

    if ($stripTool -and (Test-Path $Target)) {
        & $stripTool "--strip-all" $Target
    }
}

$raylibSources = @(
    "third_party/raylib/src/rcore.c",
    "third_party/raylib/src/rshapes.c",
    "third_party/raylib/src/rtextures.c",
    "third_party/raylib/src/rtext.c",
    "third_party/raylib/src/rmodels.c",
    "third_party/raylib/src/raudio.c",
    "third_party/raylib/src/utils.c",
    "third_party/raylib/src/rglfw.c"
)

$raylibObjects = @()
$raylibCFlags = @(
    "-Wall",
    "-D_GNU_SOURCE",
    "-DPLATFORM_DESKTOP_GLFW",
    "-DGRAPHICS_API_OPENGL_33",
    "-Wno-missing-braces",
    "-Werror=pointer-arith",
    "-fno-strict-aliasing",
    "-std=c99",
    "-O2",
    "-Werror=implicit-function-declaration",
    "-Ithird_party/raylib/src",
    "-Ithird_party/raylib/src/external/glfw/include"
)

$raylibCFlags += @(
    "-DNDEBUG",
    "-fvisibility=hidden",
    "-ffunction-sections",
    "-fdata-sections",
    "-fstack-protector-strong",
    "-fno-ident"
)

if ($supportsLto) {
    $raylibCFlags += "-flto"
}

foreach ($source in $raylibSources) {
    $object = Join-Path $raylibObjectRoot (([System.IO.Path]::GetFileNameWithoutExtension($source)) + ".o")
    $raylibObjects += $object

    & $cCompiler @raylibCFlags "-c" $source "-o" $object
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$resourceObject = $null
if (Get-Command windres -ErrorAction SilentlyContinue) {
    if (Test-Path "scripts/logo_converter.py") {
        & python scripts/logo_converter.py
    }
    $resourceObject = "src/resources.o"
    & windres src/resources.rc -o $resourceObject
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "Compiled Win32 resource file: $resourceObject"
}

$moduleSources = @(
    "src/modules/web_module.cpp",
    "src/modules/app_module.cpp",
    "src/modules/ui_module.cpp",
    "src/modules/engine_module.cpp"
)

$linkArgs = @(
    "src/main.cpp"
)
$linkArgs += $moduleSources
$linkArgs += @(
    "-Iinclude/rayquiro",
    "-Ithird_party/raylib/src",
    "-std=c++17"
)

$linkArgs += $commonNativeCompileFlags
$linkArgs += $commonNativeLinkFlags

$linkArgs += $raylibObjects
if ($resourceObject) {
    $linkArgs += $resourceObject
}
$linkArgs += @(
    "-o", $output
)
$linkArgs += $platformLibs

$exitCode = Invoke-Compiler $cxxCompiler $linkArgs
if ($exitCode -ne 0) {
    exit $exitCode
}

Invoke-OptionalStrip $output

if ($isWindows) {
    try {
        Copy-Item $output "rqio.exe" -Force
        Remove-Item $output -Force -ErrorAction SilentlyContinue
        Write-Host "Updated rqio.exe"
    } catch {
        Write-Warning "Built $output, but rqio.exe is locked. Close running rqio.exe processes and copy $output over rqio.exe."
    }
} else {
    Write-Host "Built $output"
}

$coreArgs = @(
    "src/rqio_core.cpp"
)
$coreArgs += $moduleSources
$coreArgs += @(
    "-Iinclude/rayquiro",
    "-Ithird_party/raylib/src",
    "-std=c++17",
    $sharedFlag
)

$coreArgs += $commonNativeCompileFlags
$coreArgs += $commonNativeLinkFlags

$coreArgs += $raylibObjects
$coreArgs += @("-o", $coreOutput)
$coreArgs += $platformLibs

$exitCode = Invoke-Compiler $cxxCompiler $coreArgs
if ($exitCode -ne 0) {
    exit $exitCode
}

Invoke-OptionalStrip $coreOutput

Write-Host "Built $coreOutput"

