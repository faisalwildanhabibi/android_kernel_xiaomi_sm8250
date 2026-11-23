# Context Dossier: drivers/input/touchscreen/focaltech_spi/focaltech_core.c

## Functional Role
Driver core for FocalTech SPI touchscreen controller (`fts_ts`) on Xiaomi POCO F3 (`alioth`).

## Modifications & Rationale
- Replaced `create_singlethread_workqueue("fts_wq")` with `alloc_ordered_workqueue("fts_wq", WQ_HIGHPRI | WQ_MEM_RECLAIM)`.
- Elevates the workqueue processing touch coordinate events to high-priority kernel workers (`kworker/*H`).
- Eliminates touch event dropping and starvation during high CPU pressure, kswapd thrashing, and memory compaction.
