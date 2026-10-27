# Semantic Context Dossier: Anti-EDL Panic Recovery Protection & USB Force Fast Charge

## 1. Subsystem & Component Identification
- **Subsystem**: Power Management / Reset Architecture & USB Charging
- **Files Involved**:
  - `drivers/power/reset/msm-poweroff.c`
  - `drivers/misc/fastchg.c`
  - `include/linux/fastchg.h`
  - `drivers/misc/Kconfig`
  - `drivers/misc/Makefile`
  - `drivers/power/supply/qcom/smb5-lib.c`
  - `drivers/power/supply/qcom_cas/smb5-lib.c`
  - `arch/arm64/configs/vendor/kona-perf_defconfig`
- **Target Platform**: Xiaomi POCO F3 / Redmi K40 (`alioth`, Qualcomm Snapdragon 870 / SM8250), Android 17 (SDK 37), Linux Kernel 4.19.

## 2. Architectural Role & Functional Purpose
- **Anti-EDL Panic Recovery Protection (`msm-poweroff.c`)**:
  - Pada arsitektur Qualcomm MSM baseline, kondisi *kernel panic* secara default mengaktifkan *emergency download mode* (`dload_mode`), yang menyebabkan perangkat masuk ke mode EDL 9008 (layar mati/black screen) dan memerlukan intervensi USB host khusus.
  - Implementasi ini memutus rantai `dload_mode` saat terjadi panic (`in_panic`), dan secara otomatis menetapkan restart reason ke `PON_RESTART_REASON_RECOVERY` dengan magic register `0x77665502`.
  - Dampak: Jika terjadi *kernel panic*, perangkat akan langsung reboot ke recovery (TWRP/OrangeFox), memungkinkan operator memeriksa log `pstore/console-ramoops` dan mencegah *hard lock/brick*.
- **USB Force Fast Charge (`fastchg.c` & `smb5-lib.c`)**:
  - Standar USB SDP (Standard Downstream Port) membatasi penarikan arus input hanya hingga 500mA (`USBIN_500MA`).
  - Driver `fastchg` menyediakan sysfs switch `/sys/kernel/fast_charge/force_fast_charge` dan parameter bootloader `ffc=1`.
  - Saat diaktifkan, batasan input SDP dinaikkan ke 900mA (`USBIN_900MA`), mempercepat pengisian daya secara aman saat terhubung ke port USB komputer/laptop tanpa memicu trip overcurrent.

## 3. Configuration Matrix
```kconfig
CONFIG_FORCE_FAST_CHARGE=y
```

## 4. Empirical Verification Status
- Status Kode C: Terverifikasi dan terintegrasi bersih.
- Status Defconfig: Terkonfigurasi aktif di `kona-perf_defconfig`.
- Audit Status: **AUDITED**.
