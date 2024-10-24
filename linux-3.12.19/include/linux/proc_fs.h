/*
 * The proc filesystem constants/structures
 */
#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

#include <linux/types.h>
#include <linux/fs.h>

struct proc_dir_entry;

#ifdef CONFIG_PROC_FS

#ifdef	CONFIG_WG_PLATFORM // WG:JB Put old proc API back in

extern struct proc_dir_entry *proc_wg_root(void); // WG:TW /proc/wg

#define	CONFIG_WG_PLATFORM_OLD_PROC_API	1

#endif	// CONFIG_WG_PLATFORM

#ifdef	CONFIG_WG_PLATFORM_OLD_PROC_API

#include <linux/proc_ns.h>

typedef	int (read_proc_t)(char *page, char **start, off_t off,
			  int count, int *eof, void *data);
typedef	int (write_proc_t)(struct file *file, const char __user *buffer,
			   unsigned long count, void *data);

extern struct proc_dir_entry *create_proc_entry(const char *name, umode_t mode,
						struct proc_dir_entry *parent);

extern void set_proc_read( struct proc_dir_entry *entry, read_proc_t*  func);
extern void set_proc_write(struct proc_dir_entry *entry, write_proc_t* func);

/*
 * This is not completely implemented yet. The idea is to
 * create an in-memory tree (like the actual /proc filesystem
 * tree) of these proc_dir_entries, so that we can dynamically
 * add new files to /proc.
 *
 * The "next" pointer creates a linked list of one /proc directory,
 * while parent/subdir create the directory structure (every
 * /proc file has a parent, but "subdir" is NULL for all
 * non-directory entries).
 */
struct proc_dir_entry {
	unsigned int low_ino;
	umode_t mode;
	nlink_t nlink;
	kuid_t uid;
	kgid_t gid;
	loff_t size;
	const struct inode_operations *proc_iops;
	const struct file_operations *proc_fops;
	struct proc_dir_entry *next, *parent, *subdir;
	void *data;
	read_proc_t *read_proc;   // WG:JB Add read  function
	write_proc_t *write_proc; // WG:JB Add write function
	atomic_t count;		/* use count */
	atomic_t in_use;	/* number of callers into module in progress; */
			/* negative -> it's going away RSN */
	// WG:JB int pde_users;  /* number of callers into module in progress */
	struct completion *pde_unload_completion;
	struct list_head pde_openers;	/* who did ->open, but not ->release */
	spinlock_t pde_unload_lock; /* proc_fops checks and pde_users bumps */
	u8 namelen;
	char name[];
};

union proc_op {
	int (*proc_get_link)(struct dentry *, struct path *);
	int (*proc_read)(struct task_struct *task, char *page);
	int (*proc_show)(struct seq_file *m,
		struct pid_namespace *ns, struct pid *pid,
		struct task_struct *task);
};

struct proc_inode {
	struct pid *pid;
	int fd;
	union proc_op op;
	struct proc_dir_entry *pde;
	struct ctl_table_header *sysctl;
	struct ctl_table *sysctl_entry;
	struct proc_ns ns;
	struct inode vfs_inode;
};

/*
 * General functions
 */
static inline struct proc_inode *PROC_I(const struct inode *inode)
{
	return container_of(inode, struct proc_inode, vfs_inode);
}

static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
	return PROC_I(inode)->pde;
}

static inline void *__PDE_DATA(const struct inode *inode)
{
	return PDE(inode)->data;
}

static inline struct pid *proc_pid(struct inode *inode)
{
	return PROC_I(inode)->pid;
}

static inline struct proc_dir_entry *create_proc_read_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, 
	read_proc_t *read_proc, void * data)
{
	struct proc_dir_entry *res=create_proc_entry(name,mode,base);
	if (res) {
		res->read_proc=read_proc;
		res->data=data;
	}
	return res;
}
 
#endif	// CONFIG_WG_PLATFORM_OLD_PROC_API

extern void proc_root_init(void);
extern void proc_flush_task(struct task_struct *);

extern struct proc_dir_entry *proc_symlink(const char *,
		struct proc_dir_entry *, const char *);
extern struct proc_dir_entry *proc_mkdir(const char *, struct proc_dir_entry *);
extern struct proc_dir_entry *proc_mkdir_data(const char *, umode_t,
					      struct proc_dir_entry *, void *);
extern struct proc_dir_entry *proc_mkdir_mode(const char *, umode_t,
					      struct proc_dir_entry *);
 
extern struct proc_dir_entry *proc_create_data(const char *, umode_t,
					       struct proc_dir_entry *,
					       const struct file_operations *,
					       void *);

static inline struct proc_dir_entry *proc_create(
	const char *name, umode_t mode, struct proc_dir_entry *parent,
	const struct file_operations *proc_fops)
{
	return proc_create_data(name, mode, parent, proc_fops, NULL);
}

extern void proc_set_size(struct proc_dir_entry *, loff_t);
extern void proc_set_user(struct proc_dir_entry *, kuid_t, kgid_t);
extern void *PDE_DATA(const struct inode *);
extern void *proc_get_parent_data(const struct inode *);
extern void proc_remove(struct proc_dir_entry *);
extern void remove_proc_entry(const char *, struct proc_dir_entry *);
extern int remove_proc_subtree(const char *, struct proc_dir_entry *);

#else /* CONFIG_PROC_FS */

static inline void proc_flush_task(struct task_struct *task)
{
}

static inline struct proc_dir_entry *proc_symlink(const char *name,
		struct proc_dir_entry *parent,const char *dest) { return NULL;}
static inline struct proc_dir_entry *proc_mkdir(const char *name,
	struct proc_dir_entry *parent) {return NULL;}
static inline struct proc_dir_entry *proc_mkdir_data(const char *name,
	umode_t mode, struct proc_dir_entry *parent, void *data) { return NULL; }
static inline struct proc_dir_entry *proc_mkdir_mode(const char *name,
	umode_t mode, struct proc_dir_entry *parent) { return NULL; }
#define proc_create(name, mode, parent, proc_fops) ({NULL;})
#define proc_create_data(name, mode, parent, proc_fops, data) ({NULL;})

static inline void proc_set_size(struct proc_dir_entry *de, loff_t size) {}
static inline void proc_set_user(struct proc_dir_entry *de, kuid_t uid, kgid_t gid) {}
static inline void *PDE_DATA(const struct inode *inode) {BUG(); return NULL;}
static inline void *proc_get_parent_data(const struct inode *inode) { BUG(); return NULL; }

static inline void proc_remove(struct proc_dir_entry *de) {}
#define remove_proc_entry(name, parent) do {} while (0)
static inline int remove_proc_subtree(const char *name, struct proc_dir_entry *parent) { return 0; }

#endif /* CONFIG_PROC_FS */

static inline struct proc_dir_entry *proc_net_mkdir(
	struct net *net, const char *name, struct proc_dir_entry *parent)
{
	return proc_mkdir_data(name, 0, parent, net);
}

#endif /* _LINUX_PROC_FS_H */
