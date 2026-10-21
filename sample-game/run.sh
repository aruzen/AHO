#!/usr/bin/env bash
set -euo pipefail
sample_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sample_build="${AHO_WANDER_BUILD_DIR:-$sample_root/build/wander}"
sample_options=()

# Prefer the installed SDK; allow normal FindVulkan discovery on other systems.
if [[ -z "${VULKAN_SDK:-}" && -d "$HOME/VulkanSDK" ]]; then
    sample_sdk="$(find "$HOME/VulkanSDK" -maxdepth 2 -type d -name macOS | sort | tail -1)"
    if [[ -n "$sample_sdk" ]]; then export VULKAN_SDK="$sample_sdk"; fi
fi
if [[ -n "${VULKAN_SDK:-}" ]]; then
    export PATH="$VULKAN_SDK/bin:$PATH"
    if [[ "$(uname -s)" == Darwin ]]; then
        sample_options+=("-DVulkan_INCLUDE_DIR=$VULKAN_SDK/include" "-DVulkan_LIBRARY=$VULKAN_SDK/lib/libvulkan.dylib")
    fi
    if [[ -f "$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json" ]]; then
        export VK_ICD_FILENAMES="${VK_ICD_FILENAMES:-$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json}"
    fi
    if [[ -d "$VULKAN_SDK/share/vulkan/explicit_layer.d" ]]; then
        export VK_LAYER_PATH="${VK_LAYER_PATH:-$VULKAN_SDK/share/vulkan/explicit_layer.d}"
    fi
fi
# This project uses C++23; Apple's default compiler may lack the required library support.
if [[ ! -f "$sample_build/CMakeCache.txt" && -x /opt/homebrew/opt/llvm/bin/clang++ ]]; then
    sample_options+=("-DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++" "-DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang")
fi
cmake -S "$sample_root" -B "$sample_build" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON -DAHO_BUILD_SAMPLE_GAME=ON \
    "-DAHO_HOME=$sample_root/AHOLi/AHO" "-DVSL_HOME=$sample_root/VSLi/VSL" "${sample_options[@]}"
cmake --build "$sample_build" --target aho-wander -j "${AHO_BUILD_JOBS:-6}"
cd "$sample_root"
exec "$sample_build/sample-game/aho-wander" "$@"
