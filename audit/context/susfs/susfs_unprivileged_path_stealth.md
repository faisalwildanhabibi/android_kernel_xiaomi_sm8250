# Context Dossier: SuSFS Generic Unprivileged Non-SU Gating

## 1. Functional Role & Architectural Purpose
This enhancement strengthens kernel-level stealth gating without introducing hardcoded path strings into the kernel source tree. The kernel delegates all target paths to userspace management (via ReSuSFS `sus_paths.txt` and `sus_paths_loop.txt`), while the kernel enforces generic permission gating ensuring that any path registered by userspace is strictly concealed from all unprivileged non-su processes.

## 2. Technical Mechanism & Call Flow
1. **Generic Unprivileged Gating (`susfs_is_inode_sus_path`)**:
   - Replaced `susfs_is_current_proc_umounted_app()` with `susfs_is_current_proc_unprivileged_non_su()`.
   - Previously, path hiding strictly depended on `TIF_PROC_UMOUNTED` (`susfs_is_current_proc_umounted_app`), which failed to conceal registered paths from unprivileged processes or isolated threads lacking the thread flag.
   - Under the enhanced check, any process with `(uid >= 10000 || TIF_PROC_NO_SU) && !susfs_is_current_app_allowed_su()` will have all registered `AS_FLAGS_SUS_PATH` inodes hidden with `-ENOENT` and omitted from `readdir` / `getdents64`.
   - Root-granted processes (`susfs_is_current_app_allowed_su()`) and system domains (`uid < 10000`) retain full, unrestricted VFS access.

2. **Decoupled Architecture & Userspace Dynamic Management**:
   - Zero hardcoded path constants in kernel code.
   - Path definitions remain strictly dynamic in ReSuSFS userspace configuration files (`sus_paths.txt`, `sus_paths_loop.txt`).
   - ReSuSFS triggers kernel registration via standard `CMD_SUSFS_ADD_SUS_PATH` and `CMD_SUSFS_ADD_SUS_PATH_LOOP` ioctl interfaces.
