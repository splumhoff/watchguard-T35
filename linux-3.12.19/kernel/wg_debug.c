#ifndef	CONFIG_CRASH_DUMP

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/kernel_stat.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>
#include <linux/netdevice.h>
#include <linux/netfilter_ipv4.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/profile.h>
#include <linux/spinlock.h>
#include <net/ip.h>
#include <net/xfrm.h>

struct proc_dir_entry* proc_wg_debug_dir;

// Add entries like this one for additional debug knobs

#undef	_WG_DEBUG_EXAMPLE_
#ifdef	_WG_DEBUG_EXAMPLE_

int wg_example;

struct proc_dir_entry* proc_wg_example_file;

int proc_write_wg_example(struct file *file, const char *buffer,
                          unsigned long count, void *data)
{
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  wg_example = simple_strtol(str, &strend, 0);

  return count;
}

int proc_read_wg_example(char *page, char **start, off_t off,
                         int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", wg_example);
}

#endif

struct proc_dir_entry* proc_wg_napi_poll_file;

int proc_read_wg_napi_poll(char *page, char **start, off_t off,
                           int count, int *eof, void *data)
{
  int j, z = 0;

  for (j = 0; j < num_online_cpus(); j++) {
    struct softnet_data* sd   = &per_cpu(softnet_data, j);
    struct list_head*    head = &(sd->poll_list);
    struct list_head*    this = head;

    if (head->next == head->prev) continue;

    z += sprintf(&page[z], "CPU %2d Polling", j);

    while ((this = this->next) != head)
    if (!this) break; else {
      struct napi_struct* napi = (struct napi_struct*)this;
      if (napi->dev)
        z += sprintf(&page[z], " %s", napi->dev->name);
    }

    z += sprintf(&page[z], "\n");
  }

  return z;
}

// Set up the proc filesystem

int __init wg_debug_init(void)
{
  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ " CPUs %d\n\n",
         __FUNCTION__, num_online_cpus());

  // Create /proc/wg_debug
  proc_wg_debug_dir = proc_mkdir("wg_debug", NULL);
  if (!proc_wg_debug_dir) return -EPERM;

#ifdef	_WG_DEBUG_EXAMPLE_
  // Create each example entry
  proc_wg_example_file = create_proc_entry("example",
                                           0644, proc_wg_debug_dir);
  if (proc_wg_example_file) {
      proc_wg_example_file->read_proc    = proc_read_wg_example;
      proc_wg_example_file->write_proc   = proc_write_wg_example;
  }
#endif

  // Create each napi_poll entry
  proc_wg_napi_poll_file = create_proc_entry("napi_poll",
                                             0444, proc_wg_debug_dir);
  if (proc_wg_napi_poll_file) {
      proc_wg_napi_poll_file->read_proc  = proc_read_wg_napi_poll;
  }

  // Return no errors
  return 0;
}

void __exit wg_debug_exit(void)
{
  // Remove /proc entries

  if (proc_wg_napi_poll_file)
    remove_proc_entry("napi_poll",      proc_wg_debug_dir);

#ifdef	_WG_DEBUG_EXAMPLE_
  if (proc_wg_example_file)
    remove_proc_entry("example",        proc_wg_debug_dir);
#endif

  if (proc_wg_debug_dir)
    remove_proc_entry("wg_debug",       NULL);
}

module_init(wg_debug_init);
module_exit(wg_debug_exit);

MODULE_LICENSE("GPL");

#endif	// !CONFIG_CRASH_DUMP
