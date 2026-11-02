#!/bin/bash

# Exit on any error or pipeline failure
set -eo pipefail

# ==========================================
# Argument Parsing
# ==========================================
if [ -z "$1" ]; then
    echo "[!] Error: No device specified."
    echo "Usage: $0 <device_name> [ksu] [aosp]"
    echo "Example: $0 alioth ksu aosp"
    exit 1
fi

DEVICE_NAME="$1"
DEFCONFIG="${DEVICE_NAME}_defconfig"
DEFCONFIG_PATH="arch/arm64/configs/${DEFCONFIG}"

PIXELOS_CONFIG="arch/arm64/configs/vendor/xiaomi/${DEVICE_NAME}.config"
if [ ! -f "$DEFCONFIG_PATH" ] && [ ! -f "$PIXELOS_CONFIG" ]; then
    echo "[!] Error: Neither $DEFCONFIG_PATH nor $PIXELOS_CONFIG found."
    echo "[!] Please verify the device name and try again."
    exit 1
fi

ENABLE_KSU=0
TARGET_OS="aosp"

shift
for arg in "$@"; do
    case "$arg" in
        ksu) ENABLE_KSU=1 ;;
        aosp) TARGET_OS="aosp" ;;
    esac
done

# ==========================================
# Configuration & Environment
# ==========================================
KERNEL_DIR="$(pwd)"
TOOLCHAIN_BIN="$HOME/zyc-clang/bin"

export PATH="${TOOLCHAIN_BIN}:${PATH}"
export ARCH="arm64"
export SUBARCH="arm64"

# Standardized OEM / Google AOSP Kernel Build Identity
export KBUILD_BUILD_USER="android-build"
export KBUILD_BUILD_HOST="google.com"
export KBUILD_BUILD_TIMESTAMP="Tue Sep 22 16:57:36 CEST 2026"

# ccache Setup
export CCACHE_DIR="$HOME/.cache/ccache_mikernel"
export CCACHE_EXEC=$(command -v ccache || true)

if [ -n "$CCACHE_EXEC" ]; then
    export USE_CCACHE=1
    echo "[*] Setting up ccache in $CCACHE_DIR..."
    mkdir -p "$CCACHE_DIR"
fi

export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_ARM32="arm-linux-gnueabi-"

echo "[*] Checking Clang version..."
clang --version || { echo "[!] Clang not found at ${TOOLCHAIN_BIN}. Please check the path."; exit 1; }

# ==========================================
# BakaSU & SuSFS Setup
# ==========================================
if [ "$ENABLE_KSU" -eq 1 ]; then
    echo "==========================================="
    echo " [*] Initializing BakaSU & SuSFS Setup"
    echo "==========================================="

    # 1. Apply SuSFS 4.19 Kernel Patch
    if [ ! -f "fs/susfs.c" ]; then
        echo "[*] Initializing SuSFS 4.19 Kernel Patch..."
        mkdir -p patches
        SUSFS_PATCH_FILE="patches/susfs_patch_to_4.19.patch"
        SUSFS_PATCH_URL="https://raw.githubusercontent.com/JackA1ltman/NonGKI_Kernel_Build_2nd/mainline/Patches/Patch/susfs_patch_to_4.19.patch"

        TMP_PATCH=$(mktemp)
        echo "[*] Attempting to download latest SuSFS patch from upstream..."
        if curl -LSs --connect-timeout 10 --max-time 30 -o "$TMP_PATCH" "$SUSFS_PATCH_URL" && [ -s "$TMP_PATCH" ]; then
            echo "[+] Successfully downloaded SuSFS patch from upstream. Updating local patch file..."
            mv "$TMP_PATCH" "$SUSFS_PATCH_FILE"
        else
            echo "[-] Failed to download SuSFS patch from upstream. Checking for local fallback patch..."
            rm -f "$TMP_PATCH"
            if [ ! -f "$SUSFS_PATCH_FILE" ]; then
                echo "[!] Error: Remote download failed and local fallback patch ($SUSFS_PATCH_FILE) not found!"
                exit 1
            fi
            echo "[*] Using existing local fallback patch: ${SUSFS_PATCH_FILE}"
        fi

        echo "[*] Applying SuSFS patch (${SUSFS_PATCH_FILE})..."
        patch -p1 --forward < "$SUSFS_PATCH_FILE"
        echo "[+] SuSFS kernel patch applied successfully."
    else
        echo "[*] SuSFS kernel code already present."
    fi

    # 2. Setup BakaSU
    echo "[*] Downloading BakaSU setup script..."
    KSU_SETUP_TMP=$(mktemp)
    if curl -LSs -o "$KSU_SETUP_TMP" "https://raw.githubusercontent.com/Baka-SU/BakaSU/main/kernel/setup.sh" && [ -s "$KSU_SETUP_TMP" ]; then
        echo "[+] BakaSU script retrieved successfully."
        bash "$KSU_SETUP_TMP" main
        rm -f "$KSU_SETUP_TMP"

        # Compatibility adjustments for SuSFS
        if [ -f "KernelSU/kernel/tools/susfs_compat.mk" ]; then
            sed -i '/Please wait susfs update/d' KernelSU/kernel/tools/susfs_compat.mk
        fi
    else
        echo "[-] Failed to retrieve valid BakaSU setup script!"
        rm -f "$KSU_SETUP_TMP"
        exit 1
    fi
    echo "[+] BakaSU & SuSFS setup finished."
fi

# ==========================================
# Build Compilation
# ==========================================
OUT_DIR="out_${TARGET_OS}"
mkdir -p "${OUT_DIR}"

MAKE_OPTS=(
    -j$(nproc --all)
    O="${OUT_DIR}"
    ARCH="${ARCH}"
    SUBARCH="${SUBARCH}"
    CC="clang"
    CROSS_COMPILE="${CROSS_COMPILE}"
    CROSS_COMPILE_ARM32="${CROSS_COMPILE_ARM32}"
    CLANG_TRIPLE="aarch64-linux-gnu-"
)

if [ "$USE_CCACHE" = "1" ]; then
    MAKE_OPTS+=(CC="ccache clang")
fi

echo "==========================================="
echo " [*] Configuring defconfig for ${DEVICE_NAME}"
echo "==========================================="
if [ -f "arch/arm64/configs/vendor/xiaomi/${DEVICE_NAME}.config" ]; then
    echo "[*] Generating PixelOS modular defconfig (kona-perf + sm8250-common + ${DEVICE_NAME})..."
    make "${MAKE_OPTS[@]}" vendor/kona-perf_defconfig vendor/xiaomi/sm8250-common.config "vendor/xiaomi/${DEVICE_NAME}.config"
else
    echo "[*] Generating defconfig: ${DEFCONFIG}..."
    make "${MAKE_OPTS[@]}" "${DEFCONFIG}"
fi

# ==========================================
# BakaSU & SuSFS Kernel Configurations
# ==========================================
if [ "$ENABLE_KSU" -eq 1 ]; then
    echo "[*] Injecting BakaSU & SuSFS configurations..."
    scripts/config --file "${OUT_DIR}/.config" \
        -e KSU \
        -e KSU_SUSFS

    make "${MAKE_OPTS[@]}" olddefconfig
fi

echo "==========================================="
echo " [*] Starting Kernel Compilation"
echo "==========================================="
make "${MAKE_OPTS[@]}" Image

if [ ! -f "${OUT_DIR}/arch/arm64/boot/Image" ]; then
    echo "[-] Kernel compilation failed: Image not found!"
    exit 1
fi
echo "[+] Kernel compilation successful!"

# ==========================================
# AnyKernel3 Packaging
# ==========================================
echo "==========================================="
echo " [*] Packaging into Flashable AnyKernel3 ZIP"
echo "==========================================="
ANYKERNEL_DIR="anykernel"
rm -rf "${ANYKERNEL_DIR}"
git clone https://github.com/AstideLabs/AnyKernel3 -b kona --single-branch --depth=1 "${ANYKERNEL_DIR}"

cp "${OUT_DIR}/arch/arm64/boot/Image" "${ANYKERNEL_DIR}/Image"

cd "${ANYKERNEL_DIR}"
sed -i "s/device.name1=.*/device.name1=${DEVICE_NAME}/" anykernel.sh

KSU_TAG="NoKSU"
if [ "$ENABLE_KSU" -eq 1 ]; then
    KSU_TAG="BakaSU-SuSFS"
fi

ZIP_NAME="Kernel-${DEVICE_NAME}-${TARGET_OS}-${KSU_TAG}-$(date +%Y%m%d_%H%M%S)"
zip -r9 "../${ZIP_NAME}.zip" * -x .git README.md

cd ..
rm -rf "${ANYKERNEL_DIR}"

echo "==========================================="
echo "[+] Flashable ZIP created: ${ZIP_NAME}.zip"
echo "==========================================="
