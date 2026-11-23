# Context Dossier: drivers/gpu/msm/kgsl.c

## Functional Role
Top-level KGSL driver core, lifecycle management, and global workqueues.

## Modifications & Rationale
- `kgsl_core_init()`: Appended `WQ_HIGHPRI` flag to `kgsl_driver.workqueue`.
- Ensures GPU event callbacks, sync fence signaling, and power state scaling execute on elevated kernel workers, preventing display compositor pipeline stalls.
