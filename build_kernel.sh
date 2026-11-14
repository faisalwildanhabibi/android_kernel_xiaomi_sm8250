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
# SukiSU-Ultra, SuSFS & NoMount Setup
# ==========================================
if [ "$ENABLE_KSU" -eq 1 ]; then
    echo "==========================================="
    echo " [*] Initializing SukiSU-Ultra, SuSFS & NoMount Setup"
    echo "==========================================="

    # 1. Setup SukiSU-Ultra
    echo "[*] Downloading SukiSU-Ultra setup script..."
    KSU_SETUP_TMP=$(mktemp)
    if curl -LSs -o "$KSU_SETUP_TMP" "https://raw.githubusercontent.com/sukisu-ultra/sukisu-ultra/main/kernel/setup.sh" && [ -s "$KSU_SETUP_TMP" ]; then
        echo "[+] SukiSU-Ultra script retrieved successfully."
        bash "$KSU_SETUP_TMP" main
        rm -f "$KSU_SETUP_TMP"

        # Compatibility adjustments for SuSFS
        if [ -f "KernelSU/kernel/tools/susfs_compat.mk" ]; then
            sed -i '/Please wait susfs update/d' KernelSU/kernel/tools/susfs_compat.mk
        fi

        # Compatibility adjustments for SukiSU-Ultra on Linux 4.19 (missing MODULE_IMPORT_NS)
        if [ -f "KernelSU/kernel/core/init.c" ]; then
            sed -i '1i #ifndef MODULE_IMPORT_NS\n#define MODULE_IMPORT_NS(ns)\n#endif' KernelSU/kernel/core/init.c
        fi
    else
        echo "[-] Failed to retrieve valid SukiSU-Ultra setup script!"
        rm -f "$KSU_SETUP_TMP"
        exit 1
    fi

    # 2. Apply SuSFS 4.19 Kernel Patch
    if [ ! -f "fs/susfs.c" ]; then
        echo "[*] Initializing SuSFS 4.19 Kernel Patch..."
        SUSFS_PATCH_FILE="patches/susfs_patch_to_4.19.patch"
        if [ ! -f "$SUSFS_PATCH_FILE" ]; then
            echo "[!] Error: Repository SuSFS patch ($SUSFS_PATCH_FILE) not found!"
            exit 1
        fi

        echo "[*] Applying SuSFS patch (${SUSFS_PATCH_FILE})..."
        patch -p1 --forward < "$SUSFS_PATCH_FILE"
        echo "[+] SuSFS kernel patch applied successfully."
    else
        echo "[*] SuSFS kernel code already present."
    fi

    # 3. Setup NoMount
    if [ ! -d "NoMount" ]; then
        echo "[*] Downloading NoMount setup script..."
        NOMOUNT_SETUP_TMP=$(mktemp)
        if curl -LSs -o "$NOMOUNT_SETUP_TMP" "https://raw.githubusercontent.com/maxsteeel/nomount/master/kernel/setup.sh" && [ -s "$NOMOUNT_SETUP_TMP" ]; then
            echo "[+] NoMount script retrieved successfully."
            sh "$NOMOUNT_SETUP_TMP" master
            rm -f "$NOMOUNT_SETUP_TMP"
        else
            echo "[-] Failed to retrieve valid NoMount setup script!"
            rm -f "$NOMOUNT_SETUP_TMP"
            exit 1
        fi
    else
        echo "[*] NoMount code already present."
    fi

    # 4. Verify WireGuard Driver
    if [ -d "drivers/net/wireguard" ]; then
        echo "[+] Native in-tree WireGuard driver present in drivers/net/wireguard."
    else
        echo "[*] In-tree WireGuard driver missing, fetching wireguard-linux-compat..."
        WG_TMP=$(mktemp -d)
        if git clone https://github.com/WireGuard/wireguard-linux-compat.git --depth=1 "$WG_TMP"; then
            echo "[+] Copying WireGuard source to drivers/net/wireguard..."
            cp -r "$WG_TMP/src" "drivers/net/wireguard"
            echo "[+] WireGuard driver populated successfully."
        else
            echo "[-] Failed to clone WireGuard repository!"
        fi
        rm -rf "$WG_TMP"
    fi

    echo "[+] KernelSU, SuSFS, NoMount & WireGuard setup finished."
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
# Feature, Performance & Stealth Configurations
# ==========================================
echo "[*] Injecting Performance & Stealth configurations..."
scripts/config --file "${OUT_DIR}/.config" \
    -e TCP_CONG_ADVANCED \
    -e TCP_CONG_BBR \
    -e DEFAULT_BBR \
    --set-str DEFAULT_TCP_CONG "bbr" \
    -e ZRAM_WRITEBACK \
    -e MQ_IOSCHED_DEADLINE \
    -e MQ_IOSCHED_KYBER \
    -d SECURITY_SELINUX_DEVELOP

if [ "$ENABLE_KSU" -eq 1 ]; then
    echo "[*] Injecting SukiSU-Ultra, SuSFS & NoMount configurations..."
    scripts/config --file "${OUT_DIR}/.config" \
        -e KSU \
        -e KSU_SUSFS \
        -e NOMOUNT \
        -e KPROBES \
        -e KPROBE_EVENTS
fi

make "${MAKE_OPTS[@]}" olddefconfig

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
git clone https://github.com/osm0sis/AnyKernel3 -b master --single-branch --depth=1 "${ANYKERNEL_DIR}"

cp "${OUT_DIR}/arch/arm64/boot/Image" "${ANYKERNEL_DIR}/Image"

cd "${ANYKERNEL_DIR}"
rm -rf kernels dtb dtbo.img modules

cat << 'EOF' > anykernel.sh
### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=Kernel for Poco F3 (alioth)
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=alioth
device.name2=aliothin
device.name3=alioth_global
device.name4=M2012K11AG
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=auto;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# boot install
split_boot;

flash_boot;
## end boot install
EOF

sed -i "s/device.name1=.*/device.name1=${DEVICE_NAME}/" anykernel.sh

KSU_TAG="NoKSU"
if [ "$ENABLE_KSU" -eq 1 ]; then
    KSU_TAG="SukiSU-SuSFS-NoMount-WG"
fi

ZIP_NAME="Kernel-${DEVICE_NAME}-${TARGET_OS}-${KSU_TAG}-$(date +%Y%m%d_%H%M%S)"
zip -r9 "../${ZIP_NAME}.zip" * -x .git README.md

cd ..
rm -rf "${ANYKERNEL_DIR}"

echo "==========================================="
echo "[+] Flashable ZIP created: ${ZIP_NAME}.zip"
echo "==========================================="
