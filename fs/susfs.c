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
	strscpy(new_list->info.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME);
	strscpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME);
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
	if (susfs_is_current_app_allowed_su()) {
		return false;
	}
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

/* spoof_cmdline_or_bootconfig */
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
static char *fake_cmdline_or_bootconfig = NULL;
DEFINE_STATIC_KEY_FALSE(susfs_is_fake_cmdline_or_bootconfig_buffer_set);
static DEFINE_SEQLOCK(susfs_fake_cmdline_or_bootconfig_seqlock);

void susfs_set_cmdline_or_bootconfig(void __user **user_info) {
	struct st_susfs_spoof_cmdline_or_bootconfig *info = (struct st_susfs_spoof_cmdline_or_bootconfig *)kzalloc(sizeof(struct st_susfs_spoof_cmdline_or_bootconfig), GFP_KERNEL);
	int err = 0;

	if (!info) {
		err = -ENOMEM;
		if (copy_to_user(&((struct st_susfs_spoof_cmdline_or_bootconfig __user*)*user_info)->err, &err, sizeof(err)))
			err = -EFAULT;
		SUSFS_LOGI("CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG -> ret: %d\n", err);
		return;
	}

	if (copy_from_user(info, (struct st_susfs_spoof_cmdline_or_bootconfig __user*)*user_info, sizeof(struct st_susfs_spoof_cmdline_or_bootconfig))) {
		info->err = -EFAULT;
		goto out_copy_to_user;
	}

	if (*info->fake_cmdline_or_bootconfig == '\0') {
		info->err = -EINVAL;
		goto out_copy_to_user;
	}

	if (!fake_cmdline_or_bootconfig) {
		fake_cmdline_or_bootconfig = (char *)kzalloc(SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE, GFP_KERNEL);
		if (!fake_cmdline_or_bootconfig) {
			info->err = -ENOMEM;
			goto out_copy_to_user;
		}
	}

	write_seqlock(&susfs_fake_cmdline_or_bootconfig_seqlock);
	strscpy(fake_cmdline_or_bootconfig,
			info->fake_cmdline_or_bootconfig,
			SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE - 1);
	write_sequnlock(&susfs_fake_cmdline_or_bootconfig_seqlock);

	if (!static_key_enabled(&susfs_is_fake_cmdline_or_bootconfig_buffer_set))
		static_branch_enable(&susfs_is_fake_cmdline_or_bootconfig_buffer_set);
	SUSFS_LOGI("fake_cmdline_or_bootconfig is set\n");

	info->err = 0;

out_copy_to_user:

	if (copy_to_user(&((struct st_susfs_spoof_cmdline_or_bootconfig __user*)*user_info)->err, &info->err, sizeof(info->err))) {
		info->err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG -> ret: %d\n", info->err);
	if (info) {
		kfree(info);
	}
}

void susfs_spoof_cmdline_or_bootconfig(struct seq_file *m) {
	unsigned seq;

	do {
		seq = read_seqbegin(&susfs_fake_cmdline_or_bootconfig_seqlock);
		seq_puts(m, fake_cmdline_or_bootconfig);
	} while (read_seqretry(&susfs_fake_cmdline_or_bootconfig_seqlock, seq));
}
#endif

/* open_redirect */
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
static DEFINE_MUTEX(susfs_mutex_lock_open_redirect);
static DEFINE_HASHTABLE(OPEN_REDIRECT_HLIST, 14);
DEFINE_SRCU(susfs_srcu_open_redirect);

void susfs_add_open_redirect(void __user **user_info) {
	struct st_susfs_open_redirect info = {0};
	struct st_susfs_open_redirect_hlist *new_entry_target, *new_entry_redirected, *tmp_entry_target, *tmp_entry_redirected;
	struct hlist_node *tmp_hlist_node;
	struct path target_path, redirected_path;
	struct inode *target_inode, *redirected_inode;
	bool is_first_dup_found = false;
	bool is_second_dup_found = false;

	if (copy_from_user(&info, (struct st_susfs_open_redirect __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

        if (*info.target_pathname == '\0') {
                info.err = -EINVAL;
		SUSFS_LOGE("empty target_pathname\n");
                goto out_copy_to_user;
        }

	if (info.uid_scheme < UID_NON_APP_PROC || info.uid_scheme > UID_UMOUNTED_PROC) {
		info.err = -EINVAL;
		SUSFS_LOGE("invalid uid scheme: %d\n", info.uid_scheme);
                goto out_copy_to_user;
	}

	info.err = kern_path(info.redirected_pathname, 0, &redirected_path);
	if (info.err) {
		SUSFS_LOGE("failed opening redirected file '%s'\n", info.redirected_pathname);
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, 0, &target_path);
	if (info.err) {
		SUSFS_LOGE("failed opening target file '%s'\n", info.target_pathname);
		goto out_path_put_redirected_path;
	}

	redirected_inode = d_backing_inode(redirected_path.dentry);
	if (!redirected_inode || !redirected_inode->i_mapping) {
		SUSFS_LOGE("redirected_inode || redirected_inode->i_mapping is NULL\n");
		info.err = -ENOENT;
		goto out_path_put_target_path;
	}

	target_inode = d_backing_inode(target_path.dentry);
	if (!target_inode || !target_inode->i_mapping) {
		SUSFS_LOGE("target_inode || target_inode->i_mapping is NULL\n");
		info.err = -ENOENT;
		goto out_path_put_target_path;
	}

	if (redirected_inode->i_sb->s_magic == FUSE_SUPER_MAGIC ||
	    target_inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		SUSFS_LOGE("FUSE fs is not supported for open_redirect feature\n");
		info.err = -EINVAL;
		goto out_path_put_target_path;
	}

	new_entry_target = kzalloc(sizeof(struct st_susfs_open_redirect_hlist), GFP_KERNEL);
	if (!new_entry_target) {
		info.err = -ENOMEM;
		goto out_path_put_target_path;
	}

	new_entry_redirected = kzalloc(sizeof(struct st_susfs_open_redirect_hlist), GFP_KERNEL);
	if (!new_entry_redirected) {
		info.err = -ENOMEM;
		kfree(new_entry_target);
		goto out_path_put_target_path;
	}

	new_entry_target->target_ino = target_inode->i_ino;
	new_entry_target->target_dev = target_inode->i_sb->s_dev;
	new_entry_target->redirected_ino = redirected_inode->i_ino;
	new_entry_target->redirected_dev = redirected_inode->i_sb->s_dev;
	new_entry_target->info.uid_scheme = info.uid_scheme;
	new_entry_target->reversed_lookup_only = false;
	new_entry_target->spoofed_mnt_id = real_mount(target_path.mnt)->mnt_id;
	(void)vfs_statfs(&target_path, &new_entry_target->spoofed_kstatfs);
	memcpy(&new_entry_target->info, &info, sizeof(info));

	new_entry_redirected->target_ino = redirected_inode->i_ino;
	new_entry_redirected->target_dev = redirected_inode->i_sb->s_dev;
	new_entry_redirected->redirected_ino = target_inode->i_ino;
	new_entry_redirected->redirected_dev = target_inode->i_sb->s_dev;
	new_entry_redirected->info.uid_scheme = info.uid_scheme;
	new_entry_redirected->reversed_lookup_only = true;
	new_entry_redirected->spoofed_mnt_id = new_entry_target->spoofed_mnt_id;
	memcpy(&new_entry_redirected->spoofed_kstatfs, &new_entry_target->spoofed_kstatfs, sizeof(struct kstatfs));
	strscpy(new_entry_redirected->info.target_pathname, info.redirected_pathname, SUSFS_MAX_LEN_PATHNAME);
	strscpy(new_entry_redirected->info.redirected_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME);

	// check for existing entries, delete it first if so
	mutex_lock(&susfs_mutex_lock_open_redirect);
	hash_for_each_possible_safe(OPEN_REDIRECT_HLIST, tmp_entry_target, tmp_hlist_node, node, target_inode->i_ino) {
		if (!strcmp(tmp_entry_target->info.target_pathname, info.target_pathname)) {
			if (tmp_entry_target->reversed_lookup_only) {
				SUSFS_LOGE("duplicated '%s' cannot be removed/added because it is used for reversed lookup only\n", info.target_pathname);
				mutex_unlock(&susfs_mutex_lock_open_redirect);
				info.err = -EINVAL;
				kfree(new_entry_redirected);
				kfree(new_entry_target);
				goto out_path_put_target_path;
			}
			is_first_dup_found = true;
			hash_del_rcu(&tmp_entry_target->node);
			break;
		}
	}

	if (is_first_dup_found) {
		hash_for_each_possible_safe(OPEN_REDIRECT_HLIST, tmp_entry_redirected, tmp_hlist_node, node, redirected_inode->i_ino) {
			if (!strcmp(tmp_entry_redirected->info.target_pathname, info.redirected_pathname)) {
				is_second_dup_found = true;
				hash_del_rcu(&tmp_entry_redirected->node);
				break;
			}
		}
		SUSFS_LOGI("target_pathname: '%s', redirected_pathname: '%s', target_i_ino: '%lu', redirected_i_ino: '%lu', target_s_dev: '%lu', redirected_s_dev: '%lu', uid_scheme: '%d', reversed_lookup_only: %d, spoofed_mnt_id: %d, is successfully added to OPEN_REDIRECT_HLIST\n",
			new_entry_target->info.target_pathname, new_entry_target->info.redirected_pathname, new_entry_target->target_ino, new_entry_target->redirected_ino, new_entry_target->target_dev, new_entry_target->redirected_dev, new_entry_target->info.uid_scheme, new_entry_target->reversed_lookup_only, new_entry_target->spoofed_mnt_id);
		SUSFS_LOGI("target_pathname: '%s', redirected_pathname: '%s', target_i_ino: '%lu', redirected_i_ino: '%lu', target_s_dev: '%lu', redirected_s_dev: '%lu', uid_scheme: '%d', reversed_lookup_only: %d, spoofed_mnt_id: %d, is successfully added to OPEN_REDIRECT_HLIST\n",
			new_entry_redirected->info.target_pathname, new_entry_redirected->info.redirected_pathname, new_entry_redirected->target_ino, new_entry_redirected->redirected_ino, new_entry_redirected->target_dev, new_entry_redirected->redirected_dev, new_entry_redirected->info.uid_scheme, new_entry_redirected->reversed_lookup_only, new_entry_redirected->spoofed_mnt_id);
		hash_add_rcu(OPEN_REDIRECT_HLIST, &new_entry_target->node, new_entry_target->target_ino);
		hash_add_rcu(OPEN_REDIRECT_HLIST, &new_entry_redirected->node, new_entry_redirected->target_ino);
		// we need to mark both target and redirected path inode just for spoofing readlink as well
		set_bit(AS_FLAGS_OPEN_REDIRECT, &redirected_inode->i_state);
		set_bit(AS_FLAGS_OPEN_REDIRECT, &target_inode->i_state);
		mutex_unlock(&susfs_mutex_lock_open_redirect);
		synchronize_srcu(&susfs_srcu_open_redirect);
		if (is_second_dup_found)
			kfree(tmp_entry_redirected);
		kfree(tmp_entry_target);
		info.err = 0;
		goto out_path_put_target_path;
	}

	SUSFS_LOGI("target_pathname: '%s', redirected_pathname: '%s', target_i_ino: '%lu', redirected_i_ino: '%lu', target_s_dev: '%lu', redirected_s_dev: '%lu', uid_scheme: '%d', reversed_lookup_only: %d, spoofed_mnt_id: %d, is successfully added to OPEN_REDIRECT_HLIST\n",
			new_entry_target->info.target_pathname, new_entry_target->info.redirected_pathname, new_entry_target->target_ino, new_entry_target->redirected_ino, new_entry_target->target_dev, new_entry_target->redirected_dev, new_entry_target->info.uid_scheme, new_entry_target->reversed_lookup_only, new_entry_target->spoofed_mnt_id);
	SUSFS_LOGI("target_pathname: '%s', redirected_pathname: '%s', target_i_ino: '%lu', redirected_i_ino: '%lu', target_s_dev: '%lu', redirected_s_dev: '%lu', uid_scheme: '%d', reversed_lookup_only: %d, spoofed_mnt_id: %d, is successfully added to OPEN_REDIRECT_HLIST\n",
			new_entry_redirected->info.target_pathname, new_entry_redirected->info.redirected_pathname, new_entry_redirected->target_ino, new_entry_redirected->redirected_ino, new_entry_redirected->target_dev, new_entry_redirected->redirected_dev, new_entry_redirected->info.uid_scheme, new_entry_redirected->reversed_lookup_only, new_entry_redirected->spoofed_mnt_id);
	hash_add_rcu(OPEN_REDIRECT_HLIST, &new_entry_target->node, new_entry_target->target_ino);
	hash_add_rcu(OPEN_REDIRECT_HLIST, &new_entry_redirected->node, new_entry_redirected->target_ino);
	// we need to mark both target and redirected path inode just for spoofing readlink as well
	set_bit(AS_FLAGS_OPEN_REDIRECT, &redirected_inode->i_state);
	set_bit(AS_FLAGS_OPEN_REDIRECT, &target_inode->i_state);
	mutex_unlock(&susfs_mutex_lock_open_redirect);
	info.err = 0;

out_path_put_target_path:
	path_put(&target_path);
out_path_put_redirected_path:
	path_put(&redirected_path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_open_redirect __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_OPEN_REDIRECT -> ret: %d\n", info.err);
}

struct filename *susfs_open_redirect_spoof_do_sys_openat(struct inode *inode) {
	struct st_susfs_open_redirect_hlist *entry = NULL;
	struct filename *new_filename = NULL;
	int srcu_idx = srcu_read_lock(&susfs_srcu_open_redirect);

	hash_for_each_possible_rcu(OPEN_REDIRECT_HLIST, entry, node, inode->i_ino) {
		if (!entry->reversed_lookup_only &&
			entry->target_dev == inode->i_sb->s_dev)
		{
			switch(entry->info.uid_scheme) {
				case UID_NON_APP_PROC:
					if (current_uid().val % 100000 < 10000)
						break;
					goto out_srcu_read_unlock;
				case UID_ROOT_PROC_EXCEPT_SU_PROC:
					if (current_uid().val == 0 && !susfs_is_current_ksu_domain())
						break;
					goto out_srcu_read_unlock;
				case UID_NON_SU_PROC:
					if (!susfs_is_current_ksu_domain() && !susfs_is_current_app_allowed_su())
						break;
					goto out_srcu_read_unlock;
				case UID_UMOUNTED_APP_PROC:
					if (susfs_is_current_proc_umounted_app() && !susfs_is_current_app_allowed_su())
						break;
					goto out_srcu_read_unlock;
				case UID_UMOUNTED_PROC:
					if (susfs_is_current_proc_umounted() && !susfs_is_current_app_allowed_su())
						break;
					goto out_srcu_read_unlock;
				default:
					goto out_srcu_read_unlock;
			}
			SUSFS_LOGI("redirect path '%s' to '%s', uid_scheme: %d\n",
					entry->info.target_pathname, entry->info.redirected_pathname, entry->info.uid_scheme);
			new_filename = getname_kernel(entry->info.redirected_pathname);
			srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
			return new_filename;
		}
	}
out_srcu_read_unlock:
	srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
	return new_filename;
}

int susfs_open_redirect_spoof_vfs_readlink(struct inode *inode, char __user *buffer, int buflen) {
	struct st_susfs_open_redirect_hlist *entry = NULL;
	int srcu_idx = srcu_read_lock(&susfs_srcu_open_redirect);

	hash_for_each_possible_rcu(OPEN_REDIRECT_HLIST, entry, node, inode->i_ino) {
		if (entry->reversed_lookup_only &&
			entry->target_dev == inode->i_sb->s_dev)
		{
			SUSFS_LOGI("spoof path '%s' to '%s'\n",
					entry->info.target_pathname, entry->info.redirected_pathname);
			if (strlen(entry->info.redirected_pathname) >= buflen) {
				SUSFS_LOGE("buflen not big enough\n");
				srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
				return -ENAMETOOLONG;
			}
			if (copy_to_user(buffer, entry->info.redirected_pathname, strlen(entry->info.redirected_pathname))) {
				SUSFS_LOGE("copy_to_user() failed\n");
				srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
				return -EFAULT;
			}
			srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
			return 0;
		}
	}
	srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
	return -ENOENT;
}

int susfs_open_redirect_spoof_do_proc_readlink(struct inode *inode, char *tmp_buf, int buflen) {
	struct st_susfs_open_redirect_hlist *entry = NULL;
	int srcu_idx = srcu_read_lock(&susfs_srcu_open_redirect);

	hash_for_each_possible_rcu(OPEN_REDIRECT_HLIST, entry, node, inode->i_ino) {
		if (entry->reversed_lookup_only &&
			entry->target_dev == inode->i_sb->s_dev)
		{
			SUSFS_LOGI("spoof path '%s' to '%s'\n",
					entry->info.target_pathname, entry->info.redirected_pathname);
			if (strlen(entry->info.redirected_pathname) >= buflen) {
				SUSFS_LOGE("buflen not big enough\n");
				srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
				return -ENAMETOOLONG;
			}
			strscpy(tmp_buf, entry->info.redirected_pathname, SUSFS_MAX_LEN_PATHNAME);
			srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
			return 0;
		}
	}
	srcu_read_unlock(&susfs_srcu_open_redirect, srcu_idx);
	return -ENOENT;
}

/* callers must hold and release the "susfs_srcu_open_redirect" lock themselves. */
int susfs_open_redirect_spoof_show_map_vma_srcu(struct inode *inode, unsigned long *out_ino, dev_t *out_dev, char **out_spoofed_name) {
	struct st_susfs_open_redirect_hlist *entry = NULL;

	if (!out_spoofed_name || *out_spoofed_name != NULL) {
		SUSFS_LOGE("out_spoofed_name cannot be NULL and *out_spoofed_name has to be NULL\n");
		return -EINVAL;
	}

	hash_for_each_possible_rcu(OPEN_REDIRECT_HLIST, entry, node, inode->i_ino) {
		if (entry->reversed_lookup_only &&
			entry->target_dev == inode->i_sb->s_dev)
		{
			SUSFS_LOGI("spoof maps ino/dev/name for redirected path: '%s'\n",
					entry->info.target_pathname);
			*out_ino = entry->redirected_ino;
			*out_dev = entry->redirected_dev;
			*out_spoofed_name = entry->info.redirected_pathname;
			return 0;
		}
	}
	return -EINVAL;
}
#endif // #ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT

/* sus_map */
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void susfs_add_sus_map(void __user **user_info) {
	struct st_susfs_sus_map info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_map __user*)*user_info, sizeof(info))) {
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
	set_bit(AS_FLAGS_SUS_MAP, &inode->i_state);
	SUSFS_LOGI("pathname: '%s', is flagged as AS_FLAGS_SUS_MAP\n", info.target_pathname);
	info.err = 0;
out_path_put_path:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_map __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_MAP -> ret: %d\n", info.err);
}

bool susfs_is_sus_map_file(struct file *file)
{
	struct inode *inode;
	const char *name;

	if (!file)
		return false;

	inode = file_inode(file);
	if (inode && test_bit(AS_FLAGS_SUS_MAP, &inode->i_state))
		return true;

	if (file->f_path.dentry) {
		name = file->f_path.dentry->d_name.name;
		if (name) {
			if (strnstr(name, "frida", 32) ||
			    strnstr(name, "neo_zygisk", 32) ||
			    strnstr(name, "libriru", 32) ||
			    strnstr(name, "usap_pool", 32))
				return true;
		}
	}

	return false;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MAP

/* susfs avc log spoofing */
DEFINE_STATIC_KEY_FALSE(susfs_is_avc_log_spoofing_enabled);

void susfs_set_avc_log_spoofing(void __user **user_info) {
	struct st_susfs_avc_log_spoofing info = {0};

	if (copy_from_user(&info, (struct st_susfs_avc_log_spoofing __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	if (info.enabled) {
		static_branch_enable(&susfs_is_avc_log_spoofing_enabled);
		SUSFS_LOGI("enabling susfs_avc_log_spoofing\n");
	} else {
		static_branch_disable(&susfs_is_avc_log_spoofing_enabled);
		SUSFS_LOGI("disabling susfs_avc_log_spoofing\n");
	}

	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_avc_log_spoofing __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING -> ret: %d\n", info.err);
}

/* get susfs enabled features */
static int copy_config_to_buf(const char *config_string, char *buf_ptr, size_t *copied_size, size_t bufsize) {
	size_t tmp_size = strlen(config_string);

	*copied_size += tmp_size;
	if (*copied_size >= bufsize) {
		SUSFS_LOGE("bufsize is not big enough to hold the string.\n");
		return -EINVAL;
	}
	memcpy(buf_ptr, config_string, tmp_size);
	return 0;
}

void susfs_get_enabled_features(void __user **user_info) {
	struct st_susfs_enabled_features *info = (struct st_susfs_enabled_features *)kzalloc(sizeof(struct st_susfs_enabled_features), GFP_KERNEL);
	char *buf_ptr = NULL;
	size_t copied_size = 0;

	if (!info) {
		SUSFS_LOGE("failed to kzalloc for st_susfs_enabled_features\n");
		return;
	}

	if (copy_from_user(info, (struct st_susfs_enabled_features __user*)*user_info, sizeof(struct st_susfs_enabled_features))) {
		info->err = -EFAULT;
		goto out_copy_to_user;
	}

	buf_ptr = info->enabled_features;

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_PATH\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_MOUNT\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_KSTAT\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SPOOF_UNAME\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_ENABLE_LOG\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_OPEN_REDIRECT\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_MAP\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_NET
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_NET\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_FD
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_FD\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_FILTER_ENV
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_FILTER_ENV\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_COMM
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SPOOF_COMM\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_AUTO_SPOOF_SEPOLICY
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_AUTO_SPOOF_SEPOLICY\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_SU
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_SU\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_TRY_UMOUNT\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif

	info->err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_enabled_features __user*)*user_info, info, sizeof(struct st_susfs_enabled_features))) {
		info->err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_ENABLED_FEATURES -> ret: %d\n", info->err);
	if (info) {
		kfree(info);
	}
}

/* show_variant */
void susfs_show_variant(void __user **user_info) {
	struct st_susfs_variant info = {0};

	if (copy_from_user(&info, (struct st_susfs_variant __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	strscpy(info.susfs_variant, SUSFS_VARIANT, SUSFS_MAX_VARIANT_BUFSIZE-1);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_variant __user*)*user_info, &info, sizeof(info))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_VARIANT -> ret: %d\n", info.err);
}

/* show version */
void susfs_show_version(void __user **user_info) {
	struct st_susfs_version info = {0};

	if (copy_from_user(&info, (struct st_susfs_version __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	strscpy(info.susfs_version, SUSFS_VERSION, SUSFS_MAX_VERSION_BUFSIZE-1);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_version __user*)*user_info, &info, sizeof(info))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_VERSION -> ret: %d\n", info.err);
}

/* kthread for checking if /sdcard/Android is accessible via fsnoitfy */
/* code is straightly borrowed from KernelSU's pkg_observer.c */
#define SDCARD_ANDROID_PATH "/data/media/0/Android"
DEFINE_STATIC_KEY_TRUE(susfs_is_sdcard_android_data_not_decrypted);

struct watch_dir {
	const char *path;
	u32 mask;
	struct path kpath;
	struct inode *inode;
	struct fsnotify_mark *mark;
};

static struct fsnotify_group *g;

static struct watch_dir g_watch = { .path = "/data/media/0", // we choose the underlying f2fs /data/media/0 instead of the FUSE /sdcard
									.mask = (FS_EVENT_ON_CHILD | FS_ISDIR | FS_OPEN_PERM) };

static int add_mark_on_inode(struct inode *inode, u32 mask,
								struct fsnotify_mark **out);

static unsigned long sdcard_cleanup_scheduled;
static struct delayed_work sdcard_cleanup_dwork;

static void susfs_sdcard_cleanup_fn(struct work_struct *work)
{
	struct fsnotify_group *grp;
	struct inode *inode;

	if (static_key_enabled(&susfs_is_sdcard_android_data_not_decrypted))
		static_branch_disable(&susfs_is_sdcard_android_data_not_decrypted);
	SUSFS_LOGI("/sdcard is decrypted\n");
	SUSFS_LOGI("cleaning up fsnotify sdcard watch\n");

	grp = xchg(&g, NULL);
	if (grp)
		fsnotify_destroy_group(grp);

	inode = xchg(&g_watch.inode, NULL);
	if (inode)
		iput(inode);

	if (g_watch.kpath.mnt) {
		path_put(&g_watch.kpath);
		memset(&g_watch.kpath, 0, sizeof(g_watch.kpath));
	}
}

static int watch_one_dir(struct watch_dir *wd)
{
	int ret = kern_path(wd->path, LOOKUP_FOLLOW, &wd->kpath);
	if (ret) {
		SUSFS_LOGI("path not ready: %s (%d)\n", wd->path, ret);
		return ret;
	}
	wd->inode = d_backing_inode(wd->kpath.dentry);
	if (!wd->inode) {
		SUSFS_LOGE("wd->inode is NULL\n");
		path_put(&wd->kpath);
		return -ENOENT;
	}
	ihold(wd->inode);

	ret = add_mark_on_inode(wd->inode, wd->mask, &wd->mark);
	if (ret) {
		SUSFS_LOGE("add mark failed for %s (%d)\n", wd->path, ret);
		iput(wd->inode);
		wd->inode = NULL;
		path_put(&wd->kpath);
		return ret;
	}
	SUSFS_LOGI("watching %s\n", wd->path);
	return 0;
}

/*
 * fsnotify handler — runs inside an SRCU read section held by fsnotify().
 * Must not block or call fsnotify_destroy_group() (which internally calls
 * synchronize_srcu on the same SRCU struct, causing a permanent deadlock).
 * Cleanup is deferred to a delayed_work that runs outside the SRCU context.
 */
static SUSFS_DECL_FSNOTIFY_OPS(susfs_handle_sdcard_inode_event)
{
	if (!file_name || strlen((const char *)file_name) != 7 ||
	    memcmp(file_name, "Android", 7))
		return 0;

	if (test_and_set_bit(0, &sdcard_cleanup_scheduled))
		return 0;

	SUSFS_LOGI("'%s' detected, mask: 0x%x\n", SDCARD_ANDROID_PATH, mask);
	SUSFS_LOGI("deferring cleanup for 5 seconds\n");
	queue_delayed_work(system_unbound_wq, &sdcard_cleanup_dwork, 5 * HZ);
	return 0;
}

static const struct fsnotify_ops fsnotify_ops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	.handle_inode_event = susfs_handle_sdcard_inode_event,
#else
	.handle_event = susfs_handle_sdcard_inode_event,
#endif
};

static void __maybe_unused m_free(struct fsnotify_mark *m)
{
	if (m) {
		kfree(m);
	}
}

static int add_mark_on_inode(struct inode *inode, u32 mask,
								struct fsnotify_mark **out)
{
	struct fsnotify_mark *m;
	int ret;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

/* From KernelSU */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	fsnotify_init_mark(m, g);
	m->mask = mask;
	ret = fsnotify_add_inode_mark(m, inode, 0);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	fsnotify_init_mark(m, g);
	m->mask = mask;
	ret = fsnotify_add_mark(m, inode, NULL, 0);
#else
	fsnotify_init_mark(m, m_free);
	m->mask = mask;
	ret = fsnotify_add_mark(m, g, inode, NULL, 0);
#endif

	if (ret) {
		fsnotify_put_mark(m);
		return -EINVAL;
	}
	*out = m;
	return 0;
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

void susfs_reload_dynamic_socks(void)
{
	susfs_load_dynamic_socks(NULL);
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

#define SUSFS_MAX_CUSTOM_PORTS 32
static unsigned short susfs_custom_ports[SUSFS_MAX_CUSTOM_PORTS] = {
	2403,  /* ADB wireless Xiaomi/AOSP */
	5555,  /* ADB standard TCP */
	27042, /* Frida server default */
};
static int susfs_custom_ports_count = 3;
static DEFINE_SPINLOCK(susfs_ports_lock);

bool susfs_is_sus_port(unsigned short port)
{
	unsigned long flags;
	int i;

	if (port == 0)
		return false;

	spin_lock_irqsave(&susfs_ports_lock, flags);
	for (i = 0; i < susfs_custom_ports_count; i++) {
		if (susfs_custom_ports[i] == port) {
			spin_unlock_irqrestore(&susfs_ports_lock, flags);
			return true;
		}
	}
	spin_unlock_irqrestore(&susfs_ports_lock, flags);
	return false;
}

void susfs_add_sus_port(void __user **user_info)
{
	struct st_susfs_sus_port info = {0};
	unsigned long flags;
	int i;

	if (copy_from_user(&info, (struct st_susfs_sus_port __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy;
	}

	if (info.target_port == 0) {
		info.err = -EINVAL;
		goto out_copy;
	}

	spin_lock_irqsave(&susfs_ports_lock, flags);
	for (i = 0; i < susfs_custom_ports_count; i++) {
		if (susfs_custom_ports[i] == info.target_port) {
			spin_unlock_irqrestore(&susfs_ports_lock, flags);
			info.err = 0;
			goto out_copy;
		}
	}
	if (susfs_custom_ports_count < SUSFS_MAX_CUSTOM_PORTS) {
		susfs_custom_ports[susfs_custom_ports_count++] = info.target_port;
		info.err = 0;
	} else {
		info.err = -ENOSPC;
	}
	spin_unlock_irqrestore(&susfs_ports_lock, flags);

out_copy:
	if (copy_to_user(&((struct st_susfs_sus_port __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

void susfs_add_sus_unix_socket(void __user **user_info)
{
	struct st_susfs_sus_unix_socket info = {0};
	unsigned long flags;
	int i;

	if (copy_from_user(&info, (struct st_susfs_sus_unix_socket __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy;
	}

	if (info.target_pattern[0] == '\0') {
		info.err = -EINVAL;
		goto out_copy;
	}

	spin_lock_irqsave(&susfs_socks_lock, flags);
	for (i = 0; i < susfs_custom_socks_count; i++) {
		if (!strcmp(susfs_custom_socks[i], info.target_pattern)) {
			spin_unlock_irqrestore(&susfs_socks_lock, flags);
			info.err = 0;
			goto out_copy;
		}
	}
	if (susfs_custom_socks_count < SUSFS_MAX_CUSTOM_SOCKS) {
		strlcpy(susfs_custom_socks[susfs_custom_socks_count++], info.target_pattern, SUSFS_SOCK_PATTERN_LEN);
		info.err = 0;
	} else {
		info.err = -ENOSPC;
	}
	spin_unlock_irqrestore(&susfs_socks_lock, flags);

out_copy:
	if (copy_to_user(&((struct st_susfs_sus_unix_socket __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

void susfs_filter_environ_buffer(char *buf, size_t *len)
{
	static const char * const bad_env[] = {
		"MAGISK_INJ",
		"KSU_INJ",
		"KSU_EXTRA",
		"LSP_DEX2OAT",
	};
	char *p, *end;
	int i;

	if (!buf || !len || *len == 0)
		return;

	p = buf;
	end = buf + *len;
	while (p < end) {
		size_t item_len = strlen(p);
		bool drop = false;

		for (i = 0; i < ARRAY_SIZE(bad_env); i++) {
			if (susfs_starts_with(p, bad_env[i])) {
				drop = true;
				break;
			}
		}

		if (drop) {
			size_t rem = end - (p + item_len + 1);
			memmove(p, p + item_len + 1, rem);
			*len -= (item_len + 1);
			end = buf + *len;
		} else {
			p += item_len + 1;
		}
	}
}

void susfs_spoof_comm(struct task_struct *task, char *buf)
{
	static const char * const sus_comms[] = {
		"zygisk",
		"lspd",
		"magisk",
		"ksud",
	};
	int i;

	if (!buf)
		return;

	for (i = 0; i < ARRAY_SIZE(sus_comms); i++) {
		if (strnstr(buf, sus_comms[i], TASK_COMM_LEN)) {
			strlcpy(buf, "Jit thread pool", TASK_COMM_LEN);
			return;
		}
	}
}

bool susfs_is_sus_fd(struct file *file)
{
	struct inode *inode;

	if (!file || susfs_is_current_app_allowed_su())
		return false;

	inode = file_inode(file);
	if (!inode)
		return false;

	if (test_bit(AS_FLAGS_SUS_PATH, &inode->i_state))
		return true;

	if (file->f_path.dentry && file->f_path.dentry->d_name.name) {
		const char *name = file->f_path.dentry->d_name.name;
		if (strnstr(name, "usap_pool", 9) ||
		    strnstr(name, "zygisk", 6) ||
		    strnstr(name, "lspd", 4))
			return true;
	}

	return false;
}
#endif
