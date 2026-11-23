# Context Dossier: mm/page_alloc.c

## Functional Role
Core memory allocator, zone watermark calculation, and buddy system management.

## Modifications & Rationale
- `watermark_scale_factor`: Increased default from `10` (0.1% of zone) to `150` (1.5% of zone).
  - On 8 GB LPDDR5, this expands the headroom between `WMARK_MIN` and `WMARK_LOW` from ~8 MB to ~120 MB.
  - Gives `kswapd0` sufficient time to reclaim memory in the background before userspace applications and `system_server` cross the min watermark into direct reclaim / compaction stalls.
- `init_per_zone_wmark_min()`: Enforced a 32768 kB (32 MB) floor for `min_free_kbytes`.
  - Prevents atomic and interrupt context allocations from starving under heavy 3D graphics buffer churn.
