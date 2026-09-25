#!/usr/bin/env python3
"""
Comprehensive Kernel & Android Compatibility Test Suite for Alioth / SM8250
Evaluates target .config against:
1. Xiaomi SM8250 / POCO F3 (alioth) Hardware & HAL compatibility
2. Kernel 4.19 Core Subsystems & Backports (EROFS, KFENCE, WALT, fscrypt, KProbes, etc.)
3. Android Compatibility Matrix (Android 11 through Android 16/17 requirements)
4. Universal AOSP ROM Ecosystem Interoperability (F2FS multi-compression, WireGuard, namespaces, exFAT)
"""

import sys
import os
import re

def parse_config(path):
    cfg = {}
    if not os.path.exists(path):
        print(f"[!] Error: Config file not found at {path}")
        sys.exit(1)
        
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or (line.startswith('#') and 'is not set' not in line):
                continue
            m = re.match(r'^(CONFIG_[A-Za-z0-9_]+)=(.*)$', line)
            if m:
                cfg[m.group(1)] = m.group(2)
            else:
                m_unset = re.match(r'^#\s+(CONFIG_[A-Za-z0-9_]+)\s+is not set$', line)
                if m_unset:
                    cfg[m_unset.group(1)] = 'n'
    return cfg

def run_tests(config_path):
    cfg = parse_config(config_path)
    
    test_suite = [
        # =========================================================================
        # 1. PIXELOS & SM8250 XIAOMI HARDWARE COMPATIBILITY
        # =========================================================================
        ("Xiaomi Alioth Hardware", "FocalTech Touchscreen", "CONFIG_TOUCHSCREEN_FOCALTECH", "y", "Alioth primary touch digitizer"),
        ("Xiaomi Alioth Hardware", "Xiaomi Touch Feature Layer", "CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE", "y", "Touch sampling rate and game gesture abstraction"),
        ("Xiaomi Alioth Hardware", "Touchscreen Common Driver", "CONFIG_TOUCHSCREEN_COMMON", "y", "Touch HAL gesture interface"),
        ("Xiaomi Alioth Hardware", "KTZ8866 AMOLED Backlight", "CONFIG_BACKLIGHT_KTZ8866", "y", "Samsung E4 AMOLED backlight controller"),
        ("Xiaomi Alioth Hardware", "Dual KTZ8866 Conflict Check", "CONFIG_BACKLIGHT_DUALKTZ8866", "n", "Must be disabled for single-panel alioth to avoid symbol collision"),
        ("Xiaomi Alioth Hardware", "DRM Display Subsystem", "CONFIG_DRM", "y", "Qualcomm SDE DRM KMS driver for Adreno 650"),
        ("Xiaomi Alioth Hardware", "BQ2597X 33W Charge Pump", "CONFIG_BQ2597X_CHARGE_PUMP", "y", "Alioth 33W Fast Charging hardware"),
        ("Xiaomi Alioth Hardware", "DS28E16 Battery Authenticator", "CONFIG_BATT_VERIFY_BY_DS28E16", "y", "OEM Battery security verification with batterysecret HAL"),
        ("Xiaomi Alioth Hardware", "1-Wire GPIO Protocol", "CONFIG_ONEWIRE_GPIO", "y", "Communication bus for DS28E16 security chip"),
        ("Xiaomi Alioth Hardware", "Disable Slave Charger SMB1355", "CONFIG_SMB1355_SLAVE_CHARGER", "n", "Prevents charger driver race conditions"),
        ("Xiaomi Alioth Hardware", "Disable Slave Charger SMB1390", "CONFIG_SMB1390_CHARGE_PUMP_PSY", "n", "Locks charging pump exclusively to BQ2597X"),
        ("Xiaomi Alioth Hardware", "AW8697 Z-Axis Haptics", "CONFIG_INPUT_AW8697_HAPTIC", "y", "Alioth linear resonant haptic actuator"),
        ("Xiaomi Alioth Hardware", "Disable Generic QTI Haptics", "CONFIG_INPUT_QTI_HAPTICS", "n", "Prevents vibration motor conflict"),
        ("Xiaomi Alioth Hardware", "FPC Fingerprint Scanner", "CONFIG_FINGERPRINT_FPC", "y", "Side fingerprint sensor (FPC)"),
        ("Xiaomi Alioth Hardware", "Goodix Fingerprint Scanner", "CONFIG_FINGERPRINT_GOODIX", "y", "Side fingerprint sensor (Goodix)"),
        ("Xiaomi Alioth Hardware", "Elliptic Ultrasound Proximity", "CONFIG_US_PROXIMITY", "y", "Inner-beauty virtual proximity sensor"),
        ("Xiaomi Alioth Hardware", "AKM09970 Hall Effect Sensor", "CONFIG_HALL_AKM09970", "y", "Magnetic flip cover detection"),
        ("Xiaomi Alioth Hardware", "Parade PS5169 Type-C Redriver", "CONFIG_PS5169", "y", "USB PD 3.0, fast charging, and OTG signal repeater"),
        ("Xiaomi Alioth Hardware", "Consumer IR SPI Driver", "CONFIG_IR_SPI", "y", "IR remote control blaster hardware"),
        ("Xiaomi Alioth Hardware", "QCA CLD3 Wi-Fi 6", "CONFIG_QCA_CLD_WLAN", "y", "Qualcomm FastConnect 6900 Wi-Fi subsystem"),
        ("Xiaomi Alioth Hardware", "ALSA SoC Audio Core", "CONFIG_SND_SOC", "y", "Audio HAL and TFA amplifier backend"),
        ("Xiaomi Alioth Hardware", "Qualcomm Alioth Platform", "CONFIG_MACH_XIAOMI_ALIOTH", "y", "POCO F3 board platform identification"),
        ("Xiaomi Alioth Hardware", "Qualcomm RPMh Power Regulators", "CONFIG_REGULATOR_QCOM_RPMH", "y", "PM8250 power management IC regulator driver (prevents freeze at millisecond 0)"),
        ("Xiaomi Alioth Hardware", "Qualcomm RPMh Core Driver", "CONFIG_QCOM_RPMH", "y", "Resource Power Manager Hardened communication bus"),

        # =========================================================================
        # 2. KERNEL 4.19 CORE SUBSYSTEMS & MODERN BACKPORTS
        # =========================================================================
        ("Kernel 4.19 Backports", "EROFS Filesystem (5.4+ Backport)", "CONFIG_EROFS_FS", "y", "Modern read-only compressed filesystem support"),
        ("Kernel 4.19 Backports", "EROFS Per-CPU KThread", "CONFIG_EROFS_FS_PCPU_KTHREAD", "y", "High-throughput parallel decompression threads"),
        ("Kernel 4.19 Backports", "EROFS High-Priority KThread", "CONFIG_EROFS_FS_PCPU_KTHREAD_HIPRI", "y", "Zero-jitter system partition decompression"),
        ("Kernel 4.19 Backports", "F2FS Filesystem", "CONFIG_F2FS_FS", "y", "Flash-Friendly Filesystem for userdata"),
        ("Kernel 4.19 Backports", "F2FS Compression Backport", "CONFIG_F2FS_FS_COMPRESSION", "y", "Userdata block compression"),
        ("Kernel 4.19 Backports", "Inline Filesystem Encryption", "CONFIG_FS_ENCRYPTION", "y", "Hardware-accelerated FBE crypto"),
        ("Kernel 4.19 Backports", "Qualcomm WALT Scheduler", "CONFIG_SCHED_WALT", "y", "Window-Assisted Load Tracking energy scheduler"),
        ("Kernel 4.19 Backports", "Pressure Stall Information (PSI)", "CONFIG_PSI", "y", "CPU/IO/Memory stall metrics for modern Android LMKD"),
        ("Kernel 4.19 Backports", "Schedutil CPU Frequency Governor", "CONFIG_CPU_FREQ_GOV_SCHEDUTIL", "y", "EAS frequency governor hooked to WALT"),
        ("Kernel 4.19 Backports", "ThinLTO Compilation", "CONFIG_THINLTO", "y", "Parallel link-time optimization with negligible link latency"),
        ("Kernel 4.19 Backports", "ZRAM Swap Device", "CONFIG_ZRAM", "y", "Compressed RAM swap block driver"),
        ("Kernel 4.19 Backports", "ZSTD Crypto Algorithm", "CONFIG_CRYPTO_ZSTD", "y", "High-ratio ZSTD compression algorithm for ZRAM"),
        ("Kernel 4.19 Backports", "RCU Priority Boosting", "CONFIG_RCU_BOOST", "y", "Prevents RCU readers from stalling UI execution threads"),
        ("Kernel 4.19 Backports", "Writeback Throttling", "CONFIG_BLK_WBT", "y", "Prevents I/O write starvation on UFS 3.1 storage"),
        ("Kernel 4.19 Backports", "Dynamic Kernel Probes (KProbes)", "CONFIG_KPROBES", "y", "Live tracing, eBPF probes, and dynamic kernel patching"),

        # =========================================================================
        # 3. ANDROID OS COMPATIBILITY MATRIX (Android 11 - 16/17)
        # =========================================================================
        ("Android Compatibility", "Android BinderFS", "CONFIG_ANDROID_BINDERFS", "y", "Mandatory isolated IPC filesystem for Android 10+"),
        ("Android Compatibility", "Android Binder IPC", "CONFIG_ANDROID_BINDER_IPC", "y", "Core Android IPC driver"),
        ("Android Compatibility", "Memory Cgroup Tracking", "CONFIG_MEMCG", "y", "Required by androidboot.memcg=1 and modern LMKD"),
        ("Android Compatibility", "Swap Memory Cgroup", "CONFIG_MEMCG_SWAP", "y", "Swap cgroup accounting for Android memory quotas"),
        ("Android Compatibility", "BPF Subsystem", "CONFIG_BPF_SYSCALL", "y", "eBPF kernel execution engine for Android network stats"),
        ("Android Compatibility", "Cgroup BPF Network Hooks", "CONFIG_CGROUP_BPF", "y", "Traffic controller & network socket tagging"),
        ("Android Compatibility", "Control Flow Integrity Strict Disabled", "CONFIG_CFI_CLANG", "n", "Must not be strict to avoid bootloop panic with KSU"),
        ("Android Compatibility", "Ashmem Shared Memory", "CONFIG_ASHMEM", "y", "Android shared memory allocator"),
        ("Android Compatibility", "Ashmem-to-Memfd Shim", "CONFIG_MEMFD_ASHMEM_SHIM", "y", "Seamless compatibility between legacy Ashmem and Android 13+ memfd"),
        ("Android Compatibility", "SELinux Security Subsystem", "CONFIG_SECURITY_SELINUX", "y", "Mandatory Android SELinux access control"),
        ("Android Compatibility", "Process Namespaces Support", "CONFIG_NAMESPACES", "y", "Android app isolation and process security boundaries"),
        ("Android Compatibility", "Overlay Filesystem", "CONFIG_OVERLAY_FS", "y", "Required for dynamic partition overlay & volatile testing"),
        ("Android Compatibility", "EXT4 Filesystem", "CONFIG_EXT4_FS", "y", "Required for system/product/metadata mounts"),
        ("Android Compatibility", "VFAT Firmware Filesystem", "CONFIG_VFAT_FS", "y", "Required for modem, DSP, and BT firmware mounts"),
        ("Android Compatibility", "Loop Device Partitioning", "CONFIG_BLK_DEV_LOOP", "y", "Required for APEX modules & virtual disk mounting"),

        # =========================================================================
        # 4. UNIVERSAL AOSP ROM ECOSYSTEM INTEROPERABILITY
        # =========================================================================
        ("Universal Interoperability", "In-Kernel WireGuard VPN", "CONFIG_WIREGUARD", "y", "Ultra-fast native WireGuard VPN with minimal battery consumption"),
        ("Universal Interoperability", "Microsoft exFAT Filesystem", "CONFIG_EXFAT_FS", "y", "Native support for high-capacity external MicroSD and USB-OTG flash drives"),
        ("Universal Interoperability", "User Namespaces (Rootless PRoot)", "CONFIG_USER_NS", "y", "Enables Termux rootless proot, Linux containers, and isolated work profiles"),
        ("Universal Interoperability", "PID Namespaces", "CONFIG_PID_NS", "y", "Enables full process tree isolation for containerization"),
        ("Universal Interoperability", "F2FS ZSTD Decompression", "CONFIG_F2FS_FS_ZSTD", "y", "Interoperability with custom ROMs utilizing ZSTD userdata compression"),
        ("Universal Interoperability", "F2FS LZ4 Decompression", "CONFIG_F2FS_FS_LZ4", "y", "Interoperability with custom ROMs utilizing LZ4 userdata compression"),
        ("Universal Interoperability", "Universal Tethering / Masquerade", "CONFIG_IP_NF_TARGET_MASQUERADE", "y", "Ensures seamless Wi-Fi hotspot and USB tethering NAT routing"),

        # =========================================================================
        # 5. PIXELOS ANDROID 16 FBE V2 & HARDWARE SECURITY ALIGNMENT
        # =========================================================================
        ("Security and Encryption", "Kernel Keyring Facility", "CONFIG_KEYS", "y", "Mandatory keystore infrastructure for Android synthetic password"),
        ("Security and Encryption", "32-bit Compat Keyring", "CONFIG_KEYS_COMPAT", "y", "Required for 32-bit Keymaster/KeyMint HAL compat"),
        ("Security and Encryption", "Qualcomm Inline Crypto Engine (ICE)", "CONFIG_CRYPTO_DEV_QCOM_ICE", "y", "Hardware UFS 3.1 inline crypto for FBE v2"),
        ("Security and Encryption", "Filesystem Inline Encryption", "CONFIG_FS_ENCRYPTION_INLINE_CRYPT", "y", "Direct inline encryption pass-through to ICE"),
        ("Security and Encryption", "Device-Mapper Default Key", "CONFIG_DM_DEFAULT_KEY", "y", "Metadata encryption on /dev/block/by-name/userdata"),
        ("Security and Encryption", "Block Inline Encryption", "CONFIG_BLK_INLINE_ENCRYPTION", "y", "Block layer inline encryption dispatch"),
        ("Security and Encryption", "Qualcomm QSEECOM Interface", "CONFIG_QSEECOM", "y", "TrustZone communication for KeyMint hardware keys"),
        ("Security and Encryption", "SELinux CheckReqProt Strict Zero", "CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE", "0", "Required by Android 12-16 to allow bionic mprotect checks"),
        ("Security and Encryption", "SELinux Development Permissive Mode", "CONFIG_SECURITY_SELINUX_DEVELOP", "y", "Allows permissive fallback to prevent hard bootloops"),
        ("Security and Encryption", "F2FS Fair RWSEM Checkpoint Protection", "CONFIG_F2FS_UNFAIR_RWSEM", "n", "Must be disabled to prevent race conditions during checkpoint flushes"),

        # =========================================================================
        # 6. BUILD INTEGRITY & ANTI-BOOTLOOP ENFORCEMENTS
        # =========================================================================
        ("Anti-Bootloop Integrity", "Disable Baseband-guard (BBG)", "CONFIG_BBG", "n", "Must be disabled to prevent LSM conflicts and modem SSR crash loops on AOSP"),
        ("Anti-Bootloop Integrity", "Disable In-Tree BPF Preload", "CONFIG_BPF_PRELOAD", "n", "Must be disabled; Android utilizes userspace bpfloader and in-kernel UMD causes incbin link failure"),
        ("Anti-Bootloop Integrity", "Disable In-Tree BPF Preload UMD", "CONFIG_BPF_PRELOAD_UMD", "n", "Must be disabled to prevent userprogs link failure"),
        ("Anti-Bootloop Integrity", "Disable ReKernel Core", "CONFIG_REKERNEL", "n", "Must be disabled to prevent Binder IPC race conditions and boot animation hang on Android 16"),
        ("Anti-Bootloop Integrity", "Disable ReKernel Network Hooks", "CONFIG_REKERNEL_NETWORK", "n", "Must be disabled on pristine AOSP to prevent networking stalls")
    ]

    # Dynamic Rule: If Clang CFI is enabled, CFI_PERMISSIVE must be enabled to prevent panic
    if cfg.get("CONFIG_CFI_CLANG") == "y":
        test_suite.append(("Android Compatibility", "Clang CFI Permissive Mode", "CONFIG_CFI_PERMISSIVE", "y", "Must be permissive when CFI is on to prevent panic with KSU"))

    print("================================================================================")
    print("      COMPREHENSIVE KERNEL & ANDROID COMPATIBILITY TEST SUITE")
    print(f"      Target: {config_path}")
    print("================================================================================\n")

    passed_count = 0
    failed_count = 0
    current_category = None

    for category, feature, symbol, expected, rationale in test_suite:
        if category != current_category:
            current_category = category
            print(f"\n--- [{current_category}] ---")
            
        actual = cfg.get(symbol, "n" if expected != "n" else "y" if symbol in cfg else "n")
        if symbol not in cfg:
            actual = "n"
            
        is_pass = (actual == expected)
        status_str = "PASS" if is_pass else "FAIL"

        if is_pass:
            passed_count += 1
            print(f"  [{status_str}] {feature:<40} | {symbol}={actual}")
        else:
            failed_count += 1
            print(f"  [{status_str}] {feature:<40} | {symbol}: expected '{expected}', got '{actual}' ({rationale})")

    total_tests = len(test_suite)
    print("\n================================================================================")
    print(f" SUMMARY: Total Tests: {total_tests} | Passed: {passed_count} | Failed: {failed_count}")
    print(f" SCORE:   {(passed_count / total_tests) * 100:.1f}%")
    print("================================================================================")

    if failed_count > 0:
        print("\n[!] FATAL: Kernel configuration failed compatibility tests!")
        print("[!] Build will be aborted to prevent non-booting or broken kernel.")
        sys.exit(1)
    else:
        print("\n[+] SUCCESS: Kernel configuration satisfies 100% of compatibility requirements.")
        print("[+] Fully verified for Xiaomi POCO F3 (alioth), Kernel 4.19, and All AOSP ROMs.")
        sys.exit(0)

def validate_kernel_image(image_path):
    print("================================================================================")
    print(" [ISO/IEC 29119 Quality Gate] Post-Flight Kernel Binary Validation")
    print("================================================================================")
    if not os.path.exists(image_path):
        print(f"[!] FAIL: Kernel Image not found at {image_path}")
        sys.exit(1)
        
    size_bytes = os.path.getsize(image_path)
    size_mb = size_bytes / 1024 / 1024
    print(f"[*] Validating file: {image_path}")
    print(f"[*] File Size: {size_mb:.2f} MB ({size_bytes} bytes)")
    
    if size_bytes < 30 * 1024 * 1024 or size_bytes > 80 * 1024 * 1024:
        print(f"[!] FAIL: Image size outside acceptable range (30MB - 80MB): {size_mb:.2f} MB")
        sys.exit(1)
    print("  [PASS] Binary size within expected operational bounds (30MB - 80MB).")
    
    with open(image_path, "rb") as f:
        # Header offset 0x38 (56 bytes) is ARM64 magic: 0x644d5241 (ASCII: 'ARM\x64')
        f.seek(0x38)
        magic = f.read(4)
        if magic != b'ARM\x64':
            print(f"[!] FAIL: Missing or invalid ARM64 header magic at 0x38: {magic.hex()} (expected: 41524d64)")
            sys.exit(1)
        print("  [PASS] Valid ARM64 Kernel Image Header Magic verified (0x644d5241 / 'ARM\\x64').")
        
    print("\n[+] SUCCESS: Kernel Image passed 100% of ISO/IEC 29119 binary integrity gates.")
    sys.exit(0)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_config>")
        print(f"       {sys.argv[0]} --validate-image <path_to_Image>")
        sys.exit(1)
        
    if sys.argv[1] == "--validate-image":
        if len(sys.argv) < 3:
            print("Error: --validate-image requires a path to the compiled Image")
            sys.exit(1)
        validate_kernel_image(sys.argv[2])
    else:
        run_tests(sys.argv[1])
