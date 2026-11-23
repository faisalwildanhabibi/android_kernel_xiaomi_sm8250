# Context Dossier: sm8250-common.config

## Architectural Purpose
Platform-level modular defconfig overlay for Qualcomm SM8250 (kona) devices on PixelOS.

## Modifications
- Added `CONFIG_DMABUF_SYSFS_STATS=y` under dedicated `# DMA-BUF Sysfs Statistics` section.
- Provides standard kernel sysfs statistics in `/sys/kernel/dmabuf/buffers` without requiring dangerous eBPF iterators.
