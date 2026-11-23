# Context Dossier: drivers/block/zram/zram_drv.c

## Functional Role
ZRAM compressed RAM block device driver for swap storage.

## Modifications & Rationale
- `default_compressor`: Changed from `lzo-rle` to `zstd`.
- `zstd` offers superior compression ratio while maintaining high decompression speeds on ARM64 Kryo 585 cores, reducing memory consumption and preventing ZRAM physical memory bloating under heavy multitasking.
