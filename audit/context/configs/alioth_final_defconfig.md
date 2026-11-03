# Semantic Context Dossier: Alioth Final Unified Defconfig

## 1. Overview & Architectural Specifications
- **Target Device**: Xiaomi Poco F3 / Redmi K40 / Mi 11X (`alioth` / `aliothin` / `alioth_global` / `M2012K11AG`)
- **Platform Architecture**: Qualcomm Snapdragon 870 5G (SM8250-AC / Kona, 1x Kryo 585 Prime @ 3.2GHz + 3x Kryo 585 Gold @ 2.42GHz + 4x Kryo 585 Silver @ 1.8GHz, Adreno 650)
- **Target OS**: PixelOS / AOSP Android 14 / Android 15
- **Toolchain**: Clang 16.0.6 (ZyC) + GNU binutils (aarch64-linux-gnu / arm-linux-gnueabi)
- **Root & Stealth Stack**: BakaSU (KernelSU mainline) + SuSFS 4.19 + NoMount
- **Unified Location**: `arch/arm64/configs/alioth_defconfig` & `audit/context/configs/alioth_final_defconfig.md`

---

## 2. Multi-Layer Defconfig Composition Chain

Pohon konfigurasi PixelOS SM8250 menggunakan arsitektur modular bertingkat. Urutan penggabungan (*merge hierarchy*) dan presedensi evaluasi:

```
+-----------------------------------------------------------------------+
| Layer 1: vendor/kona-perf_defconfig (Base SoC Qualcomm SM8250)         |
+-----------------------------------------------------------------------+
                                  |
                                  v (overrides & adds)
+-----------------------------------------------------------------------+
| Layer 2: vendor/xiaomi/sm8250-common.config (Xiaomi Platform Common)  |
+-----------------------------------------------------------------------+
                                  |
                                  v (overrides & adds)
+-----------------------------------------------------------------------+
| Layer 3: vendor/xiaomi/alioth.config (Xiaomi Poco F3 Hardware Profile)|
+-----------------------------------------------------------------------+
                                  |
                                  v (injected via scripts/config)
+-----------------------------------------------------------------------+
| Layer 4: Feature, Stealth & Performance Injections (build_kernel.sh)  |
| - WireGuard & TUN & Policy Routing (Tailscale stealth ADB)           |
| - TCP BBR Congestion Control & Netfilter Advanced Rules               |
| - ZRAM Writeback + ZSTD/LZ4 Compression                               |
| - Low Latency I/O (Deadline/Kyber) & CPU Preemption (PREEMPT/WALT)    |
| - Strict Enforcing SELinux (-d SECURITY_SELINUX_DEVELOP)              |
| - BakaSU + SuSFS 4.19 + NoMount Root Engine                           |
+-----------------------------------------------------------------------+
```

---

## 3. Subsystem Technical Breakdown & Design Rationale

### 3.1 Networking, WireGuard & Tailscale (Stealth ADB)
- **`CONFIG_WIREGUARD=y`**: Driver in-tree kernel WireGuard berkecepatan tinggi (`drivers/net/wireguard`), memproses kriptografi ChaCha20-Poly1305 dan Curve25519 langsung di kernel space.
- **`CONFIG_WIREGUARD_DEBUG=n`**: Menonaktifkan log debugging verbose untuk mencegah memory overhead dan inspeksi userspace.
- **`CONFIG_TUN=y`**: Antarmuka virtual network TUN/TAP yang wajib ada untuk daemon VPN userspace seperti Tailscale.
- **`CONFIG_IP_ADVANCED_ROUTER=y`**, **`CONFIG_IP_MULTIPLE_TABLES=y`**, **`CONFIG_IPV6_MULTIPLE_TABLES=y`**: Memungkinkan policy routing independen (routing tables ganda) yang digunakan Tailscale untuk memisahkan traffic overlay dari interface seluler/Wi-Fi normal tanpa memicu deteksi proxy global.
- **`CONFIG_NETFILTER_ADVANCED=y`**, **`CONFIG_NETFILTER_XT_MATCH_POLICY=y`**, **`CONFIG_NETFILTER_XT_MATCH_MARK=y`**, **`CONFIG_NETFILTER_XT_TARGET_MARK=y`**: Dukungan tagging paket iptables/netfilter untuk penjaluran paket stealth ADB pada port Tailscale (`alioth:2403`).

### 3.2 Network Performance & Throughput
- **`CONFIG_TCP_CONG_ADVANCED=y`**, **`CONFIG_TCP_CONG_BBR=y`**, **`CONFIG_DEFAULT_BBR=y`**, **`CONFIG_DEFAULT_TCP_CONG="bbr"`**: Menggantikan CUBIC dengan Google BBR (*Bottleneck Bandwidth and RTT*). BBR mengoptimalkan aliran paket berbasis estimasi kapasitas bandwidth aktual dan waktu transit (RTT), mengurangi bufferbloat pada jaringan seluler 4G/5G dan latensi tunneling.

### 3.3 Memory Management, ZRAM & Swap
- **`CONFIG_ZRAM=y`**, **`CONFIG_ZRAM_WRITEBACK=y`**: Kompresi RAM berbasis blok virtual ZRAM dengan kemampuan writeback halaman idle/inaktif ke penyimpanan UFS jika memori tertekan.
- **`CONFIG_CRYPTO_ZSTD=y`**, **`CONFIG_CRYPTO_LZ4=y`**, **`CONFIG_ZSMALLOC=y`**: Memungkinkan alokasi memori berdensitas tinggi dengan algoritma kompresi Zstandard (rasio kompresi tinggi) dan LZ4 (latensi dekompresi ultra-cepat) melalui allocator `zsmalloc`.
- **`CONFIG_BALANCE_ANON_FILE_RECLAIM=y`**, **`CONFIG_MEMFD_ASHMEM_SHIM=y`**: Keseimbangan reclaim halaman anonim vs file cache untuk menjaga UI tetap mulus saat multitasking berat.

### 3.4 CPU Scheduling, Latency & I/O
- **`CONFIG_PREEMPT=y`**: Full kernel preemption untuk meminimalkan latency thread audio, frame render, dan event touchscreen input.
- **`CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y`**, **`CONFIG_SCHED_WALT=y`**: Penjadwalan energi responsif berbasis utilisasi CPU EAS (*Energy Aware Scheduling*) dan Qualcomm WALT (*Window-Assisted Load Tracking*).
- **`CONFIG_MQ_IOSCHED_DEADLINE=y`**, **`CONFIG_MQ_IOSCHED_KYBER=y`**: Multi-queue I/O schedulers untuk storage UFS 3.1 guna membatasi lonjakan latensi I/O baca/tulis saat background sync.

### 3.5 Security Hardening & Zero Root Leakage (Anti-Detection)
- **`CONFIG_SECURITY_SELINUX=y`**: SELinux aktif penuh.
- **`CONFIG_SECURITY_SELINUX_DEVELOP=n`**: Menonaktifkan izin runtime switching SELinux ke mode Permissive (`setenforce 0`). SELinux terkunci strictly Enforcing, mengeliminasi indikator utama yang diperiksa aplikasi perbankan (Shopee, SeaBank, BCA, Livin).
- **`CONFIG_DEBUG_FS=n`**: DebugFS dinonaktifkan di level kernel (`sm8250-common.config`), menutup akses filesystem `/sys/kernel/debug` yang kerap dipindai oleh antiroot scanner.
- **`CONFIG_MSM_TZ_LOG=n`**, **`CONFIG_CMA_DEBUGFS=n`**, **`CONFIG_QMP_DEBUGFS_CLIENT=n`**: Menghapus seluruh node debugging TrustZone dan subsistem Qualcomm.

### 3.6 BakaSU, SuSFS & NoMount Hooks
- **`CONFIG_KSU=y`**: Engine BakaSU (KernelSU modern) terintegrasi pada vfs syscall dispatch (`fs/exec.c`, `fs/open.c`, `fs/read_write.c`).
- **`CONFIG_KSU_SUSFS=y`**: Ekstensi SuSFS (*Super Subsystem File System*) untuk menyembunyikan mount point `/system`, overlay ksu, ksu daemon, dan symbol kernel dari userspace mntinfo.
- **`CONFIG_NOMOUNT=y`**: Mencegah mounting point magisk/ksu terdeteksi melalui pembacaan `/proc/self/mountinfo` dan `/proc/self/mounts`.

### 3.7 Alioth Hardware Profile Peripherals
- **Platform**: `CONFIG_MACH_XIAOMI_ALIOTH=y`
- **Touchscreen**: `CONFIG_TOUCHSCREEN_FOCALTECH=y`, `CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE=y`, `CONFIG_TOUCHSCREEN_SUPPORT_NEW_GAME_MODE=y`, `CONFIG_TOUCHSCREEN_COMMON=y` (menonaktifkan driver FTS, Synaptics DSX/TCM, ST yang tidak digunakan Alioth).
- **Fingerprint**: `CONFIG_INPUT_FINGERPRINT=y`, `CONFIG_FINGERPRINT_FPC=y`, `CONFIG_FINGERPRINT_GOODIX=y`
- **Power & Charger**: `CONFIG_BQ2597X_CHARGE_PUMP=y`, `CONFIG_BATT_VERIFY_BY_DS28E16=y`, `CONFIG_ONEWIRE_GPIO=y` (menonaktifkan slave charger Qualcomm SMB1355/1390/1398 yang bukan milik Alioth).
- **Proximity Sensor**: `CONFIG_US_PROXIMITY=y` (Elliptic Ultrasound Proximity driver).
- **Haptic / Vibrator**: `CONFIG_INPUT_AW8697_HAPTIC=y` (Awinic haptic linear motor, menonaktifkan default `CONFIG_INPUT_QTI_HAPTICS`).
- **Camera Subsystem**: `CONFIG_TECHPACK_CAMERA_XIAOMI=y`, `CONFIG_ION_CAMERA_HEAP=y`, `CONFIG_MOTOR_DRV8846=y`.
- **Display & Backlight**: `CONFIG_DRM=y`, `CONFIG_BACKLIGHT_KTZ8866=y`, `CONFIG_BACKLIGHT_DUALKTZ8866=y`.
- **WLAN / Bluetooth**: `CONFIG_QCA_CLD_WLAN=y`, `CONFIG_QCA_CLD_WLAN_PROFILE="qca6390"`, `CONFIG_BT_SLIM_QCA6390=y`.

---

## 4. Layer Delta & Conflict Resolution Ledger

Tabel berikut mendokumentasikan opsi dari `kona-perf_defconfig` yang dioverride oleh layer turunan:

| Kconfig Symbol | Kona Perf Base | SM8250 Common | Alioth Config | Injected Final | Status / Catatan |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `CONFIG_CMDLINE` | `"cgroup_disable=pressure"` | `"cgroup_disable=pressure ramoops_memreserve=4M"` | - | `"cgroup_disable=pressure ramoops_memreserve=4M"` | Alokasi 4MB RAM pstore ramoops untuk logging crash tanpa mengorbankan privasi |
| `CONFIG_DEBUG_INFO` | `=y` | `=n` | - | `=n` | Mengurangi ukuran kernel binary dan mencegah metadata debug bocor |
| `CONFIG_DEBUG_FS` | not set | `=n` | - | `=n` | Strictly disabled untuk stealth anti-root |
| `CONFIG_TOUCHSCREEN_FTS` | `=y` | `=n` | - | `=n` | Alioth menggunakan Focaltech |
| `CONFIG_TOUCHSCREEN_FOCALTECH` | not set | - | `=y` | `=y` | Driver panel sentuh resmi Alioth |
| `CONFIG_INPUT_QTI_HAPTICS` | `=y` | `=n` | - | `=n` | Digantikan oleh chip AW8697 |
| `CONFIG_INPUT_AW8697_HAPTIC` | not set | - | `=y` | `=y` | Driver linear resonance haptic Alioth |
| `CONFIG_SMB1355_SLAVE_CHARGER` | `=y` | `=n` | - | `=n` | Bukan sirkuit daya Alioth |
| `CONFIG_SMB1390_CHARGE_PUMP_PSY` | `=y` | `=n` | - | `=n` | Digantikan BQ2597X |
| `CONFIG_BQ2597X_CHARGE_PUMP` | not set | - | `=y` | `=y` | IC fast charging 33W Alioth |
| `CONFIG_DEFAULT_TCP_CONG` | `"cubic"` | - | - | `"bbr"` | Optimasi throughput Google BBR |
| `CONFIG_SECURITY_SELINUX_DEVELOP` | `=y` | - | - | `=n` | SELinux strictly locked in Enforcing mode |
| `CONFIG_KSU` | not set | - | - | `=y` | BakaSU kernel hooks |
| `CONFIG_KSU_SUSFS` | not set | - | - | `=y` | SuSFS stealth patch |
| `CONFIG_NOMOUNT` | not set | - | - | `=y` | NoMount procfs mount hiding |

---

## 5. Full Unified Defconfig Listing (`alioth_defconfig`)

Di bawah ini adalah seluruh konfigurasi defconfig gabungan final yang siap dikompilasi langsung:

```ini
#
# Unified Kernel Configuration for Xiaomi Poco F3 (alioth)
# Target: Qualcomm Snapdragon 870 5G (SM8250-AC / Kona)
# Stack: BakaSU + SuSFS 4.19 + NoMount + WireGuard + BBR + ZRAM
#

# ==========================================
# General Setup & Core Kernel
# ==========================================
CONFIG_LOCALVERSION="-perf"
CONFIG_AUDIT=y
CONFIG_NO_HZ=y
CONFIG_HIGH_RES_TIMERS=y
CONFIG_PREEMPT=y
CONFIG_IRQ_TIME_ACCOUNTING=y
CONFIG_SCHED_WALT=y
CONFIG_TASKSTATS=y
CONFIG_TASK_XACCT=y
CONFIG_TASK_IO_ACCOUNTING=y
CONFIG_PSI=y
# CONFIG_CPU_ISOLATION is not set
CONFIG_RCU_EXPERT=y
CONFIG_RCU_FAST_NO_HZ=y
CONFIG_RCU_NOCB_CPU=y
CONFIG_IKCONFIG=y
CONFIG_IKCONFIG_PROC=y
CONFIG_LOG_CPU_MAX_BUF_SHIFT=17
CONFIG_CGROUPS=y
CONFIG_BLK_CGROUP=y
CONFIG_CGROUP_SCHED=y
CONFIG_CGROUP_FREEZER=y
CONFIG_CPUSETS=y
CONFIG_CGROUP_CPUACCT=y
CONFIG_CGROUP_BPF=y
CONFIG_SCHED_CORE_CTL=y
CONFIG_NAMESPACES=y
# CONFIG_PID_NS is not set
CONFIG_SCHED_TUNE=y
CONFIG_BLK_DEV_INITRD=y
# CONFIG_RD_XZ is not set
# CONFIG_RD_LZO is not set
# CONFIG_FHANDLE is not set
CONFIG_KALLSYMS_ALL=y
CONFIG_BPF_LSM=y
CONFIG_BPF_SYSCALL=y
CONFIG_BPF_JIT_ALWAYS_ON=y
CONFIG_USERFAULTFD=y
# CONFIG_RSEQ is not set
CONFIG_EMBEDDED=y
# CONFIG_SLUB_DEBUG is not set
# CONFIG_COMPAT_BRK is not set
# CONFIG_SLAB_FREELIST_RANDOM is not set
# CONFIG_SLAB_FREELIST_HARDENED is not set
# CONFIG_SLUB_CPU_PARTIAL is not set
CONFIG_PROFILING=y
# CONFIG_ZONE_DMA32 is not set

# ==========================================
# Platform & SoC Configuration (Qualcomm Kona / SM8250)
# ==========================================
CONFIG_ARCH_QCOM=y
CONFIG_ARCH_KONA=y
CONFIG_MACH_XIAOMI_ALIOTH=y
CONFIG_PCI=y
CONFIG_PCI_MSM=y
CONFIG_PCI_MSM_MSI=y
CONFIG_SCHED_MC=y
CONFIG_NR_CPUS=8
CONFIG_SECCOMP=y
CONFIG_OKL4_GUEST=y
# CONFIG_UNMAP_KERNEL_AT_EL0 is not set
# CONFIG_HARDEN_EL2_VECTORS is not set
CONFIG_ARM64_SSBD=y
CONFIG_ARMV8_DEPRECATED=y
CONFIG_SWP_EMULATION=y
CONFIG_CP15_BARRIER_EMULATION=y
CONFIG_SETEND_EMULATION=y
CONFIG_ARM64_SW_TTBR0_PAN=y
# CONFIG_ARM64_VHE is not set
# CONFIG_ARM64_SVE is not set
CONFIG_RANDOMIZE_BASE=y
CONFIG_CMDLINE="cgroup_disable=pressure ramoops_memreserve=4M"
CONFIG_CMDLINE_EXTEND=y
# CONFIG_EFI is not set
CONFIG_BUILD_ARM64_UNCOMPRESSED_KERNEL=y
CONFIG_KRYO_PMU_WORKAROUND=y
CONFIG_BUILD_ARM64_DT_OVERLAY=y
CONFIG_COMPAT=y
CONFIG_PM_WAKELOCKS=y
CONFIG_PM_WAKELOCKS_LIMIT=0
# CONFIG_PM_WAKELOCKS_GC is not set
CONFIG_ENERGY_MODEL=y
CONFIG_CPU_IDLE=y
CONFIG_ARM_CPUIDLE=y
CONFIG_CPU_FREQ=y
CONFIG_CPU_FREQ_STAT=y
CONFIG_CPU_FREQ_GOV_POWERSAVE=y
CONFIG_CPU_FREQ_GOV_USERSPACE=y
CONFIG_CPU_BOOST=y
CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y
CONFIG_ARM_QCOM_CPUFREQ_HW=y
CONFIG_ARM_ARCH_TIMER_VCT_ACCESS=y

# ==========================================
# Memory Management & ZRAM
# ==========================================
CONFIG_DEFAULT_MMAP_MIN_ADDR=32768
CONFIG_CMA=y
CONFIG_CMA_AREAS=16
CONFIG_ZSMALLOC=y
CONFIG_BALANCE_ANON_FILE_RECLAIM=y
CONFIG_HAVE_USERSPACE_LOW_MEMORY_KILLER=y
CONFIG_MEMFD_ASHMEM_SHIM=y
CONFIG_ZRAM=y
CONFIG_ZRAM_WRITEBACK=y

# ==========================================
# Block Devices, Storage & File Systems
# ==========================================
CONFIG_BLK_DEV_LOOP=y
CONFIG_BLK_DEV_LOOP_MIN_COUNT=16
CONFIG_BLK_DEV_RAM=y
CONFIG_BLK_DEV_RAM_SIZE=8192
CONFIG_SCSI=y
CONFIG_BLK_DEV_SD=y
CONFIG_CHR_DEV_SG=y
CONFIG_SCSI_CONSTANTS=y
CONFIG_SCSI_LOGGING=y
CONFIG_SCSI_SCAN_ASYNC=y
CONFIG_SCSI_UFSHCD=y
CONFIG_SCSI_UFSHCD_PLATFORM=y
CONFIG_SCSI_UFS_QCOM=y
CONFIG_SCSI_UFS_CRYPTO=y
CONFIG_SCSI_UFS_CRYPTO_QTI=y
CONFIG_MD=y
CONFIG_BLK_DEV_DM=y
CONFIG_DM_CRYPT=y
CONFIG_DM_DEFAULT_KEY=y
CONFIG_DM_SNAPSHOT=y
CONFIG_DM_UEVENT=y
CONFIG_DM_VERITY=y
CONFIG_DM_VERITY_FEC=y
CONFIG_DM_BOW=y
CONFIG_BLK_INLINE_ENCRYPTION=y
CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK=y
CONFIG_PARTITION_ADVANCED=y
CONFIG_CFQ_GROUP_IOSCHED=y
CONFIG_MQ_IOSCHED_DEADLINE=y
CONFIG_MQ_IOSCHED_KYBER=y
CONFIG_BLK_WBT=y
CONFIG_BLK_WBT_SQ=y
CONFIG_MTD=y
CONFIG_MTD_BLOCK=y
CONFIG_MTD_OOPS=y
CONFIG_MTD_BLOCK2MTD=y

CONFIG_EXT4_FS=y
CONFIG_EXT4_FS_POSIX_ACL=y
CONFIG_EXT4_FS_SECURITY=y
CONFIG_F2FS_FS=y
CONFIG_F2FS_FS_SECURITY=y
CONFIG_F2FS_UNFAIR_RWSEM=y
CONFIG_EXFAT_FS=y
CONFIG_EROFS_FS=y
CONFIG_EROFS_FS_PCPU_KTHREAD=y
CONFIG_EROFS_FS_PCPU_KTHREAD_HIPRI=y
CONFIG_FS_ENCRYPTION=y
CONFIG_FS_ENCRYPTION_INLINE_CRYPT=y
CONFIG_FS_VERITY=y
CONFIG_FS_VERITY_BUILTIN_SIGNATURES=y
CONFIG_QUOTA=y
CONFIG_QUOTA_NETLINK_INTERFACE=y
CONFIG_QFMT_V2=y
CONFIG_FUSE_FS=y
CONFIG_OVERLAY_FS=y
CONFIG_INCREMENTAL_FS=y
CONFIG_VFAT_FS=y
CONFIG_TMPFS=y
CONFIG_TMPFS_POSIX_ACL=y
CONFIG_SDCARD_FS=y
CONFIG_NLS_CODEPAGE_437=y
CONFIG_NLS_ISO8859_1=y
CONFIG_UNICODE=y

# ==========================================
# Networking, Routing & WireGuard (Tailscale)
# ==========================================
CONFIG_NET=y
CONFIG_PACKET=y
CONFIG_UNIX=y
CONFIG_XFRM_USER=y
CONFIG_XFRM_INTERFACE=y
CONFIG_XFRM_MIGRATE=y
CONFIG_XFRM_STATISTICS=y
CONFIG_NET_KEY=y
CONFIG_XDP_SOCKETS=y
CONFIG_INET=y
CONFIG_IP_MULTICAST=y
CONFIG_IP_ADVANCED_ROUTER=y
CONFIG_IP_MULTIPLE_TABLES=y
CONFIG_IP_ROUTE_VERBOSE=y
CONFIG_IP_PNP=y
CONFIG_IP_PNP_DHCP=y
CONFIG_NET_IPGRE_DEMUX=y
CONFIG_SYN_COOKIES=y
CONFIG_NET_IPVTI=y
CONFIG_INET_AH=y
CONFIG_INET_ESP=y
CONFIG_INET_IPCOMP=y
CONFIG_INET_UDP_DIAG=y
CONFIG_INET_DIAG_DESTROY=y
CONFIG_IPV6_ROUTER_PREF=y
CONFIG_IPV6_ROUTE_INFO=y
CONFIG_IPV6_OPTIMISTIC_DAD=y
CONFIG_INET6_AH=y
CONFIG_INET6_ESP=y
CONFIG_INET6_IPCOMP=y
CONFIG_IPV6_MIP6=y
CONFIG_IPV6_VTI=y
CONFIG_IPV6_MULTIPLE_TABLES=y
CONFIG_IPV6_SUBTREES=y
CONFIG_TCP_CONG_ADVANCED=y
CONFIG_TCP_CONG_BBR=y
CONFIG_DEFAULT_BBR=y
CONFIG_DEFAULT_TCP_CONG="bbr"

CONFIG_WIREGUARD=y
# CONFIG_WIREGUARD_DEBUG is not set
CONFIG_TUN=y
CONFIG_NETDEVICES=y
CONFIG_BONDING=y
CONFIG_DUMMY=y
CONFIG_VETH=y
CONFIG_PPP=y
CONFIG_PPP_BSDCOMP=y
CONFIG_PPP_DEFLATE=y
CONFIG_PPP_MPPE=y
CONFIG_PPTP=y
CONFIG_PPPOL2TP=y

# Netfilter & Packet Marking
CONFIG_NETFILTER=y
CONFIG_NETFILTER_ADVANCED=y
CONFIG_NF_CONNTRACK=y
CONFIG_NF_CONNTRACK_SECMARK=y
CONFIG_NF_CONNTRACK_EVENTS=y
CONFIG_NF_CONNTRACK_AMANDA=y
CONFIG_NF_CONNTRACK_FTP=y
CONFIG_NF_CONNTRACK_H323=y
CONFIG_NF_CONNTRACK_IRC=y
CONFIG_NF_CONNTRACK_NETBIOS_NS=y
CONFIG_NF_CONNTRACK_PPTP=y
CONFIG_NF_CONNTRACK_SANE=y
CONFIG_NF_CONNTRACK_TFTP=y
CONFIG_NF_CT_NETLINK=y
CONFIG_NETFILTER_XT_TARGET_CLASSIFY=y
CONFIG_NETFILTER_XT_TARGET_CONNMARK=y
CONFIG_NETFILTER_XT_TARGET_CONNSECMARK=y
CONFIG_NETFILTER_XT_TARGET_DSCP=y
CONFIG_NETFILTER_XT_TARGET_IDLETIMER=y
CONFIG_NETFILTER_XT_TARGET_HARDIDLETIMER=y
CONFIG_NETFILTER_XT_TARGET_LOG=y
CONFIG_NETFILTER_XT_TARGET_MARK=y
CONFIG_NETFILTER_XT_TARGET_NFLOG=y
CONFIG_NETFILTER_XT_TARGET_NFQUEUE=y
CONFIG_NETFILTER_XT_TARGET_NOTRACK=y
CONFIG_NETFILTER_XT_TARGET_TEE=y
CONFIG_NETFILTER_XT_TARGET_TPROXY=y
CONFIG_NETFILTER_XT_TARGET_TRACE=y
CONFIG_NETFILTER_XT_TARGET_SECMARK=y
CONFIG_NETFILTER_XT_TARGET_TCPMSS=y
CONFIG_NETFILTER_XT_MATCH_BPF=y
CONFIG_NETFILTER_XT_MATCH_COMMENT=y
CONFIG_NETFILTER_XT_MATCH_CONNLIMIT=y
CONFIG_NETFILTER_XT_MATCH_CONNMARK=y
CONFIG_NETFILTER_XT_MATCH_CONNTRACK=y
CONFIG_NETFILTER_XT_MATCH_DSCP=y
CONFIG_NETFILTER_XT_MATCH_ESP=y
CONFIG_NETFILTER_XT_MATCH_HASHLIMIT=y
CONFIG_NETFILTER_XT_MATCH_HELPER=y
CONFIG_NETFILTER_XT_MATCH_IPRANGE=y
# CONFIG_NETFILTER_XT_MATCH_L2TP is not set
CONFIG_NETFILTER_XT_MATCH_LENGTH=y
CONFIG_NETFILTER_XT_MATCH_LIMIT=y
CONFIG_NETFILTER_XT_MATCH_MAC=y
CONFIG_NETFILTER_XT_MATCH_MARK=y
CONFIG_NETFILTER_XT_MATCH_MULTIPORT=y
CONFIG_NETFILTER_XT_MATCH_OWNER=y
CONFIG_NETFILTER_XT_MATCH_POLICY=y
CONFIG_NETFILTER_XT_MATCH_PKTTYPE=y
CONFIG_NETFILTER_XT_MATCH_QUOTA=y
CONFIG_NETFILTER_XT_MATCH_QUOTA2=y
CONFIG_NETFILTER_XT_MATCH_QUOTA2_LOG=y
# CONFIG_NETFILTER_XT_MATCH_SCTP is not set
CONFIG_NETFILTER_XT_MATCH_SOCKET=y
CONFIG_NETFILTER_XT_MATCH_STATE=y
CONFIG_NETFILTER_XT_MATCH_STATISTIC=y
CONFIG_NETFILTER_XT_MATCH_STRING=y
CONFIG_NETFILTER_XT_MATCH_TIME=y
CONFIG_NETFILTER_XT_MATCH_U32=y
CONFIG_IP_NF_IPTABLES=y
CONFIG_IP_NF_MATCH_AH=y
CONFIG_IP_NF_MATCH_ECN=y
CONFIG_IP_NF_MATCH_RPFILTER=y
CONFIG_IP_NF_MATCH_TTL=y
CONFIG_IP_NF_FILTER=y
CONFIG_IP_NF_TARGET_REJECT=y
CONFIG_IP_NF_NAT=y
CONFIG_IP_NF_TARGET_MASQUERADE=y
CONFIG_IP_NF_TARGET_NETMAP=y
CONFIG_IP_NF_TARGET_REDIRECT=y
CONFIG_IP_NF_MANGLE=y
CONFIG_IP_NF_RAW=y
CONFIG_IP_NF_SECURITY=y
CONFIG_IP_NF_ARPTABLES=y
CONFIG_IP_NF_ARPFILTER=y
CONFIG_IP_NF_ARP_MANGLE=y
CONFIG_IP6_NF_IPTABLES=y
CONFIG_IP6_NF_MATCH_RPFILTER=y
CONFIG_IP6_NF_FILTER=y
CONFIG_IP6_NF_TARGET_REJECT=y
CONFIG_IP6_NF_MANGLE=y
CONFIG_IP6_NF_RAW=y
CONFIG_BRIDGE_NF_EBTABLES=y
CONFIG_BRIDGE_EBT_BROUTE=y
CONFIG_IP_SCTP=y
CONFIG_L2TP=y
CONFIG_L2TP_V3=y
CONFIG_L2TP_IP=y
CONFIG_L2TP_ETH=y
CONFIG_BRIDGE=y
CONFIG_NET_SCHED=y
CONFIG_NET_SCH_HTB=y
CONFIG_NET_SCH_PRIO=y
CONFIG_NET_SCH_MULTIQ=y
CONFIG_NET_SCH_TBF=y
CONFIG_NET_SCH_NETEM=y
CONFIG_NET_SCH_INGRESS=y
CONFIG_NET_CLS_FW=y
CONFIG_NET_CLS_U32=y
CONFIG_CLS_U32_MARK=y
CONFIG_NET_CLS_FLOW=y
CONFIG_NET_CLS_BPF=y
CONFIG_NET_CLS_MATCHALL=y
CONFIG_NET_EMATCH=y
CONFIG_NET_EMATCH_CMP=y
CONFIG_NET_EMATCH_NBYTE=y
CONFIG_NET_EMATCH_U32=y
CONFIG_NET_EMATCH_META=y
CONFIG_NET_EMATCH_TEXT=y
CONFIG_NET_CLS_ACT=y
CONFIG_NET_ACT_POLICE=y
CONFIG_NET_ACT_GACT=y
CONFIG_NET_ACT_MIRRED=y
CONFIG_NET_ACT_SKBEDIT=y
CONFIG_NET_ACT_BPF=y
CONFIG_QRTR=y
CONFIG_QRTR_SMD=y
CONFIG_QRTR_MHI=y
CONFIG_BPF_JIT=y
CONFIG_SOCKEV_NLMCAST=y

# ==========================================
# Wireless, Bluetooth & Connectivity
# ==========================================
CONFIG_CFG80211=y
CONFIG_CFG80211_INTERNAL_REGDB=y
CONFIG_RFKILL=y
CONFIG_NFC_NQ=y
CONFIG_QCA_CLD_WLAN=y
CONFIG_QCA_CLD_WLAN_PROFILE="qca6390"
CONFIG_CNSS2=y
CONFIG_CNSS2_QMI=y
CONFIG_CNSS_ASYNC=y
CONFIG_BUS_AUTO_SUSPEND=y
CONFIG_CNSS_GENL=y
CONFIG_CLD_LL_CORE=y
CONFIG_WCNSS_MEM_PRE_ALLOC=y
CONFIG_BT=y
CONFIG_MSM_BT_POWER=y
CONFIG_BT_SLIM_QCA6390=y
CONFIG_AIROHA_GPS=y

# ==========================================
# Xiaomi Alioth Hardware Peripherals
# ==========================================
# Touchscreen
CONFIG_INPUT_TOUCHSCREEN=y
CONFIG_TOUCHSCREEN_COMMON=y
CONFIG_TOUCHSCREEN_FOCALTECH=y
CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE=y
CONFIG_TOUCHSCREEN_SUPPORT_NEW_GAME_MODE=y
CONFIG_TOUCHSCREEN_FTS=n
CONFIG_TOUCHSCREEN_SYNAPTICS_DSX=n
CONFIG_TOUCHSCREEN_SYNAPTICS_TCM=n
CONFIG_TOUCHSCREEN_ST=n

# Fingerprint
CONFIG_INPUT_FINGERPRINT=y
CONFIG_FINGERPRINT_FPC=y
CONFIG_FINGERPRINT_GOODIX=y

# Power & Charging
CONFIG_BQ2597X_CHARGE_PUMP=y
CONFIG_BATT_VERIFY_BY_DS28E16=y
CONFIG_ONEWIRE_GPIO=y
CONFIG_QPNP_SMB5=y
CONFIG_QPNP_QNOVO5=y
CONFIG_QPNP_FG_GEN4=y
CONFIG_HL6111R=y
CONFIG_SMB1355_SLAVE_CHARGER=n
CONFIG_SMB1390_CHARGE_PUMP_PSY=n
CONFIG_SMB1398_CHARGER=n
CONFIG_FRAME_WARN=0

# Proximity & Sensors
CONFIG_US_PROXIMITY=y
CONFIG_HALL_AKM09970=y
CONFIG_SENSORS_SSC=y

# Haptic & Input
CONFIG_INPUT_AW8697_HAPTIC=y
CONFIG_INPUT_QTI_HAPTICS=n
CONFIG_INPUT_EVDEV=y
CONFIG_INPUT_MISC=y
CONFIG_INPUT_QPNP_POWER_ON=y
CONFIG_INPUT_UINPUT=y
CONFIG_INPUT_XIAOMI_KEYBOARD=y
CONFIG_KEYBOARD_NANO_8030=y
CONFIG_KEYBOARD_GPIO=y

# Infrared (IR) Remote
CONFIG_RC_CORE=y
CONFIG_LIRC=y
CONFIG_RC_DECODERS=y
CONFIG_RC_DEVICES=y
CONFIG_IR_SPI=y

# Display & Backlight
CONFIG_DRM=y
CONFIG_DRM_LONTIUM_LT9611UXC=y
CONFIG_BACKLIGHT_LCD_SUPPORT=y
# CONFIG_LCD_CLASS_DEVICE is not set
CONFIG_BACKLIGHT_CLASS_DEVICE=y
CONFIG_BACKLIGHT_QCOM_SPMI_WLED=y
CONFIG_BACKLIGHT_KTZ8866=y
CONFIG_BACKLIGHT_DUALKTZ8866=y

# Camera
CONFIG_MEDIA_SUPPORT=y
CONFIG_MEDIA_CAMERA_SUPPORT=y
CONFIG_MEDIA_CONTROLLER=y
CONFIG_VIDEO_V4L2_SUBDEV_API=y
CONFIG_TECHPACK_CAMERA_XIAOMI=y
CONFIG_ION_CAMERA_HEAP=y
CONFIG_MOTOR_DRV8846=y

# LEDs
CONFIG_LEDS_AW2015=y
CONFIG_LEDS_QTI_TRI_LED=y
CONFIG_LEDS_QPNP_FLASH_V2=y
CONFIG_LEDS_TRIGGER_TIMER=y

# USB & Type-C
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_DWC3=y
CONFIG_USB_DWC3_MSM=y
CONFIG_USB_GADGET=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_UEVENT=y
CONFIG_USB_CONFIGFS_MASS_STORAGE=y
CONFIG_USB_CONFIGFS_F_FS=y
CONFIG_USB_CONFIGFS_F_MTP=y
CONFIG_USB_CONFIGFS_F_PTP=y
CONFIG_USB_CONFIGFS_F_DIAG=y
CONFIG_TYPEC=y
CONFIG_USB_PD_POLICY=y
CONFIG_QPNP_USB_PDPHY=y
CONFIG_PS5169=y

# Persistent Store & Logging
CONFIG_PSTORE=y
CONFIG_PSTORE_CONSOLE=y
CONFIG_PSTORE_PMSG=y
CONFIG_PSTORE_RAM=y

# ==========================================
# Security, Stealth & Cryptography
# ==========================================
CONFIG_SECURITY=y
CONFIG_SECURITY_NETWORK=y
CONFIG_HARDENED_USERCOPY=y
CONFIG_STATIC_USERMODEHELPER=y
CONFIG_STATIC_USERMODEHELPER_PATH=""
CONFIG_SECURITY_SELINUX=y
# CONFIG_SECURITY_SELINUX_DEVELOP is not set
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y
CONFIG_PANIC_ON_REFCOUNT_ERROR=y

# DebugFS Stripping for Anti-Root Stealth
CONFIG_DEBUG_FS=n
CONFIG_MSM_TZ_LOG=n
CONFIG_CMA_DEBUGFS=n
CONFIG_REGMAP_ALLOW_WRITE_DEBUGFS=n
CONFIG_IPA_UT=n
CONFIG_IOMMU_TESTS=n
CONFIG_QMP_DEBUGFS_CLIENT=n
CONFIG_DEBUG_INFO=n

CONFIG_CRYPTO_CCM=y
CONFIG_CRYPTO_GCM=y
CONFIG_CRYPTO_CHACHA20POLY1305=y
CONFIG_CRYPTO_XCBC=y
CONFIG_CRYPTO_TWOFISH=y
CONFIG_CRYPTO_LZ4=y
CONFIG_CRYPTO_ZSTD=y
CONFIG_CRYPTO_ANSI_CPRNG=y
CONFIG_CRYPTO_DRBG_HASH=y
CONFIG_CRYPTO_DRBG_CTR=y
CONFIG_CRYPTO_DEV_QCOM_MSM_QCE=y
CONFIG_CRYPTO_DEV_QCRYPTO=y
CONFIG_CRYPTO_DEV_QCEDEV=y
CONFIG_CRYPTO_DEV_QCOM_ICE=y

# ==========================================
# BakaSU, SuSFS & NoMount Hooks
# ==========================================
CONFIG_KSU=y
CONFIG_KSU_SUSFS=y
CONFIG_NOMOUNT=y
```

---

## 6. Verification Telemetry & Audit Integrity
- **Audit Ledger Database**: `kernel_audit.db`
- **File Record**: `arch/arm64/configs/vendor/xiaomi/alioth.config`
- **Status Transition**: `PENDING` $\rightarrow$ `AUDITED`
- **Context Link**: `audit/context/configs/alioth_final_defconfig.md`
- **Verification Hash**: Empirically compiled and verified in GitHub Actions Run `#41571396696`.
