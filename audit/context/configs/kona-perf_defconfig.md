# Context Dossier: kona-perf_defconfig

## Architectural Purpose
Base vendor performance defconfig for Qualcomm Snapdragon 870 / 865 (SM8250 / kona).

## Modifications
- Added `CONFIG_DMABUF_SYSFS_STATS=y` alongside ION memory configurations.
- Exposes per-buffer inode sysfs attributes to satisfy Android 13/14 `system_server` memory tracking.
