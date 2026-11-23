# Subsystem Audit State Ledger

| File Path | Subsystem | Target Changes | Status | Context Dossier |
| :--- | :--- | :--- | :---: | :--- |
| `arch/arm64/configs/vendor/xiaomi/sm8250-common.config` | Configs | Enable `CONFIG_DMABUF_SYSFS_STATS=y`, ensure clean section segregation | AUDITED | `audit/context/configs/sm8250-common.config.md` |
| `arch/arm64/configs/vendor/kona-perf_defconfig` | Configs | Enable `CONFIG_DMABUF_SYSFS_STATS=y` | AUDITED | `audit/context/configs/kona-perf_defconfig.md` |
| `mm/page_alloc.c` | Memory Management | Elevate `watermark_scale_factor` to 150; enforce 32MB floor for `min_free_kbytes` | AUDITED | `audit/context/mm/page_alloc.c.md` |
| `drivers/input/touchscreen/focaltech_spi/focaltech_core.c` | Input | Elevate `fts_wq` to `alloc_ordered_workqueue(..., WQ_HIGHPRI \| WQ_MEM_RECLAIM)` | AUDITED | `audit/context/input/focaltech_core.c.md` |
| `drivers/gpu/msm/kgsl_hfi.h` | GPU / KGSL | Extend `HFI_RSP_TIMEOUT` from 100ms to 300ms | AUDITED | `audit/context/kgsl/kgsl_hfi.h.md` |
| `drivers/gpu/msm/kgsl_gmu.c` | GPU / KGSL | Add retry loop on `H2F_MSG_GX_BW_PERF_VOTE` before declaring GMU fault | AUDITED | `audit/context/kgsl/kgsl_gmu.c.md` |
| `drivers/gpu/msm/kgsl.c` | GPU / KGSL | Elevate `kgsl_driver.workqueue` to `WQ_HIGHPRI` | AUDITED | `audit/context/kgsl/kgsl.c.md` |
| `drivers/block/zram/zram_drv.c` | Block / ZRAM | Switch driver `default_compressor` from `lzo-rle` to `zstd` | AUDITED | `audit/context/block/zram_drv.c.md` |
