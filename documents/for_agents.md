# For Coding Agents

このファイルは、会話コンテキストが消えた後でも、このリポジトリで作業を再開しやすくするためのメモ。

## Project

- Repository root: `C:/Users/user/source/repos/aruzen/AHO`
- Main language: C++23
- Main components:
  - `VSLi/VSL`: lower-level Vulkan support library
  - `AHOLi/AHO`: AHO library built on VSL
  - `AHO`: executable/application side
  - `documents`: design notes and agent-facing reports

## Windows Toolchain

Windows builds are intended to use MSVC, not MinGW.

Known working compiler:

```text
C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe
```

Known compiler identification:

```text
MSVC 19.51.36252.0
```

The compiler is selected by the Visual Studio generator. Do not hardcode an MSVC installation or compiler version in `CMakeLists.txt`.

Known working Visual Studio generator:

```text
Visual Studio 18 2026
```

If CLion says the CMake compiler is MSVC but the toolchain is MinGW, switch the CLion toolchain/profile to Visual Studio. Do not try to make the project build with CLion bundled MinGW unless the user explicitly asks for that path.

The CLion bundled CMake may not know `Visual Studio 18 2026`. Prefer system CMake from:

```text
C:/Program Files/CMake/bin/cmake.exe
```

## CMake

Use this preset on Windows:

```powershell
cmake --preset windows-vs2026-debug
```

Or explicitly:

```powershell
cmake --fresh -S . -B build/vs2026 -G "Visual Studio 18 2026" -A x64 -T v145
```

The Windows preset uses `${sourceDir}` for `AHO_HOME` and `VSL_HOME`. Without a preset, CMake accepts these environment variables and otherwise defaults to the libraries in this checkout.

Set the dependency roots before configuring (the paths below are examples for the current machine):

```powershell
$env:VULKAN_SDK = 'C:\VulkanSDK\1.4.304.0'
$env:GLFW_ROOT = 'D:\CLibrary\glfw-3.3.8.bin.WIN64'
```

`-DVULKAN_SDK=...` and `-DGLFW_ROOT=...` can also be used. The legacy `GLFW` environment variable is accepted when `GLFW_ROOT` is not set. GLFW's `lib-vc2022` directory is preferred, with `lib-vc2019` as a fallback. The Vulkan SDK must include GLM and the Debug/Release SPIRV-Cross libraries used by VSL. Use x64 dependencies with the x64 preset.

The checked-in `AHO.sln` / `.vcxproj` build uses the same dependency roots through `Windows.props`. Project references build and link AHO's libraries automatically. Both native projects and CMake use C++23 features, the conforming MSVC preprocessor, and UTF-8 source encoding.

Windows builds use `glfw3dll.lib` by default and copy `glfw3.dll` beside each executable, including the CMake test binaries. This avoids linking the prebuilt Release GLFW static library's CRT into Debug applications. To use a static GLFW built with a compatible CRT, configure with `-DAHO_GLFW_SHARED=OFF` or build the native project with `/p:AHO_GLFW_SHARED=false`.

In Visual Studio, select AHO as the startup project. Both build routes set its debugger working directory to `AHO/` and provide `AHO_HOME` and the Vulkan SDK tools on `PATH`. When launching an executable directly, set `AHO_HOME` to this checkout's `AHOLi/AHO`, add the SDK's `Bin` directory to `PATH`, and launch from `AHO/` so that relative shader paths resolve.

Linux/macOS compatibility matters. Do not remove the non-Windows `find_package` / `pkg_check_modules` path unless the user explicitly asks for a Windows-only setup.

## Windows MSBuild Caveats

In the Codex sandbox, MSBuild can fail with:

```text
FileTracker ... E_ACCESSDENIED
```

This is a sandbox permission issue, not necessarily a project build error. Re-run the same build with escalated permissions if verification requires MSBuild.

MSBuild can also fail if both `Path` and `PATH` exist in the environment. Use a clean PowerShell environment when running CMake/MSBuild:

```powershell
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = 'C:\Program Files\CMake\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0;C:\VulkanSDK\1.4.304.0\Bin'
```

## Tests

GoogleTest is used for tests.

GoogleTest is intentionally placed locally and ignored by git:

```text
thirdparty/googletest/
```

The source was cloned from the official GoogleTest repository at tag `v1.17.0`.

Tests live under each component:

```text
AHOLi/AHO/test
VSLi/VSL/test
```

Root `Test/` is no longer the active test location.

Current targets:

```text
aho-test
vsl-test
```

Build tests:

```powershell
cmake --build --preset windows-vs2026-tests
```

Run tests:

```powershell
ctest --preset windows-vs2026-debug
```

Equivalent direct command:

```powershell
ctest --test-dir build/vs2026 -C Debug --output-on-failure
```

Build and test Release:

```powershell
cmake --build --preset windows-vs2026-release
ctest --preset windows-vs2026-release
```

Last verified on 2026-10-28 with MSVC x64: CMake and the native Visual Studio solution build in Debug and Release. CMake tests pass in both configurations:

```text
41 tests passed, 0 failed, 4 disabled
```

The Debug applications from both build routes were also launched with copied test resources and closed normally (exit code 0). The Vulkan SDK's missing SPIRV-Cross PDB warning (`LNK4099`) and existing numeric-conversion warnings remain; the GLFW CRT conflict (`LNK4098`) is resolved by the DLL build.

When testing VSL, do not delete tests just because they expose a current bug. If a case documents an expected failure, unsafe call order, missing state guard, or Vulkan lifecycle issue, keep it as a GoogleTest disabled test by prefixing the test or suite name with `DISABLED_`.

Example:

```cpp
TEST(VSLPhase, DISABLED_SubmitBeforeSetupShouldNotCrash)
{
    // Documents a known broken order-sensitive case.
}
```

VSL is order-sensitive. Tests should intentionally cover invalid or surprising call orders, not only happy paths. If the test cannot run reliably without a real Vulkan device/window, keep a small disabled regression test or a clearly named integration test scaffold in `VSLi/VSL/test`.

## Codex Working Space

Use these directories for temporary experiments proposed by Codex:

```text
AHOLi/AHO/test/codex-working-space
VSLi/VSL/test/codex-working-space
```

Experimental tests or scratch validation files should start there. Move them into the parent `test/` directory only after they are stable and should become part of the regular GTest suite.

## Existing Commits From Recent Agent Work

Recent relevant commits:

```text
44697ac Add Windows MSVC CMake preset
ff6a4cc Document implementation gaps
1f8c30b Add AHO GoogleTest coverage
7ffe69a Include cinttypes in VSL defines
```

## Known AHO Fixes Already Made

The test work fixed several real issues:

- `angle.hpp`: added missing `<compare>`
- `color.hpp`: added missing includes and fixed RGB/RGBA scalar operations
- `color.hpp`: added constructors from component wrapper types such as `R<T>`, `G<T>`, `B<T>`
- `mat.hpp`: fixed initializer-list constructor so the column index resets per row
- `resource/buffer.hpp`: made `Buffer<General>` RAII and move-only, with initialized size
- `resource/buffer.cpp`: removed explicit instantiation for unfinished `GPUFriendly` and `OnGPU`
- `input_manager.cpp`: fixed iterator invalidation when removing expired weak pointers

## Known Unimplemented Or Risky Areas

See:

```text
documents/未実装箇所レポート.md
```

High priority areas from that report:

- `AHOLi/AHO/include/AHO/resource/buffer.hpp`: `GPUFriendly` and `OnGPU` are still unfinished
- `AHOLi/AHO/include/AHO/pipeline/pipeline.hpp`: typed-safe pipeline design is incomplete
- `VSLi/VSL/src/vulkan/phase.cpp`: acquire/present image index handling needs review
- `VSLi/VSL/src/window.cpp` and `VSLi/VSL/src/vulkan/surface.cpp`: initialization failure currently leaves partially constructed objects

## Git Hygiene

The working tree may contain user or generated changes unrelated to the current task. Do not revert or stage unrelated changes.

Use conventional prefix-style commit messages going forward. Preferred prefixes:

```text
feat: add or expose a user-facing feature
fix: fix a bug or broken behavior
test: add or update tests only
docs: update documents only
chore: build, tooling, repository maintenance, or non-runtime changes
refactor: restructure code without intended behavior change
```

Examples:

```text
docs: add coding agent notes
test: add AHO core coverage
fix: handle expired input manager targets
chore: update CMake test presets
```

Known unrelated dirty files seen recently:

```text
AHOLi/AHO/built-in-resource/shaders/raw/vd2p_fc_umpv.vert
VSLi/VSLi.vcxproj.filters
AHO/shaders/3Dnormal.vert.spv
AHOLi/AHO/built-in-resource/shaders/*.spv
```

Use:

```powershell
git -c safe.directory=C:/Users/user/source/repos/aruzen/AHO status --short
```

when git complains about dubious ownership under the Codex sandbox user.

## Agent Behavior Notes

- Prefer `rg` / `rg --files` for searching.
- Use `apply_patch` for manual edits.
- Keep Linux/macOS compatibility unless the user explicitly requests Windows-only behavior.
- For Windows builds, avoid MinGW unless explicitly requested.
- For Vulkan/window/input tests that need real GLFW windows, treat them as integration tests rather than ordinary unit tests.
