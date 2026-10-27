# Context Dossier: SuSFS Native Unprivileged Path Stealth & Static Path Registration

## 1. Functional Role & Architectural Purpose
This module enhancement establishes persistent, native stealth at the Linux VFS layer against advanced anti-root heuristic detectors (e.g., LIAPP, RootBeer, Play Integrity detectors). It closes exposure windows where untrusted user applications (`uid >= 10000`) probe root markers (`/data/adb`, `/dev/socket/zygote_next`, `/system/bin/su`) before userspace daemon configuration runs, or when unprivileged app threads run without `TIF_PROC_UMOUNTED`.

## 2. Technical Mechanism & Call Flow
1. **Unprivileged Non-SU Enforcement (`susfs_is_inode_sus_path`)**:
   - Replaced `susfs_is_current_proc_umounted_app()` with `susfs_is_current_proc_unprivileged_non_su()`.
   - Any untrusted process (`uid >= 10000` or carrying `TIF_PROC_NO_SU`) lacking BakaSU root grant will unconditionally receive `-ENOENT` and have directory entries pruned from `readdir` / `getdents64`.
   - Allowed root apps (`susfs_is_current_app_allowed_su()`) and system-level processes (`uid < 10000`) retain full, unrestricted access to all nodes and sockets.

2. **Native Default SUS Paths Registration (`LH_SUS_PATH_LOOP`)**:
   - Introduced `susfs_add_default_sus_path_loop()` and `susfs_init_default_sus_paths()` in `fs/susfs.c`.
   - Statically registers essential root paths at kernel boot:
     * `/data/adb`
     * `/data/adb/ksu`
     * `/data/adb/modules`
     * `/data/adb/modules_update`
     * `/data/adb/post-fs-data.d`
     * `/data/adb/service.d`
     * `/data/adb/boot-completed.d`
     * `/data/adb/lspd`
     * `/dev/socket/zygote_next`
     * `/dev/socket/usap_pool_primary`
     * `/dev/socket/usap_pool_secondary`
     * `/system/bin/su`
     * `/system/xbin/su`
   - Dynamically flags inodes with `AS_FLAGS_SUS_PATH` via `susfs_run_sus_path_loop()` at boot completion / dynamic worker stages.

3. **Heuristic Fast-Path Hiding (`susfs_is_sus_su_path`)**:
   - Extended `susfs_is_sus_su_path()` in `include/linux/susfs_def.h` to immediately intercept direct `stat()`, `faccessat()`, and `open()` calls on `/data/adb` and Zygote Next sockets without requiring inode traversal.
