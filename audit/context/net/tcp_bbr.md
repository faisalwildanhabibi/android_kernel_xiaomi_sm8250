# Semantic Context Dossier: TCP BBR Congestion Control & Fair Queueing (FQ)

## 1. Subsystem & Component Identification
- **Subsystem**: Networking / TCP Congestion Control & Packet Schedulers
- **Files Involved**:
  - `net/ipv4/tcp_bbr.c`
  - `net/sched/sch_fq.c`
  - `net/sched/sch_cake.c`
  - `net/sched/sch_fq_codel.c`
  - `arch/arm64/configs/vendor/kona-perf_defconfig`
- **Target Platform**: Xiaomi POCO F3 / Redmi K40 (`alioth`, Qualcomm Snapdragon 870 / SM8250), Android 17 (SDK 37), Linux Kernel 4.19.

## 2. Architectural Role & Functional Purpose
- **TCP BBR (Bottleneck Bandwidth and RTT)**:
  - Algoritma kontrol kongesti berbasis model eksplisit yang mengestimasi delivery rate maksimum (bottleneck bandwidth) dan round-trip time propagasi dua arah minimum ($RT_{prop}$).
  - Mengeliminasi bufferbloat secara drastis dibanding algoritma konvensional berbasis packet loss (seperti CUBIC atau Reno) dengan membatasi volume data dalam antrian (*in-flight data*) mendekati nilai *Bandwidth-Delay Product* (BDP).
  - Tahan terhadap packet loss acak pada jaringan seluler (LTE/5G) dan Wi-Fi dengan fluktuasi sinyal tinggi.
- **Fair Queueing (FQ / sch_fq)**:
  - Packet scheduler yang mengimplementasikan pemisahan flow (flow hashing), pacing rate enforcement per-soket (`sk_pacing_rate`), serta model Earliest Departure Time (EDT / `tcp_wstamp_ns`).
  - Menghilangkan artefak kuantum pada koneksi low-rate dan membatasi fluktuasi jitter P99 latency.

## 3. Structural Mechanics & Data Structures
- **BBR Context (`struct bbr`)**:
  - `min_rtt_us`: Sampel propagasi RTT terendah dalam jendela waktu sliding window.
  - `bw`: Filter nilai bandwidth bottleneck delivery rate tertinggi.
  - `pacing_gain` & `cwnd_gain`: Koefisien penguatan laju transmisi dan batas inflight tergantung pada fase siklus (STARTUP, DRAIN, PROBE_BW, PROBE_RTT).
  - `extra_acked`: Estimasi ACK aggregation windowed-max filter untuk menjaga throughput pada jaringan dengan ACK decimation / aggregation.
- **FQ Scheduling (`struct fq_sched_data`)**:
  - Red-black tree (`rb_root`) berbasis departure time nanodetik (`skb->tstamp`).
  - Mendukung offload dan software pacing untuk koneksi TCP multi-gigabit.

## 4. Configuration Matrix
```kconfig
CONFIG_TCP_CONG_ADVANCED=y
CONFIG_TCP_CONG_BBR=y
CONFIG_DEFAULT_BBR=y
CONFIG_DEFAULT_TCP_CONG="bbr"
CONFIG_NET_SCH_FQ=y
CONFIG_NET_SCH_FQ_CODEL=y
CONFIG_NET_SCH_CAKE=y
CONFIG_CRYPTO_ZSTD=y
```

## 5. Empirical Verification Status
- Status Kode C: Terverifikasi 100% lengkap di kernel source tree (`net/ipv4/tcp_bbr.c` dan `net/sched/sch_fq.c`).
- Status Defconfig: Terkonfigurasi aktif di `kona-perf_defconfig`.
- Audit Status: **AUDITED**.
