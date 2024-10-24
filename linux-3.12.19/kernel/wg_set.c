#ifndef	CONFIG_CRASH_DUMP

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>

static	struct proc_dir_entry* proc_symbol_file;

static	unsigned* wg_ptr;
static  char wg_str[128];

static	int proc_write_symbol(struct file *file, const char *buffer,
                              unsigned long count, void *data)
{
  int j;
  char*  strend;
  unsigned  val;

  if (count > (sizeof(wg_str)-4)) count = sizeof(wg_str)-4;
  if(copy_from_user(wg_str, buffer, count))
    return -EFAULT;

  wg_str[count] = '\0';

  for (j = 0; wg_str[j]; j++) {
    if (wg_str[j] <= ' ') break;
    if (wg_str[j] == '=') break;
  }

  wg_str[j++] = 0;

  if ((wg_ptr = (int*)__symbol_get(wg_str))) {
    if (   wg_str[j] >= '0') {
      if ((wg_str[j] == '0') && (wg_str[j + 1] =='x'))
        val = simple_strtol(&wg_str[j+2], &strend, 16);
      else
        val = simple_strtol(&wg_str[j+0], &strend, 10);
      printk(KERN_DEBUG "Setting %s [%p] to %s\n", wg_str, wg_ptr, &wg_str[j]);
      *wg_ptr = val;
    }
  }

  return count;
}

static int proc_read_symbol(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  if (wg_ptr)
    return sprintf(page, "%s %d 0x%x\n", wg_str, *wg_ptr, *wg_ptr);
  return -EINVAL;
}

static	struct proc_dir_entry* proc_wg_set_dir;

// Set up the proc filesystem

static	int __init wg_set_init(void)
{
  printk(KERN_EMERG "\n%s: Built " __DATE__ " " __TIME__ "\n\n", __FUNCTION__);

  // Create /proc/wg_set
  proc_wg_set_dir = proc_mkdir("wg_set", NULL);
  if (!proc_wg_set_dir) return -EPERM;

  // Create symbol entry
  proc_symbol_file = create_proc_entry("symbol", 0644, proc_wg_set_dir);
  if (proc_symbol_file) {
      proc_symbol_file->read_proc  = proc_read_symbol;
      proc_symbol_file->write_proc = proc_write_symbol;
  }

  // Return no errors
  return 0;
}

static void __exit wg_set_exit(void)
{
  // Remove /proc entries

  if (proc_symbol_file)
    remove_proc_entry("symbol", proc_wg_set_dir);

  if (proc_wg_set_dir)
    remove_proc_entry("wg_set", NULL);
}

module_init(wg_set_init);
module_exit(wg_set_exit);

MODULE_LICENSE("GPL");

#endif	// !CONFIG_CRASH_DUMP
