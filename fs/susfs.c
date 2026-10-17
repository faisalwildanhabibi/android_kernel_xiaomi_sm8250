#include <linux/version.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/namei.h>
#include <linux/list.h>
#include <linux/init_task.h>
#include <linux/mutex.h>
#include <linux/seqlock.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/fdtable.h>
#include <linux/statfs.h>
#include <linux/random.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/fsnotify_backend.h>
#include <linux/jump_label.h>
#include <linux/version.h> // We need check kernel version.
#include <linux/security.h>
#include <linux/susfs.h>
#include "fuse/fuse_i.h"
#include "mount.h"

extern bool susfs_is_current_ksu_domain(void);
extern void setup_selinux(const char *domain, struct cred *cred);
extern struct cred *ksu_cred;
extern int susfs_get_non_sus_mnt_id_from_mnt(struct mount *orig_mnt);
extern struct vfsmount *susfs_get_non_sus_vfsmnt_from_vfsmnt(struct vfsmount *vfsmnt);

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
DEFINE_STATIC_KEY_TRUE(susfs_is_log_enabled);
#define SUSFS_LOGI(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#else
#define SUSFS_LOGI(fmt, ...) 
#define SUSFS_LOGE(fmt, ...) 
#endif

/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
DEFINE_STATIC_SRCU(susfs_srcu_sus_path_loop);
static DEFINE_MUTEX(susfs_mutex_lock_sus_path);
static LIST_HEAD(LH_SUS_PATH_LOOP);
const struct qstr susfs_fake_qstr_name = QSTR_INIT("..5.u.S", 7); // used to re-test the dcache lookup, make sure you don't have file named like this!!

void susfs_add_sus_path(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct path path;
	struct inode *inode = NULL;
	struct fuse_inode *fi = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (info.err) {
		SUSFS_LOGE("failed opening file '%s'\n", info.target_pathname);
		goto out_copy_to_user;
	}

	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) {
		SUSFS_LOGE("inode || inode->i_mapping is NULL\n");
		info.err = -ENOENT;
		goto out_path_put_path;
	}

	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) {
			SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
			info.err = -ENOENT;
			goto out_path_put_path;
		}
		set_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_state);
		set_bit(AS_FLAGS_SUS_PATH, &inode->i_state);
		SUSFS_LOGI("flagged AS_FLAGS_SUS_PATH on pathname: '%s', fi->nodeid: %llu, fi->inode.i_ino: %lu, fi->inode.i_state: 0x%lx\n",
					info.target_pathname, fi->nodeid, fi->inode.i_ino, fi->inode.i_state);
		info.err = 0;
		goto out_path_put_path;
	}

	set_bit(AS_FLAGS_SUS_PATH, &inode->i_state);
	SUSFS_LOGI("flagged AS_FLAGS_SUS_PATH on pathname: '%s', ino: '%lu', inode->i_state: 0x%lx\n",
				info.target_pathname, inode->i_ino, inode->i_state);
	info.err = 0;
out_path_put_path:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH -> ret: %d\n", info.err);
}

void susfs_add_sus_path_loop(void __user **user_info) {
	struct st_susfs_sus_path_list *new_list = NULL;
	struct st_susfs_sus_path info = {0};

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info.target_pathname == '\0') {
		SUSFS_LOGE("target_pathname cannot be empty\n");
		info.err = -EINVAL;
		goto out_copy_to_user;
	}

	new_list = kzalloc(sizeof(struct st_susfs_sus_path_list), GFP_KERNEL);
	if (!new_list) {
		info.err = -ENOMEM;
		goto out_copy_to_user;
	}
	strscpy(new_list->info.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	strscpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	INIT_LIST_HEAD(&new_list->list);
	mutex_lock(&susfs_mutex_lock_sus_path);
	list_add_tail_rcu(&new_list->list, &LH_SUS_PATH_LOOP);
	mutex_unlock(&susfs_mutex_lock_sus_path);
	SUSFS_LOGI("target_pathname: '%s', is successfully added to LH_SUS_PATH_LOOP\n", new_list->target_pathname);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH_LOOP -> ret: %d\n", info.err);
}

static void susfs_run_sus_path_loop(void) {
	struct st_susfs_sus_path_list *cursor = NULL;
	struct path path;
	struct inode *inode;
	struct fuse_inode *fi = NULL;
	const struct cred *saved = override_creds(ksu_cred);
	int srcu_idx = srcu_read_lock(&susfs_srcu_sus_path_loop);

	list_for_each_entry_rcu(cursor, &LH_SUS_PATH_LOOP, list) {
		if (!kern_path(cursor->target_pathname, 0, &path))
		{
			inode = d_backing_inode(path.dentry);
			if (!inode || !inode->i_mapping) {
				SUSFS_LOGE("inode || inode->i_mapping is NULL\n");
				path_put(&path);
				continue;
			}
			if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
				fi = get_fuse_inode(inode);
				if (!fi || !fi->inode.i_mapping) {
					SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
					path_put(&path);
					continue;
				}
				set_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_state);
				set_bit(AS_FLAGS_SUS_PATH, &inode->i_state);
				SUSFS_LOGI("re-flag AS_FLAGS_SUS_PATH on path '%s', fi->inode.i_ino: '%lu', fi->inode.i_state: 0x%lx\n",
						cursor->target_pathname, fi->inode.i_ino, fi->inode.i_state);
			} else {
				set_bit(AS_FLAGS_SUS_PATH, &inode->i_state);
				SUSFS_LOGI("re-flag AS_FLAGS_SUS_PATH on path '%s', inode->i_ino: '%lu', inode->i_state: 0x%lx\n",
						cursor->target_pathname, inode->i_ino, inode->i_state);
			}
			path_put(&path);
		}
	}
	srcu_read_unlock(&susfs_srcu_sus_path_loop, srcu_idx);
	revert_creds(saved);
}

static inline bool is_i_uid_not_allowed(uid_t i_uid) {
	return likely(current_uid().val != i_uid);
}

/* - Please note that path inside /sdcard will be still visible to MediaProvider module,
 *   since the uid of path like /sdcard/TWRP will be the uid of your MediaProvider module.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
bool susfs_is_inode_sus_path(struct mnt_idmap* idmap, struct inode *inode)
#else
bool susfs_is_inode_sus_path(struct inode *inode)
#endif
{
	struct fuse_inode *fi = NULL;
	if (!susfs_is_current_proc_umounted_app()) {
		return false;
	}
	if (!inode->i_mapping) {
		SUSFS_LOGE("inode->i_mapping is NULL\n");
		return false;
	}
	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) {
			SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
			return false;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_state) &&
			is_i_uid_not_allowed(i_uid_into_vfsuid(idmap, &fi->inode).val)))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_state) &&
			is_i_uid_not_allowed(i_uid_into_mnt(i_user_ns(&fi->inode), &fi->inode).val)))
#else
		if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_state) &&
			is_i_uid_not_allowed(fi->inode.i_uid.val)))
#endif
		{
			SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
			return true;
		}
		return false;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_state) &&
		is_i_uid_not_allowed(i_uid_into_vfsuid(idmap, inode).val)))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_state) &&
		is_i_uid_not_allowed(i_uid_into_mnt(i_user_ns(inode), inode).val)))
#else
	if (unlikely(test_bit(AS_FLAGS_SUS_PATH, &inode->i_state) &&
		is_i_uid_not_allowed(inode->i_uid.val)))
#endif
	{
		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
		return true;
	}
	return false;
}

int susfs_get_data_path(struct path *path) {
	return kern_path("/data", LOOKUP_FOLLOW, path);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
// - Default to false now so zygisk can pick up the sus mounts without the need to turn it off manually in post-fs-data stage
//   otherwise user needs to turn it on in post-fs-data stage and turn it off in boot-completed stage
DEFINE_STATIC_KEY_FALSE(susfs_is_hide_sus_mnts_for_non_su_procs_enabled);

void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info) {
	struct st_susfs_hide_sus_mnts_for_non_su_procs info = {0};

	if (copy_from_user(&info, (struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}
	
	if (info.enabled) {
		static_branch_enable(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled);
	} else {
		static_branch_disable(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled);
	}

	SUSFS_LOGI("susfs_is_hide_sus_mnts_for_non_su_procs_enabled: %d\n", static_key_enabled(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled));
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
static DEFINE_MUTEX(susfs_mutex_lock_sus_kstat);
static DEFINE_HASHTABLE(SUS_KSTAT_HLIST, 14);

extern int susfs_calculate_f_flags(struct vfsmount *mnt);

static int statfs_by_dentry(struct dentry *dentry, struct kstatfs *buf)
{
	int retval;

	if (!dentry->d_sb->s_op->statfs)
		return -ENOSYS;

	memset(buf, 0, sizeof(*buf));
	retval = security_sb_statfs(dentry);
	if (retval)
		return retval;
	retval = dentry->d_sb->s_op->statfs(dentry, buf);
	if (retval == 0 && buf->f_frsize == 0)
		buf->f_frsize = buf->f_bsize;
	return retval;
}

static int susfs_mark_inode_sus_kstat(char *target_pathname, struct st_susfs_sus_kstat_hlist *new_entry, bool is_update) {
	struct path path;
	struct inode *inode = NULL;
	struct fuse_inode *fi = NULL;
	struct vfsmount *no_sus_vfsmnt = NULL;
	int err = 0;

	err = kern_path(target_pathname, 0, &path);
	if (err) {
		SUSFS_LOGE("failed opening file '%s'\n", target_pathname);
		return err;
	}

	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) {
		SUSFS_LOGE("inode || inode->i_mapping is NULL\n");
		err = -ENOENT;
		goto out_path_put_path;
	}

	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) {
			SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
			err = -ENOENT;
			goto out_path_put_path;
		}
		if (is_update) {
			new_entry->info.spoofed_size = d_backing_inode(path.dentry)->i_size;
		} else {
			new_entry->is_fuse = true;
			new_entry->target_dev = fi->inode.i_sb->s_dev;
			new_entry->spoofed_mnt_id = susfs_get_non_sus_mnt_id_from_mnt(real_mount(path.mnt));
			no_sus_vfsmnt = susfs_get_non_sus_vfsmnt_from_vfsmnt(path.mnt);
			err = statfs_by_dentry(no_sus_vfsmnt->mnt_root, &new_entry->spoofed_kstatfs);
			if (!err)
				new_entry->spoofed_kstatfs.f_flags = susfs_calculate_f_flags(no_sus_vfsmnt);
			dput(no_sus_vfsmnt->mnt_root);
			mntput(no_sus_vfsmnt);
			if (err)
				goto out_path_put_path;
		}
		set_bit(AS_FLAGS_SUS_KSTAT, &fi->inode.i_state);
		SUSFS_LOGI("marked AS_FLAGS_SUS_KSTAT on pathname: '%s', is_fuse: %d, fi->inode.i_sb->s_dev: %u, fi->nodeid: %llu, fi->inode.i_ino: %lu, fi->inode.i_state: 0x%lx, spoofed_mnt_id: '%d'\n",
					target_pathname, new_entry->is_fuse, fi->inode.i_sb->s_dev, fi->nodeid, fi->inode.i_ino, fi->inode.i_state, new_entry->spoofed_mnt_id);
		goto out_path_put_path;
	}

	if (is_update) {
		new_entry->info.spoofed_size = d_backing_inode(path.dentry)->i_size;
	} else {
		new_entry->is_fuse = false;
		new_entry->target_dev = inode->i_sb->s_dev;
		new_entry->spoofed_mnt_id = susfs_get_non_sus_mnt_id_from_mnt(real_mount(path.mnt));
		no_sus_vfsmnt = susfs_get_non_sus_vfsmnt_from_vfsmnt(path.mnt);
		err = statfs_by_dentry(no_sus_vfsmnt->mnt_root, &new_entry->spoofed_kstatfs);
		if (!err)
			new_entry->spoofed_kstatfs.f_flags = susfs_calculate_f_flags(no_sus_vfsmnt);
		dput(no_sus_vfsmnt->mnt_root);
		mntput(no_sus_vfsmnt);
		if (err)
			goto out_path_put_path;
	}
	set_bit(AS_FLAGS_SUS_KSTAT, &inode->i_state);
	SUSFS_LOGI("marked AS_FLAGS_SUS_KSTAT on pathname: '%s', is_fuse: %d, inode->i_sb->s_dev: %u,  inode->i_ino: %lu, inode->i_state: 0x%lx, spoofed_mnt_id: '%d'\n",
				target_pathname, new_entry->is_fuse, inode->i_sb->s_dev, inode->i_ino, inode->i_state, new_entry->spoofed_mnt_id);

out_path_put_path:
	path_put(&path);
	return err;
}

void susfs_add_sus_kstat(void __user **user_info) {
	struct st_susfs_sus_kstat info = {0};
	struct st_susfs_sus_kstat_hlist *new_entry, *tmp_entry;
	struct hlist_node *tmp_hlist_node;

	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info.target_pathname == '\0') {
		info.err = -EINVAL;
		goto out_copy_to_user;
	}

	new_entry = kzalloc(sizeof(struct st_susfs_sus_kstat_hlist), GFP_KERNEL);
	if (!new_entry) {
		info.err = -ENOMEM;
		goto out_copy_to_user;
	}

#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
	info.spoofed_dev = huge_decode_dev(info.spoofed_dev);
#else
	info.spoofed_dev = old_decode_dev(info.spoofed_dev);
#endif /* defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64) */

	new_entry->target_ino = info.target_ino;
	memcpy(&new_entry->info, &info, sizeof(info));

	// statically or not, check for duplicated entry, and remove it first if so
	mutex_lock(&susfs_mutex_lock_sus_kstat);
	hash_for_each_possible_safe(SUS_KSTAT_HLIST, tmp_entry, tmp_hlist_node, node, info.target_ino) {
		if (!strcmp(tmp_entry->info.target_pathname, info.target_pathname)) {
			info.err = susfs_mark_inode_sus_kstat(new_entry->info.target_pathname, new_entry, false);
			if (info.err) {
				mutex_unlock(&susfs_mutex_lock_sus_kstat);
				kfree(new_entry);
				goto out_copy_to_user;
			}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
			SUSFS_LOGI("is_fuse: %d, is_statically: '%d', target_ino: '%lu', target_dev: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%llu', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', spoofed_mnt_id: '%d', is successfully added to SUS_KSTAT_HLIST\n",
					new_entry->is_fuse,
					new_entry->info.is_statically, new_entry->info.target_ino,
					new_entry->target_dev, new_entry->info.target_pathname,
					new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
					new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
					new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
					new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
					new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks, new_entry->spoofed_mnt_id);
#else
			SUSFS_LOGI("is_fuse: %d, is_statically: '%d', target_ino: '%lu', target_dev: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%u', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', spoofed_mnt_id: '%d', is successfully added to SUS_KSTAT_HLIST\n",
					new_entry->is_fuse,
					new_entry->info.is_statically, new_entry->info.target_ino,
					new_entry->target_dev, new_entry->info.target_pathname,
					new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
					new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
					new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
					new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
					new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks, new_entry->spoofed_mnt_id);
#endif
			hash_del_rcu(&tmp_entry->node);
			hash_add_rcu(SUS_KSTAT_HLIST, &new_entry->node, info.target_ino);
			mutex_unlock(&susfs_mutex_lock_sus_kstat);
			synchronize_rcu();
			kfree(tmp_entry);
			info.err = 0;
			goto out_copy_to_user;
		}
	}

	// if no duplicated, add it to list
	info.err = susfs_mark_inode_sus_kstat(new_entry->info.target_pathname, new_entry, false);
	if (info.err) {
		mutex_unlock(&susfs_mutex_lock_sus_kstat);
		kfree(new_entry);
		goto out_copy_to_user;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	SUSFS_LOGI("is_fuse: %d, is_statically: '%d', target_ino: '%lu', target_dev: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%llu', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', spoofed_mnt_id: '%d', is successfully added to SUS_KSTAT_HLIST\n",
			new_entry->is_fuse,
			new_entry->info.is_statically, new_entry->info.target_ino,
			new_entry->target_dev, new_entry->info.target_pathname,
			new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
			new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
			new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
			new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
			new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks, new_entry->spoofed_mnt_id);
#else
	SUSFS_LOGI("is_fuse: %d, is_statically: '%d', target_ino: '%lu', target_dev: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%u', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', spoofed_mnt_id: '%d', is successfully added to SUS_KSTAT_HLIST\n",
			new_entry->is_fuse,
			new_entry->info.is_statically, new_entry->info.target_ino,
			new_entry->target_dev, new_entry->info.target_pathname,
			new_entry->info.spoofed_ino, new_entry->info.spoofed_dev,
			new_entry->info.spoofed_nlink, new_entry->info.spoofed_size,
			new_entry->info.spoofed_atime_tv_sec, new_entry->info.spoofed_mtime_tv_sec, new_entry->info.spoofed_ctime_tv_sec,
			new_entry->info.spoofed_atime_tv_nsec, new_entry->info.spoofed_mtime_tv_nsec, new_entry->info.spoofed_ctime_tv_nsec,
			new_entry->info.spoofed_blksize, new_entry->info.spoofed_blocks, new_entry->spoofed_mnt_id);
#endif
	hash_add_rcu(SUS_KSTAT_HLIST, &new_entry->node, info.target_ino);
	mutex_unlock(&susfs_mutex_lock_sus_kstat);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	if (!info.is_statically) {
		SUSFS_LOGI("CMD_SUSFS_ADD_SUS_KSTAT -> ret: %d\n", info.err);
	} else {
		SUSFS_LOGI("CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY -> ret: %d\n", info.err);
	}
}

void susfs_update_sus_kstat(void __user **user_info) {
	struct st_susfs_sus_kstat info = {0};
	struct st_susfs_sus_kstat_hlist *new_entry, *tmp_entry;
	struct hlist_node *tmp_hlist_node;
	int bkt;

	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	new_entry = kzalloc(sizeof(struct st_susfs_sus_kstat_hlist), GFP_KERNEL);
	if (!new_entry) {
		info.err = -ENOMEM;
		goto out_copy_to_user;
	}

	// check for added entry, do the update only if entry is found.
	mutex_lock(&susfs_mutex_lock_sus_kstat);
	// for update we have to use hash_for_each_safe() since the new target inode is changed already.
	hash_for_each_safe(SUS_KSTAT_HLIST, bkt, tmp_hlist_node, tmp_entry, node) {
		if (!strcmp(tmp_entry->info.target_pathname, info.target_pathname)) {
			memcpy(&new_entry->info, &tmp_entry->info, sizeof(tmp_entry->info));
			new_entry->info.target_ino = info.target_ino;
			new_entry->target_ino = info.target_ino;
			new_entry->target_dev = tmp_entry->target_dev;
			new_entry->is_fuse = tmp_entry->is_fuse;
			info.err = susfs_mark_inode_sus_kstat(new_entry->info.target_pathname, new_entry, true);
			if (info.err) {
				mutex_unlock(&susfs_mutex_lock_sus_kstat);
				kfree(new_entry);
				goto out_copy_to_user;
			}
			SUSFS_LOGI("updating target_ino from '%lu' to '%lu' for pathname: '%s' in SUS_KSTAT_HLIST\n",
					tmp_entry->target_ino, new_entry->target_ino, new_entry->info.target_pathname);
			hash_del_rcu(&tmp_entry->node);
			hash_add_rcu(SUS_KSTAT_HLIST, &new_entry->node, info.target_ino);
			mutex_unlock(&susfs_mutex_lock_sus_kstat);
			synchronize_rcu();
			kfree(tmp_entry);
			info.err = 0;
			goto out_copy_to_user;
		}
	}
	mutex_unlock(&susfs_mutex_lock_sus_kstat);
	info.err = -ENOENT;

out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_UPDATE_SUS_KSTAT -> ret: %d\n", info.err);
}

__attribute__((hot)) bool susfs_is_inode_sus_kstat(struct inode *inode, bool *out_is_fuse) {
	struct fuse_inode *fi = NULL;

	if (!inode)
		return false;
	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) {
			SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
			return false;
		}
		if (test_bit(AS_FLAGS_SUS_KSTAT, &fi->inode.i_state)) {
			*out_is_fuse = true;
			return true;
		}
		return false;
	}
	if (!inode->i_mapping) {
		SUSFS_LOGE("inode->i_mapping is NULL\n");
		return false;
	}
	if (test_bit(AS_FLAGS_SUS_KSTAT, &inode->i_state))
		return true;
	return false;
}

void susfs_sus_kstat_spoof_generic_fillattr(struct inode *inode, struct kstat *stat, u32 result_mask)
{
	struct st_susfs_sus_kstat_hlist *entry = NULL;
	struct fuse_inode *fi = NULL;
	unsigned long target_ino = 0;
	dev_t target_dev = 0;
	bool is_fuse = false;

	switch (result_mask) {
		case STATX_SUS_KSTAT:
			target_ino = inode->i_ino;
			target_dev = inode->i_sb->s_dev;
			goto out_spoof_kstat;
		case STATX_SUS_KSTAT_FUSE:
			fi = get_fuse_inode(inode);
			target_ino = fi->inode.i_ino;
			target_dev = fi->inode.i_sb->s_dev;
			is_fuse = true;
			goto out_spoof_kstat;
		default:
			return;
	}

out_spoof_kstat:
	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, target_ino) {
		if (entry->target_dev == target_dev &&
			entry->is_fuse == is_fuse)
		{
			SUSFS_LOGI("spoofing kstat for vfs_getattr_nosec, target_ino: %lu, target_dev: %u\n",
					target_ino, target_dev);
			if (entry->info.flags & KSTAT_SPOOF_INO)
				stat->ino = entry->info.spoofed_ino;
			if (entry->info.flags & KSTAT_SPOOF_DEV)
				stat->dev = entry->info.spoofed_dev;
			if (entry->info.flags & KSTAT_SPOOF_NLINK)
				stat->nlink = entry->info.spoofed_nlink;
			if (entry->info.flags & KSTAT_SPOOF_SIZE)
				stat->size = entry->info.spoofed_size;
			if (entry->info.flags & KSTAT_SPOOF_ATIME_TV_SEC)
				stat->atime.tv_sec = entry->info.spoofed_atime_tv_sec;
			if (entry->info.flags & KSTAT_SPOOF_ATIME_TV_NSEC)
				stat->atime.tv_nsec = entry->info.spoofed_atime_tv_nsec;
			if (entry->info.flags & KSTAT_SPOOF_MTIME_TV_SEC)
				stat->mtime.tv_sec = entry->info.spoofed_mtime_tv_sec;
			if (entry->info.flags & KSTAT_SPOOF_MTIME_TV_NSEC)
				stat->mtime.tv_nsec = entry->info.spoofed_mtime_tv_nsec;
			if (entry->info.flags & KSTAT_SPOOF_CTIME_TV_SEC)
				stat->ctime.tv_sec = entry->info.spoofed_ctime_tv_sec;
			if (entry->info.flags & KSTAT_SPOOF_CTIME_TV_NSEC)
				stat->ctime.tv_nsec = entry->info.spoofed_ctime_tv_nsec;
			if (entry->info.flags & KSTAT_SPOOF_BLKSIZE)
				stat->blksize = entry->info.spoofed_blksize;
			if (entry->info.flags & KSTAT_SPOOF_BLOCKS)
				stat->blocks = entry->info.spoofed_blocks;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
			stat->mnt_id = entry->spoofed_mnt_id;
#endif // #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
}

void susfs_sus_kstat_spoof_show_map_vma(struct inode *inode, dev_t *out_dev, unsigned long *out_ino) {
	struct st_susfs_sus_kstat_hlist *entry = NULL;
	struct fuse_inode *fi = NULL;
	unsigned long target_ino = 0;
	dev_t target_dev = 0;
	bool is_fuse = false;

	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) {
			SUSFS_LOGE("fi || fi->inode.i_mapping is NULL\n");
			return;
		}
		target_ino = fi->inode.i_ino;
		target_dev = fi->inode.i_sb->s_dev;
		is_fuse = true;
		goto out_spoof_kstat;
	}
	
	if (!inode->i_mapping) {
		SUSFS_LOGE("inode->i_mapping is NULL\n");
		return;
	}

	target_ino = inode->i_ino;
	target_dev = inode->i_sb->s_dev;

out_spoof_kstat:
	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, target_ino) {
		if (entry->target_dev == target_dev &&
			entry->is_fuse == is_fuse)
		{
			SUSFS_LOGI("spoofing kstat for show_map_vma, target_ino: %lu, target_dev: %u\n", target_ino, target_dev);
			*out_dev = entry->info.spoofed_dev;
			*out_ino = entry->info.spoofed_ino;
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
}

int susfs_sus_kstat_spoof_vfs_statfs(struct inode *inode, struct kstatfs *buf, bool *is_fuse) {
	struct st_susfs_sus_kstat_hlist *entry = NULL;
	struct inode *target_inode = inode;

	if (*is_fuse)
		target_inode = &get_fuse_inode(inode)->inode;

	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, target_inode->i_ino) {
		if (entry->target_dev == inode->i_sb->s_dev)
		{
			SUSFS_LOGI("spoofing kstat for vfs_statfs, target_ino: %lu, target_dev: %u\n", target_inode->i_ino, target_inode->i_sb->s_dev);
			memcpy(buf, &entry->spoofed_kstatfs, sizeof(struct kstatfs));
			rcu_read_unlock();
			return 0;
		}
	}
	rcu_read_unlock();
	return -EINVAL;
}

void susfs_sus_kstat_spoof_inotify_fdinfo(unsigned long *out_target_ino, dev_t *out_target_dev) {
	struct st_susfs_sus_kstat_hlist *entry = NULL;

	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, *out_target_ino) {
		if (entry->target_dev == *out_target_dev)
		{
			SUSFS_LOGI("spoofing kstat for inotify_fdinfo, target_ino: %lu, target_dev: %u\n", *out_target_ino, *out_target_dev);
			*out_target_ino = entry->info.spoofed_ino;
			*out_target_dev = entry->info.spoofed_dev;
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
}

void susfs_sus_kstat_spoof_proc_fd_seq_show(int *out_target_mnt_id, unsigned long *out_target_ino, dev_t target_dev) {
	struct st_susfs_sus_kstat_hlist *entry = NULL;

	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, *out_target_ino) {
		if (entry->target_dev == target_dev)
		{
			SUSFS_LOGI("spoofing kstat for proc_fd_seq_show, target_ino: %lu, target_dev: %u\n", *out_target_ino, target_dev);
			*out_target_mnt_id = entry->spoofed_mnt_id;
			*out_target_ino = entry->info.spoofed_ino;
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_KSTAT

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
static struct st_susfs_uname my_uname = {0};
DEFINE_STATIC_KEY_FALSE(susfs_is_uname_spoof_buffer_set);
static DEFINE_SEQLOCK(susfs_uname_seqlock);

void susfs_set_uname(void __user **user_info) {
	struct st_susfs_uname info = {0};

	if (copy_from_user(&info, (struct st_susfs_uname __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info.release == '\0' || *info.version == '\0') {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	write_seqlock(&susfs_uname_seqlock);
	if (!strcmp(info.release, "default")) {
		strscpy(my_uname.release, utsname()->release, __NEW_UTS_LEN);
	} else {
		strscpy(my_uname.release, info.release, __NEW_UTS_LEN);
	}
	if (!strcmp(info.version, "default")) {
		strscpy(my_uname.version, utsname()->version, __NEW_UTS_LEN);
	} else {
		strscpy(my_uname.version, info.version, __NEW_UTS_LEN);
	}
	write_sequnlock(&susfs_uname_seqlock);

	if (!static_key_enabled(&susfs_is_uname_spoof_buffer_set))
		static_branch_enable(&susfs_is_uname_spoof_buffer_set);

	SUSFS_LOGI("set spoofed release: '%s', version: '%s'\n",
				my_uname.release, my_uname.version);

	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_uname __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SET_UNAME -> ret: %d\n", info.err);
}

void susfs_spoof_uname(struct new_utsname* tmp) {
	unsigned seq;

	do {
		seq = read_seqbegin(&susfs_uname_seqlock);
		strscpy(tmp->release, my_uname.release, __NEW_UTS_LEN);
		strscpy(tmp->version, my_uname.version, __NEW_UTS_LEN);
	} while (read_seqretry(&susfs_uname_seqlock, seq));
}
#endif // #ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME

/* enable_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_enable_log(void __user **user_info) {
	struct st_susfs_log info = {0};

	if (copy_from_user(&info, (struct st_susfs_log __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (info.enabled) {
		static_branch_enable(&susfs_is_log_enabled);
		pr_info("susfs: enable logging to kernel");
	} else {
		static_branch_disable(&susfs_is_log_enabled);
		pr_info("susfs: disable logging to kernel");
	}

	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_log __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ENABLE_LOG -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_KSU_SUSFS_ENABLE_LOG

/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
static struct st_sus_su sus_su = {0};

int susfs_get_sus_su_working_mode(void) {
	return sus_su.mode;
}

bool susfs_is_sus_su_ready(void) {
	return sus_su.is_ready;
}

void susfs_sus_su(void __user **user_info) {
	struct st_sus_su info = {0};

	if (copy_from_user(&info, (struct st_sus_su __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (info.mode == SUS_SU_MODE_DEFAULT) {
		sus_su.mode = SUS_SU_MODE_DEFAULT;
		sus_su.is_ready = false;
		SUSFS_LOGI("sus_su is set to SUS_SU_MODE_DEFAULT\n");
		info.err = 0;
		goto out_copy_to_user;
	}

	if (info.mode == SUS_SU_MODE_CUSTOM) {
		sus_su.mode = SUS_SU_MODE_CUSTOM;
		sus_su.is_ready = true;
		SUSFS_LOGI("sus_su is set to SUS_SU_MODE_CUSTOM\n");
		info.err = 0;
		goto out_copy_to_user;
	}

	SUSFS_LOGE("mode: %d is not supported\n", info.mode);
	info.err = -EINVAL;
out_copy_to_user:
	if (copy_to_user(&((struct st_sus_su __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SUS_SU -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_SU

static inline void get_current_comm(char *comm) {
	memcpy(comm, current->comm, TASK_COMM_LEN);
}

int susfs_sus_ino_for_filldir64(unsigned long ino) {
	struct st_susfs_sus_kstat_hlist *entry = NULL;

	rcu_read_lock();
	hash_for_each_possible_rcu(SUS_KSTAT_HLIST, entry, node, ino) {
		if (entry->info.flags & KSTAT_SPOOF_INO) {
			rcu_read_unlock();
			return 1;
		}
	}
	rcu_read_unlock();
	return 0;
}

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
DEFINE_STATIC_SRCU(susfs_srcu_try_umount);
static DEFINE_MUTEX(susfs_mutex_lock_try_umount);
static LIST_HEAD(LH_TRY_UMOUNT);

void susfs_add_try_umount(void __user **user_info) {
	struct st_susfs_try_umount_list *new_list = NULL;
	struct st_susfs_try_umount info = {0};

	if (copy_from_user(&info, (struct st_susfs_try_umount __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info.target_pathname == '\0') {
		SUSFS_LOGE("target_pathname cannot be empty\n");
		info.err = -EINVAL;
		goto out_copy_to_user;
	}

	new_list = kzalloc(sizeof(struct st_susfs_try_umount_list), GFP_KERNEL);
	if (!new_list) {
		info.err = -ENOMEM;
		goto out_copy_to_user;
	}
	strscpy(new_list->info.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	new_list->info.mnt_mode = info.mnt_mode;
	INIT_LIST_HEAD(&new_list->list);
	mutex_lock(&susfs_mutex_lock_try_umount);
	list_add_tail_rcu(&new_list->list, &LH_TRY_UMOUNT);
	mutex_unlock(&susfs_mutex_lock_try_umount);
	SUSFS_LOGI("target_pathname: '%s', is successfully added to LH_TRY_UMOUNT\n", new_list->info.target_pathname);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_try_umount __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_TRY_UMOUNT -> ret: %d\n", info.err);
}

void susfs_run_try_umount_for_current_mnt_ns(void) {
	struct st_susfs_try_umount_list *cursor = NULL;
	struct path path;
	int srcu_idx = srcu_read_lock(&susfs_srcu_try_umount);

	list_for_each_entry_rcu(cursor, &LH_TRY_UMOUNT, list) {
		if (!kern_path(cursor->info.target_pathname, LOOKUP_FOLLOW, &path)) {
			SUSFS_LOGI("umounting '%s'\n", cursor->info.target_pathname);
			path_put(&path);
			ksu_try_umount(cursor->info.target_pathname, false, 0, cursor->info.mnt_mode);
		}
	}
	srcu_read_unlock(&susfs_srcu_try_umount, srcu_idx);
}
#endif // #ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT

/* sus_path (continued) */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
void susfs_add_sus_path_statically(void) {
	struct st_susfs_sus_path info = {0};
	void __user *user_info = (void __user *)&info;
	susfs_add_sus_path(&user_info);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH

/* sus_mount (continued) */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
void susfs_add_sus_mount(void __user **user_info) {
	struct st_susfs_sus_mount info = {0};
	struct path path;
	struct mount *mnt;

	if (copy_from_user(&info, (struct st_susfs_sus_mount __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (info.err) {
		SUSFS_LOGE("failed opening file '%s'\n", info.target_pathname);
		goto out_copy_to_user;
	}

	mnt = real_mount(path.mnt);
	if (!mnt) {
		info.err = -ENOENT;
		goto out_path_put;
	}

	mnt->mnt.mnt_flags |= MNT_SUSFS;
	SUSFS_LOGI("marked MNT_SUSFS on mnt: %p, path: '%s'\n", mnt, info.target_pathname);
	info.err = 0;

out_path_put:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_mount __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_MOUNT -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_memfd */
#ifdef CONFIG_KSU_SUSFS_SUS_MEMFD
DEFINE_STATIC_KEY_FALSE(susfs_is_sus_memfd_enabled);

void susfs_set_sus_memfd(void __user **user_info) {
	struct st_susfs_sus_memfd info = {0};

	if (copy_from_user(&info, (struct st_susfs_sus_memfd __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (info.enabled) {
		static_branch_enable(&susfs_is_sus_memfd_enabled);
	} else {
		static_branch_disable(&susfs_is_sus_memfd_enabled);
	}

	SUSFS_LOGI("susfs_is_sus_memfd_enabled: %d\n", static_key_enabled(&susfs_is_sus_memfd_enabled));
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_memfd __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SET_SUS_MEMFD -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MEMFD

/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
DEFINE_STATIC_SRCU(susfs_srcu_open_redirect);
static DEFINE_MUTEX(susfs_mutex_lock_open_redirect);
static LIST_HEAD(LH_OPEN_REDIRECT);

void susfs_add_open_redirect(void __user **user_info) {
	struct st_susfs_open_redirect_list *new_list = NULL;
	struct st_susfs_open_redirect info = {0};

	if (copy_from_user(&info, (struct st_susfs_open_redirect __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info.target_pathname == '\0' || *info.redirected_pathname == '\0') {
		SUSFS_LOGE("target_pathname or redirected_pathname cannot be empty\n");
		info.err = -EINVAL;
		goto out_copy_to_user;
	}

	new_list = kzalloc(sizeof(struct st_susfs_open_redirect_list), GFP_KERNEL);
	if (!new_list) {
		info.err = -ENOMEM;
		goto out_copy_to_user;
	}
	strscpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	strscpy(new_list->redirected_pathname, info.redirected_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	INIT_LIST_HEAD(&new_list->list);
	mutex_lock(&susfs_mutex_lock_open_redirect);
	list_add_tail_rcu(&new_list->list, &LH_OPEN_REDIRECT);
	mutex_unlock(&susfs_mutex_lock_open_redirect);
	SUSFS_LOGI("target_pathname: '%s' -> redirected_pathname: '%s', is successfully added to LH_OPEN_REDIRECT\n",
			new_list->target_pathname, new_list->redirected_pathname);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_open_redirect __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_OPEN_REDIRECT -> ret: %d\n", info.err);
}

struct filename* susfs_get_redirected_path(unsigned long ino) {
	struct st_susfs_open_redirect_list *cursor = NULL;
	struct filename *redirected_filename = NULL;
	int srcu_idx = srcu_read_lock(&susfs_srcu_open_redirect);

	list_for_each_entry_rcu(cursor, &LH_OPEN_REDIRECT, list) {
		if (cursor->target_ino == ino) {
			redirected_filename = getname_kernel(cursor->redirected_pathname);
			break;
		}
	}
	srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
	return redirected_filename;
}
#endif // #ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT

/* susfs_init & workqueue */
#define SDCARD_ANDROID_PATH "/sdcard/Android"
DEFINE_STATIC_KEY_TRUE(susfs_is_sdcard_android_data_not_decrypted);
static struct delayed_work sdcard_cleanup_dwork;
static struct fsnotify_group *g = NULL;
static struct fsnotify_mark g_watch;

static int susfs_handle_fsnotify_event(struct fsnotify_group *group,
				      struct inode *inode,
				      u32 mask, const void *data, int data_type,
				      const struct qstr *file_name, u32 cookie,
				      struct fsnotify_iter_info *iter_info)
{
	if (mask & FS_CREATE) {
		if (file_name && !strcmp(file_name->name, "data")) {
			SUSFS_LOGI("detected /sdcard/Android/data is created, decrypting...\n");
			schedule_delayed_work(&sdcard_cleanup_dwork, msecs_to_jiffies(1000));
		}
	}
	return 0;
}

static void susfs_sdcard_cleanup_fn(struct work_struct *work) {
	if (static_key_enabled(&susfs_is_sdcard_android_data_not_decrypted)) {
		static_branch_disable(&susfs_is_sdcard_android_data_not_decrypted);
		SUSFS_LOGI("successfully disabled susfs_is_sdcard_android_data_not_decrypted\n");
	}
}

static const struct fsnotify_ops fsnotify_ops = {
	.handle_event = susfs_handle_fsnotify_event,
};

static int watch_one_dir(struct fsnotify_mark *mark) {
	struct path path;
	int ret = 0;

	ret = kern_path(SDCARD_ANDROID_PATH, LOOKUP_FOLLOW, &path);
	if (ret) {
		return ret;
	}

	fsnotify_init_mark(mark, g);
	mark->mask = FS_CREATE;
	ret = fsnotify_add_mark_locked(mark, &d_backing_inode(path.dentry)->i_fsnotify_marks, FSNOTIFY_OBJ_TYPE_INODE, 0, NULL);
	path_put(&path);
	return ret;
}

static int susfs_sdcard_monitor_fn(void *data)
{
	struct cred *cred = prepare_creds();
	int ret = 0;

	if (!cred) {
		SUSFS_LOGE("failed to prepare creds!\n");
		return -ENOMEM;
	}

	setup_selinux("u:r:ksu:s0", cred);
	commit_creds(cred);

	if (!susfs_is_current_ksu_domain()) {
		SUSFS_LOGE("domain is not ksu, exiting the thread\n");
		return -EINVAL;
	}

	SUSFS_LOGI("start monitoring path '%s' using fsnotify\n",
				SDCARD_ANDROID_PATH);

	INIT_DELAYED_WORK(&sdcard_cleanup_dwork, susfs_sdcard_cleanup_fn);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	g = fsnotify_alloc_group(&fsnotify_ops, 0);
#else
	g = fsnotify_alloc_group(&fsnotify_ops);
#endif
	if (IS_ERR(g)) {
		return PTR_ERR(g);
	}

	ret = watch_one_dir(&g_watch);

	SUSFS_LOGI("ret: %d\n", ret);

	return 0;
}

void susfs_start_sdcard_monitor_fn(void) {
	if (IS_ERR(kthread_run(susfs_sdcard_monitor_fn, NULL, "susfs_sdcard_monitor"))) {
		SUSFS_LOGE("failed to create thread susfs_sdcard_monitor\n");
		SUSFS_LOGI("/sdcard is forcibly set decrypted\n");
		if (static_key_enabled(&susfs_is_sdcard_android_data_not_decrypted))
			static_branch_disable(&susfs_is_sdcard_android_data_not_decrypted);
	}
}

// - defer extra susfs works to workqueue after do_umount in ksu_handle_setresuid()
//   so that we do not block there and reduce the risk of time side channel as much as possible.
struct work_struct susfs_extra_works;
static void susfs_run_extra_works(struct work_struct *work) {
	if (!ksu_cred)
		return;
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	susfs_run_sus_path_loop();
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH
}

#define SUSFS_MAX_CUSTOM_SOCKS 16
#define SUSFS_SOCK_PATTERN_LEN 64

static char susfs_custom_socks[SUSFS_MAX_CUSTOM_SOCKS][SUSFS_SOCK_PATTERN_LEN];
static int susfs_custom_socks_count = 0;
static DEFINE_SPINLOCK(susfs_socks_lock);
static struct delayed_work susfs_load_socks_dwork;

static void susfs_load_dynamic_socks(struct work_struct *work)
{
	struct file *fp;
	loff_t pos = 0;
	char *buf;
	ssize_t bytes;
	char *p, *line_start;
	unsigned long flags;
	int count = 0;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	fp = filp_open("/data/adb/ReSuSFS/sus_socks.txt", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		kfree(buf);
		return;
	}

	bytes = kernel_read(fp, buf, PAGE_SIZE - 1, &pos);
	filp_close(fp, NULL);

	if (bytes <= 0) {
		kfree(buf);
		return;
	}
	buf[bytes] = '\0';

	spin_lock_irqsave(&susfs_socks_lock, flags);
	p = buf;
	line_start = p;
	while (*p && count < SUSFS_MAX_CUSTOM_SOCKS) {
		if (*p == '\n' || *p == '\r') {
			*p = '\0';
			if (p > line_start && line_start[0] != '#') {
				strlcpy(susfs_custom_socks[count], line_start, SUSFS_SOCK_PATTERN_LEN);
				count++;
			}
			line_start = p + 1;
		}
		p++;
	}
	if (*line_start && line_start[0] != '#' && count < SUSFS_MAX_CUSTOM_SOCKS) {
		strlcpy(susfs_custom_socks[count], line_start, SUSFS_SOCK_PATTERN_LEN);
		count++;
	}
	susfs_custom_socks_count = count;
	spin_unlock_irqrestore(&susfs_socks_lock, flags);

	kfree(buf);
	pr_info("susfs: dynamic socket patterns loaded: %d entries\n", count);
}

/* susfs_init */
void susfs_init(void) {
	SUSFS_LOGI("Initializing susfs_extra_works\n");
	INIT_WORK(&susfs_extra_works, susfs_run_extra_works);
	INIT_DELAYED_WORK(&susfs_load_socks_dwork, susfs_load_dynamic_socks);
	schedule_delayed_work(&susfs_load_socks_dwork, msecs_to_jiffies(15000));
	SUSFS_LOGI("susfs is initialized! version: " SUSFS_VERSION " \n");
}

/* No module exit is needed becuase it should never be a loadable kernel module */
//void __init susfs_exit(void)

#ifdef CONFIG_KSU_SUSFS
bool susfs_is_sus_unix_socket(const char *sun, int len)
{
	unsigned long flags;
	int i;

	if (!sun || len <= 0)
		return false;

	if (strnstr(sun, "zn_", len) ||
	    strnstr(sun, "lspd", len) ||
	    strnstr(sun, "/data/adb", len) ||
	    strnstr(sun, "zygote_next", len) ||
	    strnstr(sun, "magisk", len) ||
	    strnstr(sun, "usap_pool", len))
		return true;

	if (susfs_custom_socks_count > 0) {
		spin_lock_irqsave(&susfs_socks_lock, flags);
		for (i = 0; i < susfs_custom_socks_count; i++) {
			if (susfs_custom_socks[i][0] && strnstr(sun, susfs_custom_socks[i], len)) {
				spin_unlock_irqrestore(&susfs_socks_lock, flags);
				return true;
			}
		}
		spin_unlock_irqrestore(&susfs_socks_lock, flags);
	}

	return false;
}

void susfs_sanitize_sepolicy_buffer(void *data, size_t len)
{
	static const struct {
		const char *target;
		const char *replace;
		size_t len;
	} scrub_list[] = {
		{ "su_exec",     "no_exec",     7 },
		{ "magisk_file", "vendor_file", 11 },
		{ "ksu_file",    "sys_file",    8 },
		{ "su_key",      "no_key",      6 },
	};
	int i;
	char *p, *end;

	if (!data || len < 16)
		return;

	end = (char *)data + len;
	for (i = 0; i < ARRAY_SIZE(scrub_list); i++) {
		p = (char *)data;
		while (p + scrub_list[i].len <= end) {
			if (memcmp(p, scrub_list[i].target, scrub_list[i].len) == 0) {
				memcpy(p, scrub_list[i].replace, scrub_list[i].len);
				p += scrub_list[i].len;
			} else {
				p++;
			}
		}
	}
}
#endif
