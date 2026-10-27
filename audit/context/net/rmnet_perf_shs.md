# Context Dossier: Qualcomm RMNET Performance Acceleration & Software Hash Steering (SHS)

## Subsystem Architecture
- **Components**:
  - `techpack/data/drivers/rmnet/perf/`
  - `techpack/data/drivers/rmnet/shs/`
  - `arch/arm64/configs/vendor/kona-perf_defconfig`
  - `build_kernel.sh`
- **Architectural Scope**:
  1. **RMNET Performance (`CONFIG_RMNET_PERF=y`)**:
     - Qualcomm proprietary cellular hardware packet aggregation and checksum offload driver for `rmnet_data` interfaces.
     - Reduces CPU packet processing overhead during high-bandwidth 5G and LTE-A downloads.
  2. **RMNET Software Hash Steering (`CONFIG_RMNET_SHS=y`)**:
     - Implements dynamic per-flow software hash steering across the 8 Kryo CPU cores.
     - Distributes incoming modem network packets across CPU cores to prevent softirq hotspots and bottlenecking on a single CPU core during multi-threaded downloads.
  3. **Zero-Divergence Gate Verification**:
     - Verified present and strictly asserted in `build_kernel.sh`.
