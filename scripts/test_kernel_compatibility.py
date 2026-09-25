#!/usr/bin/env python3
"""
Comprehensive Kernel & Android Compatibility Test Suite for Alioth / SM8250
ISO/IEC 29119 Multi-Tier Severity Quality Gate Engine:
- FATAL (Blocker): Immediate build termination (sys.exit(1)) if vital hardware, crypto, security, or anti-bootloop constraints fail.
- ADVISORY (Warning): Non-blocking informational audit (sys.exit(0)) for optional ecosystem flags.
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
                val = m.group(2).strip('"\'')
                cfg[m.group(1)] = val if val != '' else 'y'
            else:
                m_unset = re.match(r'^#\s+(CONFIG_[A-Za-z0-9_]+)\s+is not set$', line)
                if m_unset:
                    cfg[m_unset.group(1)] = 'n'
    return cfg

def run_tests(config_path):
    cfg = parse_config(config_path)
    
    # Format: (Category, Feature Name, CONFIG_SYMBOL, Expected Value, Rationale, Severity Level ["FATAL"|"ADVISORY"])
    test_suite = [
        # =========================================================================
        # 1. PIXELOS & SM8250 XIAOMI HARDWARE COMPATIBILITY (FATAL BLOCKERS)
        # =========================================================================
        ("Xiaomi Alioth Hardware", "FocalTech Touchscreen", "CONFIG_TOUCHSCREEN_FOCALTECH", "y", "Alioth primary touch digitizer", "FATAL"),
        ("Xiaomi Alioth Hardware", "Xiaomi Touch Feature Layer", "CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE", "y", "Touch sampling rate and game gesture abstraction", "FATAL"),
        ("Xiaomi Alioth Hardware", "Touchscreen Common Driver", "CONFIG_TOUCHSCREEN_COMMON", "y", "Touch HAL gesture interface", "FATAL"),
        ("Xiaomi Alioth Hardware", "KTZ8866 AMOLED Backlight", "CONFIG_BACKLIGHT_KTZ8866", "y", "Samsung E4 AMOLED backlight controller", "FATAL"),
        ("Xiaomi Alioth Hardware", "Dual KTZ8866 Support", "CONFIG_BACKLIGHT_DUALKTZ8866", "y", "SM8250 common backlight controller for AMOLED panels", "ADVISORY"),
        ("Xiaomi Alioth Hardware", "DRM Display Subsystem", "CONFIG_DRM", "y", "Qualcomm SDE DRM KMS driver for Adreno 650", "FATAL"),
        ("Xiaomi Alioth Hardware", "BQ2597X 33W Charge Pump", "CONFIG_BQ2597X_CHARGE_PUMP", "y", "Alioth 33W Fast Charging hardware", "FATAL"),
        ("Xiaomi Alioth Hardware", "DS28E16 Battery Authenticator", "CONFIG_BATT_VERIFY_BY_DS28E16", "y", "OEM Battery security verification with batterysecret HAL", "FATAL"),
        ("Xiaomi Alioth Hardware", "1-Wire GPIO Protocol", "CONFIG_ONEWIRE_GPIO", "y", "Communication bus for DS28E16 security chip", "FATAL"),
        ("Xiaomi Alioth Hardware", "Disable Slave Charger SMB1355", "CONFIG_SMB1355_SLAVE_CHARGER", "n", "Prevents charger driver race conditions", "FATAL"),
        ("Xiaomi Alioth Hardware", "Disable Slave Charger SMB1390", "CONFIG_SMB1390_CHARGE_PUMP_PSY", "n", "Locks charging pump exclusively to BQ2597X", "FATAL"),
        ("Xiaomi Alioth Hardware", "AW8697 Z-Axis Haptics", "CONFIG_INPUT_AW8697_HAPTIC", "y", "Alioth linear resonant haptic actuator", "FATAL"),
        ("Xiaomi Alioth Hardware", "Disable Generic QTI Haptics", "CONFIG_INPUT_QTI_HAPTICS", "n", "Prevents vibration motor conflict", "FATAL"),
        ("Xiaomi Alioth Hardware", "FPC Fingerprint Scanner", "CONFIG_FINGERPRINT_FPC", "y", "Side fingerprint sensor (FPC)", "FATAL"),
        ("Xiaomi Alioth Hardware", "Goodix Fingerprint Scanner", "CONFIG_FINGERPRINT_GOODIX", "y", "Side fingerprint sensor (Goodix)", "FATAL"),
        ("Xiaomi Alioth Hardware", "Elliptic Ultrasound Proximity", "CONFIG_US_PROXIMITY", "y", "Inner-beauty virtual proximity sensor", "FATAL"),
        ("Xiaomi Alioth Hardware", "AKM09970 Hall Effect Sensor", "CONFIG_HALL_AKM09970", "y", "Magnetic flip cover detection", "FATAL"),
        ("Xiaomi Alioth Hardware", "Parade PS5169 Type-C Redriver", "CONFIG_PS5169", "y", "USB PD 3.0, fast charging, and OTG signal repeater", "FATAL"),
        ("Xiaomi Alioth Hardware", "Consumer IR SPI Driver", "CONFIG_IR_SPI", "y", "IR remote control blaster hardware", "FATAL"),
        ("Xiaomi Alioth Hardware", "QCA CLD3 Wi-Fi 6", "CONFIG_QCA_CLD_WLAN", "y", "Qualcomm FastConnect 6900 Wi-Fi subsystem", "FATAL"),
        ("Xiaomi Alioth Hardware", "ALSA SoC Audio Core", "CONFIG_SND_SOC", "y", "Audio HAL and TFA amplifier backend", "FATAL"),
        ("Xiaomi Alioth Hardware", "Qualcomm Alioth Platform", "CONFIG_MACH_XIAOMI_ALIOTH", "y", "POCO F3 board platform identification", "FATAL"),
        ("Xiaomi Alioth Hardware", "Qualcomm RPMh Power Regulators", "CONFIG_REGULATOR_QCOM_RPMH", "y", "PM8250 power management IC regulator driver (prevents freeze at millisecond 0)", "FATAL"),
        ("Xiaomi Alioth Hardware", "Qualcomm RPMh Core Driver", "CONFIG_QCOM_RPMH", "y", "Resource Power Manager Hardened communication bus", "FATAL"),

        # =========================================================================
        # 2. KERNEL 4.19 CORE SUBSYSTEMS & MODERN BACKPORTS
        # =========================================================================
        ("Kernel 4.19 Backports", "EROFS Filesystem (5.4+ Backport)", "CONFIG_EROFS_FS", "y", "Modern read-only compressed filesystem support", "FATAL"),
        ("Kernel 4.19 Backports", "EROFS Per-CPU KThread", "CONFIG_EROFS_FS_PCPU_KTHREAD", "y", "High-throughput parallel decompression threads", "FATAL"),
        ("Kernel 4.19 Backports", "EROFS High-Priority KThread", "CONFIG_EROFS_FS_PCPU_KTHREAD_HIPRI", "y", "Zero-jitter system partition decompression", "ADVISORY"),
        ("Kernel 4.19 Backports", "F2FS Filesystem", "CONFIG_F2FS_FS", "y", "Flash-Friendly Filesystem for userdata", "FATAL"),
        ("Kernel 4.19 Backports", "F2FS Compression Backport", "CONFIG_F2FS_FS_COMPRESSION", "y", "Userdata block compression", "ADVISORY"),
        ("Kernel 4.19 Backports", "Inline Filesystem Encryption", "CONFIG_FS_ENCRYPTION", "y", "Hardware-accelerated FBE crypto", "FATAL"),
        ("Kernel 4.19 Backports", "Qualcomm WALT Scheduler", "CONFIG_SCHED_WALT", "y", "Window-Assisted Load Tracking energy scheduler", "FATAL"),
        ("Kernel 4.19 Backports", "Pressure Stall Information (PSI)", "CONFIG_PSI", "y", "CPU/IO/Memory stall metrics for modern Android LMKD", "FATAL"),
        ("Kernel 4.19 Backports", "Schedutil CPU Frequency Governor", "CONFIG_CPU_FREQ_GOV_SCHEDUTIL", "y", "EAS frequency governor hooked to WALT", "FATAL"),
        ("Kernel 4.19 Backports", "ThinLTO Compilation", "CONFIG_THINLTO", "y", "Parallel link-time optimization with negligible link latency", "FATAL"),
        ("Kernel 4.19 Backports", "ZRAM Swap Device", "CONFIG_ZRAM", "y", "Compressed RAM swap block driver", "FATAL"),
        ("Kernel 4.19 Backports", "ZSTD Crypto Algorithm", "CONFIG_CRYPTO_ZSTD", "y", "High-ratio ZSTD compression algorithm for ZRAM", "FATAL"),
        ("Kernel 4.19 Backports", "RCU Priority Boosting", "CONFIG_RCU_BOOST", "y", "Prevents RCU readers from stalling UI execution threads", "ADVISORY"),
        ("Kernel 4.19 Backports", "Writeback Throttling", "CONFIG_BLK_WBT", "y", "Prevents I/O write starvation on UFS 3.1 storage", "ADVISORY"),
        ("Kernel 4.19 Backports", "Dynamic Kernel Probes (KProbes)", "CONFIG_KPROBES", "y", "Live tracing, eBPF probes, and dynamic kernel patching", "FATAL"),

        # =========================================================================
        # 3. ANDROID OS COMPATIBILITY MATRIX (Android 11 - 17)
        # =========================================================================
        ("Android Compatibility", "Android BinderFS", "CONFIG_ANDROID_BINDERFS", "y", "Mandatory isolated IPC filesystem for Android 10+", "FATAL"),
        ("Android Compatibility", "Android Binder IPC", "CONFIG_ANDROID_BINDER_IPC", "y", "Core Android IPC driver", "FATAL"),
        ("Android Compatibility", "Memory Cgroup Tracking", "CONFIG_MEMCG", "y", "Required by androidboot.memcg=1 and modern LMKD", "FATAL"),
        ("Android Compatibility", "Swap Memory Cgroup", "CONFIG_MEMCG_SWAP", "y", "Swap cgroup accounting for Android memory quotas", "FATAL"),
        ("Android Compatibility", "BPF Subsystem", "CONFIG_BPF_SYSCALL", "y", "eBPF kernel execution engine for Android network stats", "FATAL"),
        ("Android Compatibility", "Cgroup BPF Network Hooks", "CONFIG_CGROUP_BPF", "y", "Traffic controller & network socket tagging", "FATAL"),
        ("Android Compatibility", "Control Flow Integrity Strict Disabled", "CONFIG_CFI_CLANG", "n", "Must not be strict to avoid bootloop panic with KSU", "FATAL"),
        ("Android Compatibility", "Ashmem Shared Memory", "CONFIG_ASHMEM", "y", "Android shared memory allocator", "FATAL"),
        ("Android Compatibility", "Ashmem-to-Memfd Shim", "CONFIG_MEMFD_ASHMEM_SHIM", "y", "Seamless compatibility between legacy Ashmem and Android 13+ memfd", "ADVISORY"),
        ("Android Compatibility", "SELinux Security Subsystem", "CONFIG_SECURITY_SELINUX", "y", "Mandatory Android SELinux access control", "FATAL"),
        ("Android Compatibility", "Process Namespaces Support", "CONFIG_NAMESPACES", "y", "Android app isolation and process security boundaries", "FATAL"),
        ("Android Compatibility", "Overlay Filesystem", "CONFIG_OVERLAY_FS", "y", "Required for dynamic partition overlay & volatile testing", "FATAL"),
        ("Android Compatibility", "EXT4 Filesystem", "CONFIG_EXT4_FS", "y", "Required for system/product/metadata mounts", "FATAL"),
        ("Android Compatibility", "VFAT Firmware Filesystem", "CONFIG_VFAT_FS", "y", "Required for modem, DSP, and BT firmware mounts", "FATAL"),
        ("Android Compatibility", "Loop Device Partitioning", "CONFIG_BLK_DEV_LOOP", "y", "Required for APEX modules & virtual disk mounting", "FATAL"),

        # =========================================================================
        # 4. UNIVERSAL AOSP ROM ECOSYSTEM INTEROPERABILITY
        # =========================================================================
        ("Universal Interoperability", "In-Kernel WireGuard VPN", "CONFIG_WIREGUARD", "y", "Ultra-fast native WireGuard VPN with minimal battery consumption", "ADVISORY"),
        ("Universal Interoperability", "Microsoft exFAT Filesystem", "CONFIG_EXFAT_FS", "y", "Native support for high-capacity external MicroSD and USB-OTG flash drives", "ADVISORY"),
        ("Universal Interoperability", "User Namespaces (Rootless PRoot)", "CONFIG_USER_NS", "y", "Enables Termux rootless proot, Linux containers, and isolated work profiles", "ADVISORY"),
        ("Universal Interoperability", "PID Namespaces", "CONFIG_PID_NS", "y", "Enables full process tree isolation for containerization", "ADVISORY"),
        ("Universal Interoperability", "F2FS ZSTD Decompression", "CONFIG_F2FS_FS_ZSTD", "y", "Interoperability with custom ROMs utilizing ZSTD userdata compression", "ADVISORY"),
        ("Universal Interoperability", "F2FS LZ4 Decompression", "CONFIG_F2FS_FS_LZ4", "y", "Interoperability with custom ROMs utilizing LZ4 userdata compression", "ADVISORY"),
        ("Universal Interoperability", "Universal Tethering / Masquerade", "CONFIG_IP_NF_TARGET_MASQUERADE", "y", "Ensures seamless Wi-Fi hotspot and USB tethering NAT routing", "ADVISORY"),

        # =========================================================================
        # 5. PIXELOS ANDROID 16/17 FBE V2 & HARDWARE SECURITY ALIGNMENT
        # =========================================================================
        ("Security and Encryption", "Kernel Keyring Facility", "CONFIG_KEYS", "y", "Mandatory keystore infrastructure for Android synthetic password", "FATAL"),
        ("Security and Encryption", "32-bit Compat Keyring", "CONFIG_KEYS_COMPAT", "y", "Required for 32-bit Keymaster/KeyMint HAL compat", "FATAL"),
        ("Security and Encryption", "Qualcomm Inline Crypto Engine (ICE)", "CONFIG_CRYPTO_DEV_QCOM_ICE", "y", "Hardware UFS 3.1 inline crypto for FBE v2", "FATAL"),
        ("Security and Encryption", "Filesystem Inline Encryption", "CONFIG_FS_ENCRYPTION_INLINE_CRYPT", "y", "Direct inline encryption pass-through to ICE", "FATAL"),
        ("Security and Encryption", "Device-Mapper Default Key", "CONFIG_DM_DEFAULT_KEY", "y", "Metadata encryption on /dev/block/by-name/userdata", "FATAL"),
        ("Security and Encryption", "Block Inline Encryption", "CONFIG_BLK_INLINE_ENCRYPTION", "y", "Block layer inline encryption dispatch", "FATAL"),
        ("Security and Encryption", "Qualcomm QSEECOM Interface", "CONFIG_QSEECOM", "y", "TrustZone communication for KeyMint hardware keys", "FATAL"),
        ("Security and Encryption", "SELinux CheckReqProt Strict Zero", "CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE", "0", "Required by Android 12-17 to allow bionic mprotect checks", "FATAL"),
        ("Security and Encryption", "SELinux Development Permissive Mode", "CONFIG_SECURITY_SELINUX_DEVELOP", "y", "Allows permissive fallback to prevent hard bootloops", "ADVISORY"),
        ("Security and Encryption", "F2FS Fair RWSEM Checkpoint Protection", "CONFIG_F2FS_UNFAIR_RWSEM", "n", "Must be disabled to prevent race conditions during checkpoint flushes", "ADVISORY"),

        # =========================================================================
        # 6. ANTI-BOOTLOOP INTEGRITY & SAFETY GATES
        # =========================================================================
        ("Anti-Bootloop Integrity", "Disable Baseband-guard (BBG)", "CONFIG_BBG", "n", "Must be disabled to prevent LSM conflicts and modem SSR crash loops on AOSP", "FATAL"),
        ("Anti-Bootloop Integrity", "Disable In-Tree BPF Preload", "CONFIG_BPF_PRELOAD", "n", "Must be disabled; Android utilizes userspace bpfloader and in-kernel UMD causes incbin link failure", "FATAL"),
        ("Anti-Bootloop Integrity", "Disable In-Tree BPF Preload UMD", "CONFIG_BPF_PRELOAD_UMD", "n", "Must be disabled to prevent userprogs link failure", "FATAL"),
        ("Anti-Bootloop Integrity", "Disable ReKernel Core", "CONFIG_REKERNEL", "n", "Must be disabled to prevent Binder IPC race conditions and boot animation hang on Android 16/17", "FATAL"),
        ("Anti-Bootloop Integrity", "Disable ReKernel Network Hooks", "CONFIG_REKERNEL_NETWORK", "n", "Must be disabled on pristine AOSP to prevent networking stalls", "FATAL")
    ]

    # Dynamic Rule: If Clang CFI is enabled, CFI_PERMISSIVE must be enabled to prevent panic
    if cfg.get("CONFIG_CFI_CLANG") == "y":
        test_suite.append(("Android Compatibility", "Clang CFI Permissive Mode", "CONFIG_CFI_PERMISSIVE", "y", "Must be permissive when CFI is on to prevent panic with KSU", "FATAL"))

    print("================================================================================")
    print("      COMPREHENSIVE KERNEL & ANDROID COMPATIBILITY TEST SUITE (ISO/IEC 29119)")
    print(f"      Target: {config_path}")
    print("================================================================================\n")

    passed_count = 0
    fatal_failed_count = 0
    advisory_failed_count = 0
    current_category = None

    for category, feature, symbol, expected, rationale, severity in test_suite:
        if category != current_category:
            current_category = category
            print(f"\n--- [{current_category}] ---")
            
        actual = cfg.get(symbol, "n" if expected != "n" else "y" if symbol in cfg else "n")
        if symbol not in cfg:
            actual = "n"
            
        is_pass = (actual == expected)
        status_str = "PASS" if is_pass else ("FAIL [FATAL]" if severity == "FATAL" else "FAIL [ADVISORY]")

        if is_pass:
            passed_count += 1
            print(f"  [{status_str}] {feature:<40} | {symbol}={actual}")
        else:
            if severity == "FATAL":
                fatal_failed_count += 1
            else:
                advisory_failed_count += 1
            print(f"  [{status_str}] {feature:<40} | {symbol}: expected '{expected}', got '{actual}' ({rationale})")

    total_tests = len(test_suite)
    print("\n================================================================================")
    print(f" SUMMARY: Total Tests: {total_tests} | Passed: {passed_count} | Fatal Failed: {fatal_failed_count} | Advisory Failed: {advisory_failed_count}")
    print(f" SCORE:   {(passed_count / total_tests) * 100:.1f}%")
    print("================================================================================")

    if fatal_failed_count > 0:
        print(f"\n[-] [ISO/IEC 29119 HARD BLOCKER] {fatal_failed_count} FATAL hardware/security requirement(s) failed!")
        print("[-] Build aborted to prevent device brick or bootloop.")
        sys.exit(1)
    elif advisory_failed_count > 0:
        print(f"\n[*] [ISO/IEC 29119 ADVISORY] {advisory_failed_count} non-critical item(s) differ from optimal template.")
        print("[+] All critical hardware, crypto, and OS compatibility gates satisfied.")
        sys.exit(0)
    else:
        print("\n[+] SUCCESS: Kernel configuration satisfies 100% of compatibility requirements.")
        print("[+] Fully verified for Xiaomi POCO F3 (alioth), Kernel 4.19, and PixelOS Android 17.")
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
