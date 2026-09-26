// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
extern struct static_key_false susfs_is_fake_cmdline_or_bootconfig_buffer_set;
extern void susfs_spoof_cmdline_or_bootconfig(struct seq_file *m);
#endif

static int cmdline_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	if (static_branch_likely(&susfs_is_fake_cmdline_or_bootconfig_buffer_set)) {
		susfs_spoof_cmdline_or_bootconfig(m);
		seq_putc(m, '\n');
		return 0;
	}
#endif
	/* Hardened kernel fallback: sanitize bootloader unlocked indicators */
	if (saved_command_line) {
		const char *src = saved_command_line;
		const char *p;
		while (*src) {
			if (!strncmp(src, "androidboot.verifiedbootstate=orange", 36)) {
				seq_puts(m, "androidboot.verifiedbootstate=green");
				src += 36;
			} else if (!strncmp(src, "androidboot.flash.locked=0", 26)) {
				seq_puts(m, "androidboot.flash.locked=1");
				src += 26;
			} else {
				p = strpbrk(src, " \t\n");
				if (p) {
					seq_write(m, src, p - src);
					seq_putc(m, *p);
					src = p + 1;
				} else {
					seq_puts(m, src);
					break;
				}
			}
		}
	}
	seq_putc(m, '\n');
	return 0;
}

static int __init proc_cmdline_init(void)
{
	proc_create_single("cmdline", 0, NULL, cmdline_proc_show);
	return 0;
}
fs_initcall(proc_cmdline_init);