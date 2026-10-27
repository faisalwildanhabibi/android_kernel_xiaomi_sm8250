# Context Dossier: `security/selinux/ss/services.c`

## 1. Peran Fungsional & Arsitektur
`security/selinux/ss/services.c` adalah mesin logika utama Security Server (SS) pada subsistem SELinux kernel Linux. Berkas ini mengelola basis data kebijakan keamanan di memori (`policydb`), melakukan konversi antara nama konteks teks (seperti `u:r:untrusted_app:s0`) dan nomor identifikasi keamanan kernel (Security Identifier / SID), mengevaluasi aturan transisi domain, serta menentukan keputusan akses (*access decisions* via `context_struct_compute_av`).

Pada kernel SM8250 ini, `services.c` dimodifikasi secara mendalam oleh **SuSFS (Suspicious File System)** untuk menerapkan **Domain Existence Concealment**: menolak keberadaan domain-domain root sensitif ketika diminta oleh aplikasi ruang pengguna.

## 2. Struktur Data & Variabel Kunci
- `struct selinux_state`: Objek status global yang menampung pointer kebijakan keamanan (`ss->policydb`), cache status, dan konfigurasi.
- `security_context_to_sid()`: Fungsi penerjemah dari string konteks keamanan teks ke nomor SID biner.
- `security_sid_to_context()`: Fungsi penerjemah sebaliknya dari nomor SID biner ke string teks.
- `CONFIG_KSU_SUSFS`: Konfigurasi pengaktifan hook penyembunyian domain SuSFS.

## 3. Dekomposisi Fungsi & Titik Hook SuSFS
### Hook 1: Domain Probing Cloaking (`security_context_to_sid`)
- Di dalam pemanggilan `security_context_to_sid()`:
  ```c
  #ifdef CONFIG_KSU_SUSFS
  if (scontext && scontext_len > 0 && susfs_is_current_proc_unprivileged_non_su()) {
      if (strstr(scontext, ":su:") || strstr(scontext, ":ksu") ||
          strstr(scontext, ":magisk") || strstr(scontext, ":zygisk") ||
          strstr(scontext, "zygote_next")) {
          return -EINVAL;
      }
  }
  #endif
  ```
- **Mekanisme**: Salah satu teknik deteksi root paling canggih yang digunakan oleh modul proteksi aplikasi perbankan dan game anti-cheat adalah memanggil fungsi native `selinux_check_access` atau `security_context_to_sid()` dengan string konteks root khusus (misal `"u:r:su:s0"`, `"u:r:magisk:s0"`, `"u:r:ksu:s0"`, atau `"u:r:zygisk:s0"`). Jika kernel mengembalikan SID valid (sukses), aplikasi menyimpulkan bahwa perangkat telah di-root karena domain tersebut tidak ada di ROM standar.
- **Efek SuSFS**: Jika pemanggil adalah aplikasi biasa tanpa izin superuser (`susfs_is_current_proc_unprivileged_non_su()`), SuSFS langsung mengembalikan error `-EINVAL` (Invalid Argument) seolah-olah domain tersebut tidak pernah didefinisikan di dalam kebijakan SELinux perangkat, mematahkan deteksi probing konteks secara sempurna.

### Hook 2: Reverse SID-to-Context Cloaking (`security_sid_to_context_core`)
- Di dalam pemanggilan `security_sid_to_context_core()`:
  ```c
  #ifdef CONFIG_KSU_SUSFS
  if (!rc && scontext && *scontext && susfs_is_current_proc_unprivileged_non_su()) {
      if ((susfs_ksu_sid && sid == susfs_ksu_sid) ||
          strstr(*scontext, ":su:") ||
          strstr(*scontext, ":ksu") ||
          strstr(*scontext, ":magisk") ||
          strstr(*scontext, ":zygisk") ||
          strstr(*scontext, "zygote_next")) {
          kfree(*scontext);
          *scontext = NULL;
          *scontext_len = 0;
          rc = -EACCES;
      }
  }
  #endif
  ```
- **Mekanisme**: Ketika aplikasi non-root menginspeksi file context (`lgetfilecon()`, `getfilecon()`, `getxattr`) atau membaca atribut SELinux proses lain (`/proc/[pid]/attr/current`), fungsi ini memetakan SID ke representasi string konteks teks.
- **Efek SuSFS**: Mencegah kebocoran konteks root (`u:r:su:s0`, `u:object_r:zygote_next_socket:s0`, dsb.) ke aplikasi unprivileged dengan mengembalikan `-EACCES`, menyembunyikan identitas root secara total sementara membiarkan akses penuh bagi manajer BakaSU dan proses root berwenang.

## 4. Kamus Terminologi Khusus
- **`Context-to-SID Resolution`**: Resolusi nama string keamanan pengguna ke integer 32-bit yang digunakan oleh tabel tabel AVC kernel.
- **`Domain Existence Probing`**: Teknik pengintaian keamanan pengguna di mana aplikasi menguji apakah kernel mengenali nama domain terlarang untuk menyimpulkan keberadaan framework root.
