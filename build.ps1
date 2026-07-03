$cxxCompiler = "clang++"
if (-not (Get-Command $cxxCompiler -ErrorAction SilentlyContinue)) {
    $cxxCompiler = "g++"
}

$cCompiler = "clang"
if (-not (Get-Command $cCompiler -ErrorAction SilentlyContinue)) {
    $cCompiler = "gcc"
}

$vulkanSdkBin = $null
if ($env:VULKAN_SDK) {
    $candidate = Join-Path $env:VULKAN_SDK "Bin"
    if (Test-Path $candidate) {
        $vulkanSdkBin = $candidate
    }
}
if (-not $vulkanSdkBin -and (Test-Path "G:\VulkanSDK\Bin")) {
    $vulkanSdkBin = "G:\VulkanSDK\Bin"
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

$shaderSourceRoot = "assets/vulkan"
$shaderOutputs = @(
    @{ Source = (Join-Path $shaderSourceRoot "preview.vert"); Output = (Join-Path $shaderSourceRoot "preview.vert.spv"); Stage = "vertex" },
    @{ Source = (Join-Path $shaderSourceRoot "preview.frag"); Output = (Join-Path $shaderSourceRoot "preview.frag.spv"); Stage = "fragment" }
)

if ($glslc) {
    foreach ($shader in $shaderOutputs) {
        & $glslc $shader.Source "-o" $shader.Output
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    Write-Host "Compiled Vulkan preview shaders with $glslc"
} else {
    Write-Warning "glslc.exe was not found. Vulkan graphics pipeline will stay disabled until preview shaders are compiled."
}

$output = "rqio-next.exe"
$raylibObjectRoot = Join-Path ".cache" ("rqio-raylib-" + [DateTime]::UtcNow.ToString("yyyyMMddHHmmssfff"))
New-Item -ItemType Directory -Force -Path $raylibObjectRoot | Out-Null

$supportsLto = $false
if ($cxxCompiler -eq "g++" -or $cxxCompiler -like "*\\g++.exe" -or $cxxCompiler -like "*/g++") {
    $supportsLto = $true
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

$linkArgs = @(
    "src/main.cpp",
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
    "-o", $output,
    "-lopengl32",
    "-lgdi32",
    "-lwinmm",
    "-lws2_32",
    "-ladvapi32",
    "-luser32"
)

$exitCode = Invoke-Compiler $cxxCompiler $linkArgs
if ($exitCode -ne 0) {
    exit $exitCode
}

Invoke-OptionalStrip $output

try {
    Copy-Item $output "rqio.exe" -Force
    Write-Host "Updated rqio.exe"
} catch {
    Write-Warning "Built $output, but rqio.exe is locked. Close running rqio.exe processes and copy $output over rqio.exe."
}

$coreOutput = "rqio_core.dll"
$coreArgs = @(
    "src/rqio_core.cpp",
    "-Iinclude/rayquiro",
    "-Ithird_party/raylib/src",
    "-std=c++17",
    "-shared"
)

$coreArgs += $commonNativeCompileFlags
$coreArgs += $commonNativeLinkFlags

$coreArgs += $raylibObjects
$coreArgs += @(
    "-o", $coreOutput,
    "-lopengl32",
    "-lgdi32",
    "-lwinmm",
    "-lws2_32",
    "-ladvapi32",
    "-luser32"
)

$exitCode = Invoke-Compiler $cxxCompiler $coreArgs
if ($exitCode -ne 0) {
    exit $exitCode
}

Invoke-OptionalStrip $coreOutput

Write-Host "Built $coreOutput"

if (Test-Path "native_modules") {
    $modulesDir = "modules"
    New-Item -ItemType Directory -Force -Path $modulesDir | Out-Null

    $webModuleOutput = Join-Path $modulesDir "web.dll"
    $webModuleArgs = @(
        "native_modules/web_module.cpp",
        "-Iinclude/rayquiro",
        "-std=c++17",
        "-shared"
    )

    $webModuleArgs += $commonNativeCompileFlags
    $webModuleArgs += $commonNativeLinkFlags
    $webModuleArgs += @(
        "-o", $webModuleOutput,
        "-lws2_32"
    )

    & $cxxCompiler @webModuleArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Invoke-OptionalStrip $webModuleOutput

    Write-Host "Built $webModuleOutput"

    $appModuleOutput = Join-Path $modulesDir "app.dll"
    $appModuleArgs = @(
        "native_modules/app_module.cpp",
        "-Iinclude/rayquiro",
        "-std=c++17",
        "-shared"
    )

    $appModuleArgs += $commonNativeCompileFlags
    $appModuleArgs += $commonNativeLinkFlags
    $appModuleArgs += @(
        "-o", $appModuleOutput,
        "-luser32",
        "-lgdi32"
    )

    & $cxxCompiler @appModuleArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Invoke-OptionalStrip $appModuleOutput

    Write-Host "Built $appModuleOutput"

    $uiModuleOutput = Join-Path $modulesDir "ui.dll"
    $uiModuleArgs = @(
        "native_modules/ui_module.cpp",
        "-Iinclude/rayquiro",
        "-std=c++17",
        "-shared"
    )

    $uiModuleArgs += $commonNativeCompileFlags
    $uiModuleArgs += $commonNativeLinkFlags
    $uiModuleArgs += @(
        "-o", $uiModuleOutput,
        "-luser32",
        "-lgdi32"
    )

    & $cxxCompiler @uiModuleArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Invoke-OptionalStrip $uiModuleOutput

    Write-Host "Built $uiModuleOutput"

    $engineModuleOutput = Join-Path $modulesDir "engine.dll"
    $engineModuleArgs = @(
        "native_modules/engine_module.cpp",
        "-Iinclude/rayquiro",
        "-Ithird_party/raylib/src",
        "-std=c++17",
        "-shared"
    )

    $engineModuleArgs += $commonNativeCompileFlags
    $engineModuleArgs += $commonNativeLinkFlags
    $engineModuleArgs += @(
        "-o", $engineModuleOutput
    )

    $engineModuleArgs += $raylibObjects
    $engineModuleArgs += @(
        "-lopengl32",
        "-lgdi32",
        "-lwinmm",
        "-lws2_32",
        "-ladvapi32",
        "-luser32"
    )

    & $cxxCompiler @engineModuleArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Invoke-OptionalStrip $engineModuleOutput

    Write-Host "Built $engineModuleOutput"
} else {
    Write-Host "native_modules directory not found, skipping building native modules. Prebuilt modules in in-all/modules/ will be used if needed."
}

