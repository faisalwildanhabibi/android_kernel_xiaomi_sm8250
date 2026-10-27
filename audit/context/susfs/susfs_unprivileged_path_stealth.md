# Context Dossier: SuSFS & BakaSU Comprehensive Gating & Stealth Reinforcement

## 1. Functional Role & Architectural Purpose
This module enhancement unifies and hardens the permission gating and symbol resolution layers across SuSFS and BakaSU. It eliminates symbol leakage via procfs (`/proc/[pid]/wchan`, stack traces), harmonizes statx/getattr gating for unprivileged processes, and ensures that root-allowed managers maintain full diagnostic visibility while untrusted processes are subject to comprehensive stealth.

## 2. Technical Mechanism & Call Flow
1. **Unprivileged Non-SU Gating (`susfs_is_current_proc_unprivileged_non_su`)**:
   - `include/linux/susfs_def.h`: Enhanced with strict priority bypass for `susfs_is_current_app_allowed_su()`, followed by union of `susfs_is_current_proc_umounted_app()`, `susfs_is_current_app_uid()`, and `susfs_is_current_proc_no_su()`.
   - `fs/susfs.c` (`susfs_is_inode_sus_path` & open redirect schemes 3/4): Harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `fs/notify/fdinfo.c`: Inotify and mount fdinfo spoofing harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `fs/proc/fd.c`: `/proc/[pid]/fd` directory enumeration and symlink resolution harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `security/selinux/ss/services.c`: SELinux context to SID spoofing harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `security/selinux/selinuxfs.c`: `/sys/fs/selinux/enforce` read spoofing harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `kernel/seccomp.c`: Seccomp logging suppression and prctl mode spoofing harmonized to `susfs_is_current_proc_unprivileged_non_su()`.
   - `fs/stat.c` (`generic_fillattr`): Harmonized to check `susfs_is_current_proc_unprivileged_non_su()`.
   - `fs/susfs.c` (`susfs_is_sus_map_file`): Exempts BakaSU root-allowed processes (`susfs_is_current_app_allowed_su()`), allowing root management tools full introspection of their own memory mappings.

2. **Kallsyms & Wchan Leak Prevention (`kernel/kallsyms.c`)**:
   - Extended `susfs_is_hidden_kallsym()` into `kallsyms_lookup()` and `lookup_symbol_name()`.
   - Prevents KSU and SuSFS internal symbols from being leaked via `/proc/[pid]/wchan` (which now outputs `0`), stack unwinding, or kernel formatted addresses `%pS`.
   - Leaves normal kernel symbols completely intact.

3. **Userspace Module Harmony (ReSuSFS)**:
   - Zero hardcoded path strings in kernel source code.
   - All paths are managed dynamically through ReSuSFS `sus_paths.txt` and `sus_paths_loop.txt`.
   - Early execution via `scripts_postfs.txt` registers all paths during the `post-fs-data` phase.
