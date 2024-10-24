#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>

struct proc_dir_entry *wgdir;

struct proc_dir_entry*
proc_wg_root(void)
{
	return wgdir;
}
EXPORT_SYMBOL(proc_wg_root);

int __init
proc_wg_init(void)
{
	if (wgdir == NULL)
	wgdir = proc_mkdir("wg", NULL);
	if (wgdir == NULL)
		return -ENOMEM;
	return 0;
}
