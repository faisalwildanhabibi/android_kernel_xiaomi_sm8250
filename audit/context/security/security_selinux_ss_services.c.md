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
      if (strstr(scontext, ":ksu") || strstr(scontext, ":magisk") || strstr(scontext, ":zygisk")) {
          return -EINVAL;
      }
  }
  #endif
  ```
- **Mekanisme**: Mencegat probing keberadaan domain root oleh aplikasi ruang pengguna tanpa izin superuser.

### Catatan Keamanan Arsitektur Core SELinux
- Fungsi konversi internal inti `security_sid_to_context_core()` sengaja dibiarkan murni (*untouched*) tanpa injeksi gating userspace. Pengecekan atribut proses root (`/proc/[pid]/attr/current`) dipindahkan secara aman ke lapisan VFS procfs (`fs/proc/base.c:proc_pid_attr_read`) untuk mengeliminasi risiko dereferensi struktur belum terinisialisasi pada fase *early boot* kernel.

## 4. Kamus Terminologi Khusus
- **`Context-to-SID Resolution`**: Resolusi nama string keamanan pengguna ke integer 32-bit yang digunakan oleh tabel tabel AVC kernel.
- **`Domain Existence Probing`**: Teknik pengintaian keamanan pengguna di mana aplikasi menguji apakah kernel mengenali nama domain terlarang untuk menyimpulkan keberadaan framework root.
