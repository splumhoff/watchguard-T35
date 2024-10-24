#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/sysrq.h>
#include <linux/kallsyms.h>
#include <linux/profile.h>

int type = 0;
module_param(type, int, 0444);
MODULE_PARM_DESC(type, "0=Spin 1=Read -1=Write");

spinlock_t wg_spin_lockup = __SPIN_LOCK_UNLOCKED(wg_spin_lockup);
rwlock_t   wg_rw_lockup   = __RW_LOCK_UNLOCKED(wg_rw_lockup);

int __init wg_lockup_init(void)
{
  printk(KERN_EMERG "\n%s: Built " __DATE__ " " __TIME__ " CPUs %2d HZ %d\n\n",
         __FUNCTION__, NR_CPUS, HZ);

  if (type <  0) {
    write_lock(&wg_rw_lockup);
    printk(KERN_EMERG "Get locked write lock\n");
    write_lock(&wg_rw_lockup);
    printk(KERN_EMERG "Got locked write lock\n");
  } else

  if (type >  0) {
    write_lock(&wg_rw_lockup);
    printk(KERN_EMERG "Get locked read  lock\n");
    read_lock(&wg_rw_lockup);
    printk(KERN_EMERG "Got locked read  lock\n");
  } else

  if (type == 0) {
    spin_lock(&wg_spin_lockup);
    printk(KERN_EMERG "Get locked spin  lock\n");
    spin_lock(&wg_spin_lockup);
    printk(KERN_EMERG "Got locked spin  lock\n");
  }

  return -ENOSYS;
}

void __exit wg_lockup_exit(void)
{
}

module_init(wg_lockup_init);
module_exit(wg_lockup_exit);

MODULE_LICENSE("GPL");
