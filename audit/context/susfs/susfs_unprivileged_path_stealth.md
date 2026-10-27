# Context Dossier: SuSFS & BakaSU Comprehensive Gating & Stealth Reinforcement

## 1. Functional Role & Architectural Purpose
This module enhancement unifies and hardens the permission gating and symbol resolution layers across SuSFS and BakaSU. It eliminates symbol leakage via procfs (`/proc/[pid]/wchan`, stack traces), harmonizes statx/getattr gating for unprivileged processes, and ensures that root-allowed managers maintain full diagnostic visibility while untrusted processes are subject to comprehensive stealth.

## 2. Technical Mechanism & Call Flow
1. **Unprivileged Non-SU Gating (`susfs_is_current_proc_unprivileged_non_su`)**:
   - `fs/susfs.c` (`susfs_is_inode_sus_path`): Dynamically hides any inode registered by userspace ReSuSFS from all unprivileged non-su processes (`uid >= 10000 || TIF_PROC_NO_SU`) without requiring `TIF_PROC_UMOUNTED`.
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
