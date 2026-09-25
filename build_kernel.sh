#!/bin/bash

# Exit on any error
set -e

# ==========================================
# Argument Parsing
# ==========================================
if [ -z "$1" ]; then
    echo "[!] Error: No device specified."
    echo "Usage: $0 <device_name> [ksu] [miui|aosp] [droidspaces]"
    echo "Example: $0 lmi"
    echo "         $0 lmi ksu"
    echo "         $0 lmi ksu miui"
    echo "         $0 lmi aosp"
    echo "         $0 lmi ksu miui droidspaces (DroidSpaces support)"
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
ENABLE_DROIDSPACES=0
TARGET_OS="both"

shift
# Parse remaining arguments loosely
for arg in "$@"; do
    case "$arg" in
        ksu) ENABLE_KSU=1 ;;
        miui) TARGET_OS="miui" ;;
        aosp) TARGET_OS="aosp" ;;
        droidspaces) ENABLE_DROIDSPACES=1 ;;
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

# ccache Setup
export CCACHE_DIR="$HOME/.cache/ccache_mikernel"
export CCACHE_EXEC=$(command -v ccache)

if [ -z "$CCACHE_EXEC" ]; then
    echo "[!] ccache not found! Please install ccache first."
    exit 1
fi

export USE_CCACHE=1
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_ARM32="arm-linux-gnueabi-"

echo "[*] Checking Clang version..."
clang --version || { echo "[!] Clang not found at ${TOOLCHAIN_BIN}. Please check the path."; exit 1; }

echo "[*] Setting up ccache in $CCACHE_DIR..."
mkdir -p "$CCACHE_DIR"

# ==========================================
# KernelSU Setup
# ==========================================
if [ "$ENABLE_KSU" -eq 1 ]; then
    echo "==========================================="
    echo " [*] Initializing KernelSU (ReSukiSU) Setup"
    echo "==========================================="
    echo "[*] Downloading ReSukiSU setup script..."
    KSU_SETUP_TMP=$(mktemp)
    if curl -LSs -o "$KSU_SETUP_TMP" "https://raw.githubusercontent.com/ReSukiSU/ReSukiSU/main/kernel/setup.sh" && [ -s "$KSU_SETUP_TMP" ]; then
        echo "[+] [ISO 27001 / SLSA] ReSukiSU script retrieved successfully (size: $(stat -c%s "$KSU_SETUP_TMP" 2>/dev/null || stat -f%z "$KSU_SETUP_TMP" 2>/dev/null || echo 0) bytes)."
        bash "$KSU_SETUP_TMP"
        rm -f "$KSU_SETUP_TMP"
    else
        echo "[-] [ISO 27001 / SLSA ABORT] Failed to retrieve valid ReSukiSU setup script!"
        rm -f "$KSU_SETUP_TMP"
        exit 1
    fi
    echo "[+] KernelSU setup finished."
fi

# ==========================================
# Baseband-guard Setup (Disabled for Upstream Stability)

# ==========================================
# DroidSpaces Setup
# ==========================================
if [ "$ENABLE_DROIDSPACES" -eq 1 ]; then
    echo "==========================================="
    echo " [*] Initializing DroidSpaces Setup"
    echo "==========================================="

    DS_TMP_DIR=$(mktemp -d)
    DS_BASE_URL="https://raw.githubusercontent.com/ravindu644/Droidspaces-OSS/main/Documentation/resources/kernel-patches/non-GKI"

    echo "[*] Downloading DroidSpaces kernel patches..."
    wget -q -O "$DS_TMP_DIR/01_fix_xt_qtaguid.patch" \
        "$DS_BASE_URL/01.fix_kernel_panic_in_xt_qtaguid.patch"
    wget -q -O "$DS_TMP_DIR/02_fix_cgroup_prefix.patch" \
        "$DS_BASE_URL/02.fix_restore%20cgroup%20file%20prefix%20handling%20.patch"

    cd "$KERNEL_DIR"

    # Dry-run first: skip cleanly when already applied or not applicable
    apply_droidspaces_patch() {
        local patch_file=$1
        if patch -p1 --forward --dry-run < "$patch_file" > /dev/null 2>&1; then
            patch -p1 --forward < "$patch_file"
            echo "[+] Patch applied: $(basename "$patch_file")"
        else
            echo "[-] Patch skipped (already applied or not applicable): $(basename "$patch_file")"
        fi
    }

    # 1) xt_qtaguid panic fix (auto-skipped if net/netfilter/xt_qtaguid.c does not exist)
    apply_droidspaces_patch "$DS_TMP_DIR/01_fix_xt_qtaguid.patch"

    # 2) cgroup file prefix handling fix (required by DroidSpaces)
    apply_droidspaces_patch "$DS_TMP_DIR/02_fix_cgroup_prefix.patch"

    # Generate DroidSpaces config fragment for later merging into out/.config
    DS_CONFIG_FRAG="$DS_TMP_DIR/droidspaces.config"
    cat > "$DS_CONFIG_FRAG" <<'EOF'
# Kernel configurations for full DroidSpaces support
# Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>

# IPC mechanisms
CONFIG_SYSCTL=y
CONFIG_SYSVIPC=y
CONFIG_POSIX_MQUEUE=y

# Core namespace support
CONFIG_NAMESPACES=y
CONFIG_PID_NS=y
CONFIG_UTS_NS=y
CONFIG_IPC_NS=y

# Seccomp support
CONFIG_SECCOMP=y
CONFIG_SECCOMP_FILTER=y

# Control groups support
CONFIG_CGROUPS=y
CONFIG_CGROUP_DEVICE=y
CONFIG_CGROUP_PIDS=y
CONFIG_MEMCG=y
CONFIG_CGROUP_SCHED=y
CONFIG_FAIR_GROUP_SCHED=y
CONFIG_CGROUP_FREEZER=y
CONFIG_CGROUP_NET_PRIO=y

# Device filesystem support
CONFIG_DEVTMPFS=y

# Overlay filesystem support (required for volatile mode)
CONFIG_OVERLAY_FS=y

# Enable xattr, posix acl support on tmpfs
CONFIG_TMPFS_POSIX_ACL=y
CONFIG_TMPFS_XATTR=y

# Firmware loading support
CONFIG_FW_LOADER=y
CONFIG_FW_LOADER_USER_HELPER=y
CONFIG_FW_LOADER_COMPRESS=y

# DroidSpaces Network Isolation Support - NAT/none modes
CONFIG_NET_NS=y
CONFIG_VETH=y
CONFIG_BRIDGE=y
CONFIG_NETFILTER=y
CONFIG_BRIDGE_NETFILTER=y
CONFIG_NETFILTER_ADVANCED=y
CONFIG_NF_CONNTRACK=y
CONFIG_IP_NF_IPTABLES=y
CONFIG_IP_NF_FILTER=y
CONFIG_NF_NAT=y
CONFIG_NF_TABLES=y
CONFIG_IP_NF_TARGET_MASQUERADE=y
CONFIG_NETFILTER_XT_TARGET_MASQUERADE=y
CONFIG_NETFILTER_XT_TARGET_TCPMSS=y
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y
CONFIG_NF_CONNTRACK_NETLINK=y
CONFIG_NF_NAT_REDIRECT=y
CONFIG_IP_ADVANCED_ROUTER=y
CONFIG_IP_MULTIPLE_TABLES=y

# legacy compat (ignored automatically on newer kernels)
CONFIG_NF_CONNTRACK_IPV4=y
CONFIG_NF_NAT_IPV4=y
CONFIG_IP_NF_NAT=y

# Disable this on older kernels to make internet work
CONFIG_ANDROID_PARANOID_NETWORK=n

# Fix for docker unsafe procfs error
CONFIG_USER_NS=y

# UFW & FAIL2BAN CORE
CONFIG_NETFILTER_XT_MATCH_COMMENT=y
CONFIG_NETFILTER_XT_MATCH_STATE=y
CONFIG_NETFILTER_XT_MATCH_CONNTRACK=y
CONFIG_NETFILTER_XT_MATCH_MULTIPORT=y
CONFIG_NETFILTER_XT_MATCH_HL=y
CONFIG_NETFILTER_XT_TARGET_REJECT=y
CONFIG_IP_NF_TARGET_REJECT=y
CONFIG_NETFILTER_XT_TARGET_LOG=y
CONFIG_IP_NF_TARGET_ULOG=y
CONFIG_NETFILTER_XT_MATCH_RECENT=y
CONFIG_NETFILTER_XT_MATCH_LIMIT=y
CONFIG_NETFILTER_XT_MATCH_HASHLIMIT=y
CONFIG_NETFILTER_XT_MATCH_OWNER=y
CONFIG_NETFILTER_XT_MATCH_PKTTYPE=y
CONFIG_NETFILTER_XT_MATCH_MARK=y
CONFIG_NETFILTER_XT_TARGET_MARK=y
CONFIG_IP_SET=y
CONFIG_IP_SET_HASH_IP=y
CONFIG_IP_SET_HASH_NET=y
CONFIG_NETFILTER_XT_SET=y
CONFIG_NETFILTER_NETLINK_QUEUE=y
CONFIG_NETFILTER_NETLINK_LOG=y
CONFIG_NETFILTER_XT_TARGET_NFLOG=y
EOF

    echo "[+] DroidSpaces setup finished."
    echo "==========================================="
fi

# ==========================================
# AnyKernel3 Setup (Pure Kernel Mode)
# ==========================================
echo "==========================================="
echo " [*] Initializing AnyKernel3 Workspace (Pure Kernel Mode)"
echo "==========================================="
rm -rf anykernel
echo "[*] Cloning AnyKernel3..."
git clone https://github.com/AstideLabs/AnyKernel3 -b kona --single-branch --depth=1 anykernel
echo "[+] AnyKernel3 cloned successfully."
echo "[*] Installing Pure Kernel AnyKernel3 Template..."
cp -f anykernel_template.sh anykernel/anykernel.sh
sed -i "s/^device\.name1=.*/device.name1=${DEVICE_NAME}\ndevice.name2=${DEVICE_NAME}in/" anykernel/anykernel.sh
rm -rf anykernel/kernels anykernel/dtb anykernel/dtbo.img anykernel/modules
echo "[*] Pure AnyKernel3 prepared successfully."
echo "==========================================="

# ==========================================
# Modular Build Function
# ==========================================
build_target() {
    local OS_TYPE=$1
    echo "==========================================="
    echo " Starting Kernel Compilation for ${DEVICE_NAME} (Target: $OS_TYPE)"
    echo "==========================================="
    
    local OUT_DIR="${KERNEL_DIR}/out_${OS_TYPE}"
    
    local MAKE_OPTS=(
        -j"$(nproc)"
        O="${OUT_DIR}"
        ARCH="${ARCH}"
        SUBARCH="${SUBARCH}"
        LLVM=1
        LLVM_IAS=1
        CC="ccache clang"
        HOSTCC="ccache clang"
        CROSS_COMPILE="${CROSS_COMPILE}"
        CROSS_COMPILE_ARM32="${CROSS_COMPILE_ARM32}"
    )

    echo "[*] Cleaning ${OUT_DIR}..."
    rm -rf "${OUT_DIR}"
    mkdir -p "${OUT_DIR}"

    local DTS_SOURCE="arch/arm64/boot/dts/vendor/qcom"
    local DTS_BACKUP=".dts.bak.${OS_TYPE}"

    if [ "$OS_TYPE" == "miui" ]; then
        echo "[*] Applying MIUI DTS patches..."
        cp -a "${DTS_SOURCE}" "${DTS_BACKUP}"
        
        # Apply MIUI specific sed patches to dts
        sed -i 's/<154>/<1537>/g' ${DTS_SOURCE}/dsi-panel-j1s* || true
        sed -i 's/<154>/<1537>/g' ${DTS_SOURCE}/dsi-panel-j2* || true
        sed -i 's/<155>/<1544>/g' ${DTS_SOURCE}/dsi-panel-j3s-37-02-0a-dsc-video.dtsi || true
        sed -i 's/<155>/<1545>/g' ${DTS_SOURCE}/dsi-panel-j11-38-08-0a-fhd-cmd.dtsi || true
        sed -i 's/<155>/<1546>/g' ${DTS_SOURCE}/dsi-panel-k11a-38-08-0a-dsc-cmd.dtsi || true
        sed -i 's/<155>/<1546>/g' ${DTS_SOURCE}/dsi-panel-l11r-38-08-0a-dsc-cmd.dtsi || true
        sed -i 's/<70>/<695>/g' ${DTS_SOURCE}/dsi-panel-j11-38-08-0a-fhd-cmd.dtsi || true
        sed -i 's/<70>/<695>/g' ${DTS_SOURCE}/dsi-panel-j3s-37-02-0a-dsc-video.dtsi || true
        sed -i 's/<70>/<695>/g' ${DTS_SOURCE}/dsi-panel-k11a-38-08-0a-dsc-cmd.dtsi || true
        sed -i 's/<70>/<695>/g' ${DTS_SOURCE}/dsi-panel-l11r-38-08-0a-dsc-cmd.dtsi || true
        sed -i 's/<71>/<710>/g' ${DTS_SOURCE}/dsi-panel-j1s* || true
        sed -i 's/<71>/<710>/g' ${DTS_SOURCE}/dsi-panel-j2* || true

        sed -i 's/\/\/ mi,mdss-dsi-pan-enable-smart-fps/mi,mdss-dsi-pan-enable-smart-fps/g' ${DTS_SOURCE}/dsi-panel* || true
        sed -i 's/\/\/ mi,mdss-dsi-smart-fps-max_framerate/mi,mdss-dsi-smart-fps-max_framerate/g' ${DTS_SOURCE}/dsi-panel* || true
        sed -i 's/\/\/ qcom,mdss-dsi-pan-enable-smart-fps/qcom,mdss-dsi-pan-enable-smart-fps/g' ${DTS_SOURCE}/dsi-panel* || true
        sed -i 's/qcom,mdss-dsi-qsync-min-refresh-rate/\/\/qcom,mdss-dsi-qsync-min-refresh-rate/g' ${DTS_SOURCE}/dsi-panel* || true

        sed -i 's/120 90 60/120 90 60 50 30/g' ${DTS_SOURCE}/dsi-panel-g7a-36-02-0c-dsc-video.dtsi || true
        sed -i 's/120 90 60/120 90 60 50 30/g' ${DTS_SOURCE}/dsi-panel-g7a-37-02-0a-dsc-video.dtsi || true
        sed -i 's/120 90 60/120 90 60 50 30/g' ${DTS_SOURCE}/dsi-panel-g7a-37-02-0b-dsc-video.dtsi || true
        sed -i 's/144 120 90 60/144 120 90 60 50 48 30/g' ${DTS_SOURCE}/dsi-panel-j3s-37-02-0a-dsc-video.dtsi || true

        sed -i 's/\/\/39 00 00 00 00 00 03 51 03 FF/39 00 00 00 00 00 03 51 03 FF/g' ${DTS_SOURCE}/dsi-panel-j9-38-0a-0a-fhd-video.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 03 51 0D FF/39 00 00 00 00 00 03 51 0D FF/g' ${DTS_SOURCE}/dsi-panel-j2-p2-1-38-0c-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 05 51 0F 8F 00 00/39 00 00 00 00 00 05 51 0F 8F 00 00/g' ${DTS_SOURCE}/dsi-panel-j1s-42-02-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 05 51 0F 8F 00 00/39 00 00 00 00 00 05 51 0F 8F 00 00/g' ${DTS_SOURCE}/dsi-panel-j1s-42-02-0a-mp-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 05 51 0F 8F 00 00/39 00 00 00 00 00 05 51 0F 8F 00 00/g' ${DTS_SOURCE}/dsi-panel-j2-mp-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 05 51 0F 8F 00 00/39 00 00 00 00 00 05 51 0F 8F 00 00/g' ${DTS_SOURCE}/dsi-panel-j2-p2-1-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 00 00 00 00 00 05 51 0F 8F 00 00/39 00 00 00 00 00 05 51 0F 8F 00 00/g' ${DTS_SOURCE}/dsi-panel-j2s-mp-42-02-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 00 00/39 01 00 00 00 00 03 51 00 00/g' ${DTS_SOURCE}/dsi-panel-j2-38-0c-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 03 FF/39 01 00 00 00 00 03 51 03 FF/g' ${DTS_SOURCE}/dsi-panel-j11-38-08-0a-fhd-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 03 FF/39 01 00 00 00 00 03 51 03 FF/g' ${DTS_SOURCE}/dsi-panel-j9-38-0a-0a-fhd-video.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 07 FF/39 01 00 00 00 00 03 51 07 FF/g' ${DTS_SOURCE}/dsi-panel-j1u-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 07 FF/39 01 00 00 00 00 03 51 07 FF/g' ${DTS_SOURCE}/dsi-panel-j2-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 07 FF/39 01 00 00 00 00 03 51 07 FF/g' ${DTS_SOURCE}/dsi-panel-j2-p1-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 0F FF/39 01 00 00 00 00 03 51 0F FF/g' ${DTS_SOURCE}/dsi-panel-j1u-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 0F FF/39 01 00 00 00 00 03 51 0F FF/g' ${DTS_SOURCE}/dsi-panel-j2-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 03 51 0F FF/39 01 00 00 00 00 03 51 0F FF/g' ${DTS_SOURCE}/dsi-panel-j2-p1-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 05 51 07 FF 00 00/39 01 00 00 00 00 05 51 07 FF 00 00/g' ${DTS_SOURCE}/dsi-panel-j1s-42-02-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 05 51 07 FF 00 00/39 01 00 00 00 00 05 51 07 FF 00 00/g' ${DTS_SOURCE}/dsi-panel-j1s-42-02-0a-mp-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 05 51 07 FF 00 00/39 01 00 00 00 00 05 51 07 FF 00 00/g' ${DTS_SOURCE}/dsi-panel-j2-mp-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 05 51 07 FF 00 00/39 01 00 00 00 00 05 51 07 FF 00 00/g' ${DTS_SOURCE}/dsi-panel-j2-p2-1-42-02-0b-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 00 00 05 51 07 FF 00 00/39 01 00 00 00 00 05 51 07 FF 00 00/g' ${DTS_SOURCE}/dsi-panel-j2s-mp-42-02-0a-dsc-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 01 00 03 51 03 FF/39 01 00 00 01 00 03 51 03 FF/g' ${DTS_SOURCE}/dsi-panel-j11-38-08-0a-fhd-cmd.dtsi || true
        sed -i 's/\/\/39 01 00 00 11 00 03 51 03 FF/39 01 00 00 11 00 03 51 03 FF/g' ${DTS_SOURCE}/dsi-panel-j2-p2-1-38-0c-0a-dsc-cmd.dtsi || true
    fi

    if [ -f "arch/arm64/configs/vendor/xiaomi/${DEVICE_NAME}.config" ]; then
        echo "[*] Making PixelOS modular defconfig (kona-perf + sm8250-common + ${DEVICE_NAME})..."
        make "${MAKE_OPTS[@]}" vendor/kona-perf_defconfig vendor/xiaomi/sm8250-common.config "vendor/xiaomi/${DEVICE_NAME}.config"
    else
        echo "[*] Making defconfig: ${DEFCONFIG}..."
        make "${MAKE_OPTS[@]}" "${DEFCONFIG}"
    fi

    # ----------------------------------------------------
    # Configuration tweaks
    # ----------------------------------------------------
    
    # 1. Baseband-guard configuration (Ensure disabled)
    echo "[*] Ensuring Baseband-guard is disabled..."
    scripts/config --file "${OUT_DIR}/.config" -d BBG -d BPF_PRELOAD -d BPF_PRELOAD_UMD

    # 2. KernelSU configurations
    if [ "$ENABLE_KSU" -eq 1 ]; then
        echo "[*] Injecting KernelSU & SUSFS configurations..."
        scripts/config --file "${OUT_DIR}/.config" \
            -e KSU \
            -e THREAD_INFO_IN_TASK \
            -e KSU_SUSFS \
            -d CFI \
            -d CFI_CLANG \
            -d CFI_CLANG_SHADOW \
            -e CFI_PERMISSIVE
    fi

    # 2.1 EROFS filesystem configuration (Mandatory for PixelOS Android 14/15/17)
    # 2.2 Link-Time Optimization (ThinLTO for fast compilation without runner memory starvation)
    echo "[*] Ensuring ThinLTO is enabled..."
    scripts/config --file "${OUT_DIR}/.config" -e LTO_CLANG -e THINLTO -d LTO_NONE

    echo "[*] Ensuring EROFS filesystem support..."
    scripts/config --file "${OUT_DIR}/.config" \
        -e EROFS_FS \
        -e EROFS_FS_PCPU_KTHREAD \
        -e EROFS_FS_PCPU_KTHREAD_HIPRI

    # 2.3 Universal Interoperability & Low-Risk Performance Optimizations
    echo "[*] Injecting Low-Risk Performance & Network Optimizations (Westwood, BBR, ZRAM, ThinLTO)..."
    scripts/config --file "${OUT_DIR}/.config" \
        -e USER_NS \
        -e PID_NS \
        -e MEMCG \
        -e MEMCG_SWAP \
        -e F2FS_FS_ZSTD \
        -e F2FS_FS_LZ4 \
        -d F2FS_UNFAIR_RWSEM \
        -e KPROBES \
        -e HAVE_KPROBES \
        -e KPROBE_EVENTS \
        -e TCP_CONG_ADVANCED \
        -e TCP_CONG_WESTWOOD \
        -e TCP_CONG_BBR \
        -e DEFAULT_WESTWOOD \
        --set-str DEFAULT_TCP_CONG "westwood" \
        -e ZRAM_WRITEBACK \
        -e CRYPTO_ZSTD \
        -e ZRAM_DEF_COMP_ZSTD \
        --set-str ZRAM_DEF_COMP "zstd" \
        -d SLUB_DEBUG_ON \
        -d PROVE_LOCKING \
        -d LOCKDEP

    # 3. MIUI configurations
    if [ "$OS_TYPE" == "miui" ]; then
        echo "[*] Injecting MIUI specific configurations..."
        scripts/config --file "${OUT_DIR}/.config" \
            --set-str STATIC_USERMODEHELPER_PATH /system/bin/micd \
            -e PERF_CRITICAL_RT_TASK \
            -e SF_BINDER \
            -e OVERLAY_FS \
            -e MIGT \
            -e MIGT_ENERGY_MODEL \
            -e MIHW \
            -e PACKAGE_RUNTIME_INFO \
            -e BINDER_OPT \
            -e KPERFEVENTS \
            -e PERF_HUMANTASK \
            -d LTO_CLANG \
            -e LTO_NONE \
            -d SHADOW_CALL_STACK \
            -e XIAOMI_MIUI \
            -d MI_MEMORY_SYSFS \
            -e TASK_DELAY_ACCT \
            -e MIUI_ZRAM_MEMORY_TRACKING \
            -e PERF_HELPER \
            -e BOOTUP_RECLAIM \
            -e MI_RECLAIM \
            -e RTMM \
            -e MILLET_CGROUP \
            -e MILLET_SIG \
            -e MILLET_BINDER \
            -e MILLET_PKG \
            -e MILLET_BINDER_GKI \
            -e MILLET_CORE \
            -e MILLET_HS \
            -e BINDER_PRIO \
            -d REKERNEL \
            -d REKERNEL_NETWORK
    fi

    # 4. AOSP configurations
    if [ "$OS_TYPE" == "aosp" ]; then
        echo "[*] Injecting AOSP specific configurations..."
        scripts/config --file "${OUT_DIR}/.config" \
            -d REKERNEL \
            -d REKERNEL_NETWORK
    fi

    # 5. DroidSpaces configurations
    if [ "$ENABLE_DROIDSPACES" -eq 1 ]; then
        echo "[*] Merging DroidSpaces configurations..."
        scripts/kconfig/merge_config.sh -O "${OUT_DIR}/" -m "${OUT_DIR}/.config" "${DS_CONFIG_FRAG}"
    fi

    # We always need to re-evaluate dependencies because BBG is injected unconditionally
    echo "[*] Updating config (make olddefconfig)..."
    make "${MAKE_OPTS[@]}" olddefconfig

    # ----------------------------------------------------
    # Automated Compatibility Test Suite
    # ----------------------------------------------------
    echo "==========================================="
    echo " [*] Running Kernel 4.19 & Android Compatibility Test Suite"
    echo "==========================================="
    python3 scripts/test_kernel_compatibility.py "${OUT_DIR}/.config"
    echo "[+] Compatibility verification passed successfully!"
    echo "==========================================="


    # ----------------------------------------------------
    # Compilation
    # ----------------------------------------------------
    echo "[*] Building kernel..."
    make "${MAKE_OPTS[@]}" 

    # Restore DTS backup for MIUI
    if [ "$OS_TYPE" == "miui" ]; then
        echo "[*] Restoring DTS backups..."
        rm -rf "${DTS_SOURCE}"
        mv "${DTS_BACKUP}" "${DTS_SOURCE}"
    fi

    echo "==========================================="
    if [ -f "${OUT_DIR}/arch/arm64/boot/Image" ]; then
        echo "[+] $OS_TYPE Build Successful!"
        echo "[+] Kernel Image path: ${OUT_DIR}/arch/arm64/boot/Image"

        # ISO/IEC 29119 Quality Gate: Assert ARM64 Header & Size
        local IMG_SIZE=$(stat -c%s "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null || stat -f%z "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null || echo 0)
        local IMG_MAGIC=$(od -A n -N 4 -j 56 -t x1 "${OUT_DIR}/arch/arm64/boot/Image" 2>/dev/null | tr -d ' ')
        echo "[+] [ISO 29119 Quality Gate] Validating Kernel Image:"
        echo "    - File Size: $((IMG_SIZE / 1024 / 1024)) MB ($IMG_SIZE bytes)"
        echo "    - ARM64 Header Magic (offset 0x38): 0x${IMG_MAGIC} (Expected: 41524d64 / 'ARMd')"
        
        if [ "$IMG_SIZE" -lt 30000000 ] || [ "$IMG_SIZE" -gt 80000000 ]; then
            echo "[-] [ISO 29119 ABORT] Kernel Image size anomaly: $IMG_SIZE bytes!"
            exit 1
        fi
        if [ "$IMG_MAGIC" != "41524d64" ]; then
            echo "[-] [ISO 29119 ABORT] Kernel Image missing valid ARM64 magic header: 0x$IMG_MAGIC"
            exit 1
        fi
        echo "[+] [ISO 29119 PASS] Kernel Image verified valid and intact."

        echo "[*] Packaging Pure Kernel to AnyKernel3 ($OS_TYPE)..."
        rm -rf anykernel/kernels anykernel/dtb anykernel/dtbo.img anykernel/modules anykernel/Image
        
        # Copy compiled pure kernel Image directly to AnyKernel3 root
        cp "${OUT_DIR}/arch/arm64/boot/Image" "anykernel/Image"
        
        # Determine ZIP Filename
        local KSU_ZIP_STR="NoKernelSU"
        if [ "$ENABLE_KSU" -eq 1 ]; then
            KSU_ZIP_STR="ReSukiSU-SuSFS"
        fi
        local DS_ZIP_STR=""
        if [ "$ENABLE_DROIDSPACES" -eq 1 ]; then
            DS_ZIP_STR="_DroidSpaces"
        fi
        local GIT_COMMIT_ID=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
        local OS_UPPER=$(echo "$OS_TYPE" | tr '[:lower:]' '[:upper:]')
        local ZIP_FILENAME="APTKernel_Pure_${OS_UPPER}_${DEVICE_NAME}_${KSU_ZIP_STR}${DS_ZIP_STR}_$(date +'%Y%m%d_%H%M%S')_anykernel3_${GIT_COMMIT_ID}.zip"
        
        echo "[*] Zipping $ZIP_FILENAME in anykernel directory..."
        pushd anykernel > /dev/null
        zip -r9 "$ZIP_FILENAME" ./* -x .git .gitignore out/ ./*.zip > /dev/null
        mv "$ZIP_FILENAME" ../
        popd > /dev/null
        
        echo "[+] $OS_TYPE kernel binaries successfully packed into: $ZIP_FILENAME"
    else
        echo "[-] $OS_TYPE Build Failed. Kernel Image not found."
        exit 1
    fi
}

# ==========================================
# Execute builds based on target OS
# ==========================================
if [ "$TARGET_OS" == "aosp" ] || [ "$TARGET_OS" == "both" ]; then
    build_target "aosp"
fi

if [ "$TARGET_OS" == "miui" ] || [ "$TARGET_OS" == "both" ]; then
    build_target "miui"
fi

echo "==========================================="
echo "[*] ccache stats:"
ccache -s

# Cleanup DroidSpaces temp directory
if [ -n "$DS_TMP_DIR" ] && [ -d "$DS_TMP_DIR" ]; then
    rm -rf "$DS_TMP_DIR"
fi

echo "[+] All requested builds completed!"
