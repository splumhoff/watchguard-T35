#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/sysrq.h>
#include <linux/kallsyms.h>
#include <linux/profile.h>

static int	loglevel;		// Saved log level
static int	interval = 600 * HZ;	// Stack dump interval
static unsigned long previous = 0;	// Previous dump time

static int wg_hang_hook(struct pt_regs *regs)
{
  if ((jiffies - previous) >= interval) {

    // Raise log level
    loglevel = console_loglevel;
    console_loglevel = 9;

    previous = jiffies;
    printk(KERN_EMERG "\n\n%s: CPU %2d Time %lu\n\n",
           __FUNCTION__, raw_smp_processor_id(), jiffies);

    __handle_sysrq('m', 0);
    __handle_sysrq('p', 0);
    __handle_sysrq('q', 0);
    __handle_sysrq('t', 0);
    __handle_sysrq('w', 0);

    // dump_stack();

    // Restore log level
    console_loglevel = loglevel;
  }

  return 0;
}

static	int __init wg_hang_init(void)
{
  int rc;

  printk(KERN_EMERG "\n%s: Built " __DATE__ " " __TIME__ " CPUs %2d HZ %d\n\n",
         __FUNCTION__, NR_CPUS, HZ);

  // Hook the timer to dump stack
  rc = register_timer_hook(wg_hang_hook);
  if (rc)
    printk(KERN_EMERG "Timer Hook returned %d\n", rc);

  // Set previous dump time to now
  previous = jiffies;

  // Return no errors
  return 0;
}

static void __exit wg_hang_exit(void)
{
  // Unhook timer
  unregister_timer_hook(wg_hang_hook);

}

module_init(wg_hang_init);
module_exit(wg_hang_exit);

MODULE_LICENSE("GPL");
