// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
#include <linux/susfs.h>
#endif

static int version_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	struct new_utsname tmp;
	memcpy(&tmp, utsname(), sizeof(tmp));
	susfs_spoof_uname(&tmp);
	seq_printf(m, linux_proc_banner,
		tmp.sysname,
		tmp.release,
		tmp.version);
	return 0;
#else
	seq_printf(m, linux_proc_banner,
		utsname()->sysname,
		utsname()->release,
		utsname()->version);
	return 0;
#endif
}

static int __init proc_version_init(void)
{
	proc_create_single("version", 0, NULL, version_proc_show);
	return 0;
}
fs_initcall(proc_version_init);
