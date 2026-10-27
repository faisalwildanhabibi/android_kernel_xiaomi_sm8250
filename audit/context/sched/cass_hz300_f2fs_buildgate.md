# Context Dossier: CASS Scheduler, CONFIG_HZ=300, F2FS Rapid GC & Discard, and Strict Zero-Divergence Build Gate

## Subsystem Architecture
- **Components**:
  - `kernel/sched/cass.c`, `kernel/sched/fair.c`, `init/Kconfig`
  - `arch/arm64/configs/vendor/kona-perf_defconfig`
  - `fs/f2fs/f2fs.h`, `fs/f2fs/gc.h`
  - `build_kernel.sh`
- **Architectural Scope**:
  1. **Capacity Aware Superset Scheduler (CASS)**:
     - Authored by Sultan Alsawaf.
     - Replaces CFS `select_task_rq_fair` with capacity-relative utilization hierarchy balancing (`cass_select_task_rq_fair`).
     - Avoids utilization clipping when CPUs are overutilized, optimizing multi-core performance, latency, and cache locality.
     - Controlled via `CONFIG_SCHED_CASS`.
  2. **Timer Frequency (CONFIG_HZ=300)**:
     - Configures timer interrupt frequency to 300Hz (3.33ms per tick).
     - Directly synchronizes with 120Hz display refresh rate (2.5 ticks/frame) and 60Hz display refresh rate (5.0 ticks/frame).
     - Eliminates fractional tick jitter and reduces frame drops in touch event delivery.
  3. **F2FS Rapid GC & Discard Optimization**:
     - `DEF_MAX_DISCARD_REQUEST` increased from 64 to 128 for higher batch discard throughput.
     - `DEF_MIN_DISCARD_ISSUE_TIME` decreased from 50ms to 10ms for lower discard latency.
     - `DEF_GC_THREAD_MIN_SLEEP_TIME` reduced from 30s to 10s and `DEF_GC_THREAD_MAX_SLEEP_TIME` from 60s to 30s for prompt idle segment reclamation on UFS 3.1.
  4. **Strict Zero-Divergence Build Gate**:
     - Enforces `set -eo pipefail` across the compilation pipeline.
     - Pre-flight config assertion verifying that all critical symbols (`CONFIG_TCP_CONG_BBR`, `CONFIG_DEFAULT_BBR`, `CONFIG_NET_SCH_FQ`, `CONFIG_FORCE_FAST_CHARGE`, `CONFIG_HZ_300`, `CONFIG_SCHED_CASS`, `CONFIG_CRYPTO_ZSTD`, `CONFIG_KSU`, `CONFIG_KSU_SUSFS`) are physically present and active in `.config`.
     - Post-flight AnyKernel3 zip integrity testing (`zip -T`) ensuring zero corrupted packages.
