# Context Dossier: drivers/gpu/msm/kgsl_gmu.c

## Functional Role
Graphics Management Unit (GMU) power control, DCVS frequency transitions, and performance voting.

## Modifications & Rationale
- `gmu_dcvs_set()`: Added retry handling on `-ETIMEDOUT` for `H2F_MSG_GX_BW_PERF_VOTE`.
- Under heavy bus traffic, a transient delay no longer immediately trips `ADRENO_GMU_FAULT` or triggers an intrusive snapshot, preventing graphics fence hangs.
