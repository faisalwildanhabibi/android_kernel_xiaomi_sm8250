# Context Dossier: drivers/gpu/msm/kgsl_hfi.h

## Functional Role
Hardware Fuse Interface (HFI) definitions for Host-to-GMU (H2F) and GMU-to-Host (G2H) message passing on Adreno 6xx.

## Modifications & Rationale
- `HFI_RSP_TIMEOUT`: Increased from `100` ms to `300` ms.
- Prevents spurious timeout failures (`-110 ETIMEDOUT`) when the GMU Cortex-M4 microcontroller experiences delayed responses during memory bus saturation under high 3D rendering loads.
