/*
 *  linux/fs/proc/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/pid_namespace.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/completion.h>
#include <linux/poll.h>
#include <linux/printk.h>
#include <linux/file.h>
#include <linux/limits.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/magic.h>

#include <asm/uaccess.h>

#include "internal.h"

static void proc_evict_inode(struct inode *inode)
{
	struct proc_dir_entry *de;
	struct ctl_table_header *head;
	const struct proc_ns_operations *ns_ops;
	void *ns;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);

	/* Stop tracking associated processes */
	put_pid(PROC_I(inode)->pid);

	/* Let go of any associated proc directory entry */
	de = PROC_I(inode)->pde;
	if (de)
		pde_put(de);
	head = PROC_I(inode)->sysctl;
	if (head) {
		rcu_assign_pointer(PROC_I(inode)->sysctl, NULL);
		sysctl_head_put(head);
	}
	/* Release any associated namespace */
	ns_ops = PROC_I(inode)->ns.ns_ops;
	ns = PROC_I(inode)->ns.ns;
	if (ns_ops && ns)
		ns_ops->put(ns);
}

static struct kmem_cache * proc_inode_cachep;

static struct inode *proc_alloc_inode(struct super_block *sb)
{
	struct proc_inode *ei;
	struct inode *inode;

	ei = (struct proc_inode *)kmem_cache_alloc(proc_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	ei->pid = NULL;
	ei->fd = 0;
	ei->op.proc_get_link = NULL;
	ei->pde = NULL;
	ei->sysctl = NULL;
	ei->sysctl_entry = NULL;
	ei->ns.ns = NULL;
	ei->ns.ns_ops = NULL;
	inode = &ei->vfs_inode;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	return inode;
}

static void proc_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(proc_inode_cachep, PROC_I(inode));
}

static void proc_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, proc_i_callback);
}

static void init_once(void *foo)
{
	struct proc_inode *ei = (struct proc_inode *) foo;

	inode_init_once(&ei->vfs_inode);
}

void __init proc_init_inodecache(void)
{
	proc_inode_cachep = kmem_cache_create("proc_inode_cache",
					     sizeof(struct proc_inode),
					     0, (SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD|SLAB_PANIC),
					     init_once);
}

static int proc_show_options(struct seq_file *seq, struct dentry *root)
{
	struct super_block *sb = root->d_sb;
	struct pid_namespace *pid = sb->s_fs_info;

	if (!gid_eq(pid->pid_gid, GLOBAL_ROOT_GID))
		seq_printf(seq, ",gid=%u", from_kgid_munged(&init_user_ns, pid->pid_gid));
	if (pid->hide_pid != 0)
		seq_printf(seq, ",hidepid=%u", pid->hide_pid);

	return 0;
}

static const struct super_operations proc_sops = {
	.alloc_inode	= proc_alloc_inode,
	.destroy_inode	= proc_destroy_inode,
	.drop_inode	= generic_delete_inode,
	.evict_inode	= proc_evict_inode,
	.statfs		= simple_statfs,
	.remount_fs	= proc_remount,
	.show_options	= proc_show_options,
};

enum {BIAS = -1U<<31};

static inline int use_pde(struct proc_dir_entry *pde)
{
	return atomic_inc_unless_negative(&pde->in_use);
}

static void unuse_pde(struct proc_dir_entry *pde)
{
	if (atomic_dec_return(&pde->in_use) == BIAS)
		complete(pde->pde_unload_completion);
}

/* pde is locked */
static void close_pdeo(struct proc_dir_entry *pde, struct pde_opener *pdeo)
{
	if (pdeo->closing) {
		/* somebody else is doing that, just wait */
		DECLARE_COMPLETION_ONSTACK(c);
		pdeo->c = &c;
		spin_unlock(&pde->pde_unload_lock);
		wait_for_completion(&c);
		spin_lock(&pde->pde_unload_lock);
	} else {
		struct file *file;
		pdeo->closing = 1;
		spin_unlock(&pde->pde_unload_lock);
		file = pdeo->file;
		pde->proc_fops->release(file_inode(file), file);
		spin_lock(&pde->pde_unload_lock);
		list_del_init(&pdeo->lh);
		if (pdeo->c)
			complete(pdeo->c);
		kfree(pdeo);
	}
}

void proc_entry_rundown(struct proc_dir_entry *de)
{
	DECLARE_COMPLETION_ONSTACK(c);
	/* Wait until all existing callers into module are done. */
	de->pde_unload_completion = &c;
	if (atomic_add_return(BIAS, &de->in_use) != BIAS)
		wait_for_completion(&c);

	spin_lock(&de->pde_unload_lock);
	while (!list_empty(&de->pde_openers)) {
		struct pde_opener *pdeo;
		pdeo = list_first_entry(&de->pde_openers, struct pde_opener, lh);
		close_pdeo(de, pdeo);
	}
	spin_unlock(&de->pde_unload_lock);
}

#ifdef	CONFIG_WG_PLATFORM_OLD_PROC_API // WG:JB Put old proc API back in

#if 000|000
static void __pde_users_dec(struct proc_dir_entry *pde)
{
	pde->pde_users--;
	if (pde->pde_unload_completion && pde->pde_users == 0)
		complete(pde->pde_unload_completion);
}

void pde_users_dec(struct proc_dir_entry *pde)
{
	spin_lock(&pde->pde_unload_lock);
	__pde_users_dec(pde);
	spin_unlock(&pde->pde_unload_lock);
}
#endif

/* buffer size is one page but our output routines use some slack for overruns */
#define PROC_BLOCK_SIZE	(PAGE_SIZE - 1024)

static ssize_t
__proc_file_read(struct file *file, char __user *buf, size_t nbytes,
	       loff_t *ppos)
{
	struct inode * inode = file_inode(file);
	char 	*page;
	ssize_t	retval=0;
	int	eof=0;
	ssize_t	n, count;
	char	*start;
	struct proc_dir_entry * dp;
	unsigned long long pos;

	/*
	 * Gaah, please just use "seq_file" instead. The legacy /proc
	 * interfaces cut loff_t down to off_t for reads, and ignore
	 * the offset entirely for writes..
	 */
	pos = *ppos;
	if (pos > MAX_NON_LFS)
		return 0;
	if (nbytes > MAX_NON_LFS - pos)
		nbytes = MAX_NON_LFS - pos;

	dp = PDE(inode);
	if (!(page = (char*) __get_free_page(GFP_TEMPORARY)))
		return -ENOMEM;

	while ((nbytes > 0) && !eof) {
		count = min_t(size_t, PROC_BLOCK_SIZE, nbytes);

		start = NULL;
		if (dp->read_proc) {
			/*
			 * How to be a proc read function
			 * ------------------------------
			 * Prototype:
			 *    int f(char *buffer, char **start, off_t offset,
			 *          int count, int *peof, void *dat)
			 *
			 * Assume that the buffer is "count" bytes in size.
			 *
			 * If you know you have supplied all the data you
			 * have, set *peof.
			 *
			 * You have three ways to return data:
			 * 0) Leave *start = NULL.  (This is the default.)
			 *    Put the data of the requested offset at that
			 *    offset within the buffer.  Return the number (n)
			 *    of bytes there are from the beginning of the
			 *    buffer up to the last byte of data.  If the
			 *    number of supplied bytes (= n - offset) is 
			 *    greater than zero and you didn't signal eof
			 *    and the reader is prepared to take more data
			 *    you will be called again with the requested
			 *    offset advanced by the number of bytes 
			 *    absorbed.  This interface is useful for files
			 *    no larger than the buffer.
			 * 1) Set *start = an unsigned long value less than
			 *    the buffer address but greater than zero.
			 *    Put the data of the requested offset at the
			 *    beginning of the buffer.  Return the number of
			 *    bytes of data placed there.  If this number is
			 *    greater than zero and you didn't signal eof
			 *    and the reader is prepared to take more data
			 *    you will be called again with the requested
			 *    offset advanced by *start.  This interface is
			 *    useful when you have a large file consisting
			 *    of a series of blocks which you want to count
			 *    and return as wholes.
			 *    (Hack by Paul.Russell@rustcorp.com.au)
			 * 2) Set *start = an address within the buffer.
			 *    Put the data of the requested offset at *start.
			 *    Return the number of bytes of data placed there.
			 *    If this number is greater than zero and you
			 *    didn't signal eof and the reader is prepared to
			 *    take more data you will be called again with the
			 *    requested offset advanced by the number of bytes
			 *    absorbed.
			 */
			n = dp->read_proc(page, &start, *ppos,
					  count, &eof, dp->data);
		} else
			break;

		if (n == 0)   /* end of file */
			break;
		if (n < 0) {  /* error */
			if (retval == 0)
				retval = n;
			break;
		}

		if (start == NULL) {
			if (n > PAGE_SIZE)	/* Apparent buffer overflow */
				n = PAGE_SIZE;
			n -= *ppos;
			if (n <= 0)
				break;
			if (n > count)
				n = count;
			start = page + *ppos;
		} else if (start < page) {
			if (n > PAGE_SIZE)	/* Apparent buffer overflow */
				n = PAGE_SIZE;
			if (n > count) {
				/*
				 * Don't reduce n because doing so might
				 * cut off part of a data block.
				 */
				pr_warn("proc_file_read: count exceeded\n");
			}
		} else /* start >= page */ {
			unsigned long startoff = (unsigned long)(start - page);
			if (n > (PAGE_SIZE - startoff))	/* buffer overflow? */
				n = PAGE_SIZE - startoff;
			if (n > count)
				n = count;
		}
		
 		n -= copy_to_user(buf, start < page ? page : start, n);
		if (n == 0) {
			if (retval == 0)
				retval = -EFAULT;
			break;
		}

		*ppos += start < page ? (unsigned long)start : n;
		nbytes -= n;
		buf += n;
		retval += n;
	}
	free_page((unsigned long) page);
	return retval;
}

static ssize_t
proc_file_read(struct file *file, char __user *buf, size_t nbytes,
	       loff_t *ppos)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	ssize_t rv = -EIO;

	if (!use_pde(pde)) return -ENOENT;

	spin_lock(&pde->pde_unload_lock);
	if (!pde->proc_fops) {
		spin_unlock(&pde->pde_unload_lock);
		unuse_pde(pde);
		return rv;
	}
	// pde->pde_users++;
	spin_unlock(&pde->pde_unload_lock);

	rv = __proc_file_read(file, buf, nbytes, ppos);

	// pde_users_dec(pde);
	unuse_pde(pde);
	return rv;
}

static ssize_t
proc_file_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	ssize_t rv = -EIO;

	if (!use_pde(pde)) return -ENOENT;

	if (pde->write_proc) {
		spin_lock(&pde->pde_unload_lock);
		if (!pde->proc_fops) {
			spin_unlock(&pde->pde_unload_lock);
			unuse_pde(pde);
			return rv;
		}
		// pde->pde_users++;
		spin_unlock(&pde->pde_unload_lock);

		/* FIXME: does this routine need ppos?  probably... */
		rv = pde->write_proc(file, buffer, count, pde->data);
		// pde_users_dec(pde);
	}
	unuse_pde(pde);
	return rv;
}

static loff_t
proc_file_lseek(struct file *file, loff_t offset, int orig)
{
	loff_t retval = -EINVAL;
	switch (orig) {
	case 1:
		offset += file->f_pos;
	/* fallthrough */
	case 0:
		if (offset < 0 || offset > MAX_NON_LFS)
			break;
		file->f_pos = retval = offset;
	}
	return retval;
}

const struct file_operations wg_proc_file_operations = {
	.llseek		= proc_file_lseek,
	.read		= proc_file_read,
	.write		= proc_file_write,
};

#endif

static loff_t proc_reg_llseek(struct file *file, loff_t offset, int whence)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	loff_t rv = -EINVAL;
	if (use_pde(pde)) {
		loff_t (*llseek)(struct file *, loff_t, int);
		llseek = pde->proc_fops->llseek;
		if (!llseek)
			llseek = default_llseek;
		rv = llseek(file, offset, whence);
		unuse_pde(pde);
	}
	return rv;
}

static ssize_t proc_reg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	struct proc_dir_entry *pde = PDE(file_inode(file));
	ssize_t rv = -EIO;
	if (use_pde(pde)) {
		read = pde->proc_fops->read;
		if (read)
			rv = read(file, buf, count, ppos);
		unuse_pde(pde);
	}
	return rv;
}

static ssize_t proc_reg_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	struct proc_dir_entry *pde = PDE(file_inode(file));
	ssize_t rv = -EIO;
	if (use_pde(pde)) {
		write = pde->proc_fops->write;
		if (write)
			rv = write(file, buf, count, ppos);
		unuse_pde(pde);
	}
	return rv;
}

static unsigned int proc_reg_poll(struct file *file, struct poll_table_struct *pts)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	unsigned int rv = DEFAULT_POLLMASK;
	unsigned int (*poll)(struct file *, struct poll_table_struct *);
	if (use_pde(pde)) {
		poll = pde->proc_fops->poll;
		if (poll)
			rv = poll(file, pts);
		unuse_pde(pde);
	}
	return rv;
}

static long proc_reg_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	long rv = -ENOTTY;
	long (*ioctl)(struct file *, unsigned int, unsigned long);
	if (use_pde(pde)) {
		ioctl = pde->proc_fops->unlocked_ioctl;
		if (ioctl)
			rv = ioctl(file, cmd, arg);
		unuse_pde(pde);
	}
	return rv;
}

#ifdef CONFIG_COMPAT
static long proc_reg_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	long rv = -ENOTTY;
	long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
	if (use_pde(pde)) {
		compat_ioctl = pde->proc_fops->compat_ioctl;
		if (compat_ioctl)
			rv = compat_ioctl(file, cmd, arg);
		unuse_pde(pde);
	}
	return rv;
}
#endif

static int proc_reg_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	int rv = -EIO;
	int (*mmap)(struct file *, struct vm_area_struct *);
	if (use_pde(pde)) {
		mmap = pde->proc_fops->mmap;
		if (mmap)
			rv = mmap(file, vma);
		unuse_pde(pde);
	}
	return rv;
}

static unsigned long proc_reg_get_unmapped_area(struct file *file, unsigned long orig_addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct proc_dir_entry *pde = PDE(file_inode(file));
	unsigned long rv = -EIO;
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long) = NULL;
	if (use_pde(pde)) {
#ifdef CONFIG_MMU
		get_unmapped_area = current->mm->get_unmapped_area;
#endif
		if (pde->proc_fops->get_unmapped_area)
			get_unmapped_area = pde->proc_fops->get_unmapped_area;
		if (get_unmapped_area)
			rv = get_unmapped_area(file, orig_addr, len, pgoff, flags);
		unuse_pde(pde);
	}
	return rv;
}

static int proc_reg_open(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *pde = PDE(inode);
	int rv = 0;
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	struct pde_opener *pdeo;

	/*
	 * What for, you ask? Well, we can have open, rmmod, remove_proc_entry
	 * sequence. ->release won't be called because ->proc_fops will be
	 * cleared. Depending on complexity of ->release, consequences vary.
	 *
	 * We can't wait for mercy when close will be done for real, it's
	 * deadlockable: rmmod foo </proc/foo . So, we're going to do ->release
	 * by hand in remove_proc_entry(). For this, save opener's credentials
	 * for later.
	 */
	pdeo = kzalloc(sizeof(struct pde_opener), GFP_KERNEL);
	if (!pdeo)
		return -ENOMEM;

	if (!use_pde(pde)) {
		kfree(pdeo);
		return -ENOENT;
	}
	open = pde->proc_fops->open;
	release = pde->proc_fops->release;

	if (open)
		rv = open(inode, file);

	if (rv == 0 && release) {
		/* To know what to release. */
		pdeo->file = file;
		/* Strictly for "too late" ->release in proc_reg_release(). */
		spin_lock(&pde->pde_unload_lock);
		list_add(&pdeo->lh, &pde->pde_openers);
		spin_unlock(&pde->pde_unload_lock);
	} else
		kfree(pdeo);

	unuse_pde(pde);
	return rv;
}

static int proc_reg_release(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *pde = PDE(inode);
	struct pde_opener *pdeo;
	spin_lock(&pde->pde_unload_lock);
	list_for_each_entry(pdeo, &pde->pde_openers, lh) {
		if (pdeo->file == file) {
			close_pdeo(pde, pdeo);
			break;
		}
	}
	spin_unlock(&pde->pde_unload_lock);
	return 0;
}

static const struct file_operations proc_reg_file_ops = {
	.llseek		= proc_reg_llseek,
	.read		= proc_reg_read,
	.write		= proc_reg_write,
	.poll		= proc_reg_poll,
	.unlocked_ioctl	= proc_reg_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= proc_reg_compat_ioctl,
#endif
	.mmap		= proc_reg_mmap,
	.get_unmapped_area = proc_reg_get_unmapped_area,
	.open		= proc_reg_open,
	.release	= proc_reg_release,
};

#ifdef CONFIG_COMPAT
static const struct file_operations proc_reg_file_ops_no_compat = {
	.llseek		= proc_reg_llseek,
	.read		= proc_reg_read,
	.write		= proc_reg_write,
	.poll		= proc_reg_poll,
	.unlocked_ioctl	= proc_reg_unlocked_ioctl,
	.mmap		= proc_reg_mmap,
	.get_unmapped_area = proc_reg_get_unmapped_area,
	.open		= proc_reg_open,
	.release	= proc_reg_release,
};
#endif

struct inode *proc_get_inode(struct super_block *sb, struct proc_dir_entry *de)
{
	struct inode *inode = new_inode_pseudo(sb);

	if (inode) {
		inode->i_ino = de->low_ino;
		inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
		PROC_I(inode)->pde = de;

		if (de->mode) {
			inode->i_mode = de->mode;
			inode->i_uid = de->uid;
			inode->i_gid = de->gid;
		}
		if (de->size)
			inode->i_size = de->size;
		if (de->nlink)
			set_nlink(inode, de->nlink);
		WARN_ON(!de->proc_iops);
		inode->i_op = de->proc_iops;
		if (de->proc_fops) {
			if (S_ISREG(inode->i_mode)) {
#ifdef CONFIG_COMPAT
				if (!de->proc_fops->compat_ioctl)
					inode->i_fop =
						&proc_reg_file_ops_no_compat;
				else
#endif
					inode->i_fop = &proc_reg_file_ops;
			} else {
				inode->i_fop = de->proc_fops;
			}
		}
	} else
	       pde_put(de);
	return inode;
}

int proc_fill_super(struct super_block *s)
{
	struct inode *root_inode;

	s->s_flags |= MS_NODIRATIME | MS_NOSUID | MS_NOEXEC;
	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = PROC_SUPER_MAGIC;
	s->s_op = &proc_sops;
	s->s_time_gran = 1;
	
	pde_get(&proc_root);
	root_inode = proc_get_inode(s, &proc_root);
	if (!root_inode) {
		pr_err("proc_fill_super: get root inode failed\n");
		return -ENOMEM;
	}

	s->s_root = d_make_root(root_inode);
	if (!s->s_root) {
		pr_err("proc_fill_super: allocate dentry failed\n");
		return -ENOMEM;
	}

	return proc_setup_self(s);
}
