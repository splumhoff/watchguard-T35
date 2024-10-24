#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/kernel_stat.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/profile.h>

#ifdef	CONFIG_WG_ARCH_FREESCALE

#include <asm/processor.h>
#include <asm/cputable.h>
#include <asm/pmc.h>

#define	KERN_SHOW KERN_EMERG

#define	NCOUNT    12

#define	BE_BIT(n) (1<<(31-n))

#define	MSB	  BE_BIT(0)
#define	FAC	  BE_BIT(0)
#define	PMIE	  BE_BIT(1)
#define	FCECE	  BE_BIT(2)
#define	CE	  BE_BIT(5)

#define	OFFSET(x) ((void*)(((char*)gut_iomap) + (0x1000 | (x))))
#define	EVENT(x)  ((x) << 16)

// Data storage for /proc
static	struct proc_dir_entry* proc_wg_perf_dir;
static	struct proc_dir_entry* proc_enable_file;
static	struct proc_dir_entry* proc_counters_file;

// Global utilities map
static struct device_node* gut_node;
static __be32 __iomem*	   gut_iomap;

// Sampling counter
static int pm_counter;

// Counter registers
static unsigned long* pmgc0;
static unsigned long* pmlca[NCOUNT];
static unsigned long* pmlcb[NCOUNT];
static unsigned long* pmc[NCOUNT];

static unsigned long      count32[NCOUNT];
static unsigned long long count64[NCOUNT];

// Time variables
static unsigned long begin;
static unsigned long delta;

static void wg_perf_reset(void)
{
  int j;

   pmgc0 = OFFSET(0);

  *pmgc0 = FAC;

  for (j = 0; j < NCOUNT; j++) {
    pmlca[j] = OFFSET(0x10 + (j << 4)); *pmlca[j] = 0;
    pmlcb[j] = OFFSET(0x14 + (j << 4)); *pmlcb[j] = 0;
    pmc[j]   = OFFSET(0x18 + (j << 4)); *pmc[j]   = 0;
  }

  *pmlca[1] = EVENT( 27); *pmc[1] = 0;
  *pmlca[4] = EVENT(121); *pmc[4] = 0;
  *pmlca[5] = EVENT(115); *pmc[5] = 0;

  *pmgc0 = 0;

  memset(count64, 0, sizeof(count64));

  // Save begin time
  begin = jiffies;
}

static void wg_perf_ce(void)
{
  *pmgc0    = FAC;
  *pmlca[5] = EVENT(115) | CE;
  *pmc[5]   = -pm_counter & ~MSB;
  *pmgc0    = PMIE | FCECE;
}

static void wg_perf_hook(struct pt_regs *regs)
{
#ifdef CONFIG_PROFILING
  extern int Hw_SampleHook(struct pt_regs *regs);

  Hw_SampleHook(regs);
  if (pm_counter > 0)
    wg_perf_ce();
#endif
}

static int wg_perf_overflow(struct pt_regs *regs)
{
  int j;

  if (NR_CPUS > 1)
  if (raw_smp_processor_id() > 0)
    return 0;

  // if (pm_counter <= 0)
  for (j = 0; j < NCOUNT; j++) {
    unsigned long value = *pmc[j];

    count64[j] += (value - count32[j]);
    count32[j]  = value;
  }

  return 0;
}

int percent(unsigned long long value64, unsigned long long total64)
{
  unsigned long value;
  unsigned long total;

  while (total64 > 10000000) {
    total64 >>= 1;
    value64 >>= 1;
  }
  
  value = value64;
  total = total64;

  return total ? ((value * 100) / total) : 0;
}

// Print the counters
static	int proc_read_counters(char* page, char** start, off_t off,
                               int count,  int* eof,     void* data)
{
  int z = 0;
  int hundreths;

  unsigned long long tick64;
  unsigned long long busy64;
  unsigned long long hits64;
  unsigned long long miss64;

  tick64 = count64[0]; count64[0] = 0;
  busy64 = count64[1]; count64[1] = 0;
  hits64 = count64[4]; count64[4] = 0;
  miss64 = count64[5]; count64[5] = 0;

  delta = jiffies - begin;
  begin = jiffies;

  hundreths = (delta * 100) / HZ;

  z += sprintf(&page[z], "Elapsed     Time %14d.%02d Seconds\n",
               hundreths / 100, hundreths % 100);

  z += sprintf(&page[z], "L2 Platform Cycles %15llu DDR RW %15llu %5d %c\n",
               tick64, busy64, percent(busy64, tick64), '%');

  z += sprintf(&page[z], "L2 Cache    Hits   %15llu Misses %15llu %5d %c\n",
               hits64, miss64, percent(miss64, hits64 + miss64), '%');

  return z;
}

static	int proc_write_counters(struct file* file,   const char* buffer,
                                unsigned long count, void* data)
{
  wg_perf_reset();

  return count;
}

static int proc_write_enable(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  int  rc = 0;
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  if (pm_counter > 0) release_pmc_hardware();
  pm_counter = simple_strtol(str, &strend, 0);
  if (pm_counter > 0) rc = reserve_pmc_hardware(wg_perf_hook);
  if (rc)
    printk(KERN_EMERG "Performance Hook returned %d\n", rc);

  wg_perf_reset();
  wg_perf_ce();

  return count;
}

static int proc_read_enable(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", pm_counter);
}

// Set up the proc filesystem
static	int __init wg_perf_init(void)
{
  int rc;

  printk(KERN_EMERG "\n%s: Built " __DATE__ " " __TIME__ " CPUs %d\n\n",
         __FUNCTION__, NR_CPUS);

  gut_node = of_find_node_by_name(NULL, "global-utilities");
  if (!gut_node)  return -ENOENT;

  gut_iomap = of_iomap(gut_node, 0);
  of_node_put(gut_node);
  if (!gut_iomap) return -ENOMEM;

  // Create /proc/wg_perf/counters
  proc_wg_perf_dir = proc_mkdir("wg_perf", NULL);
  if (proc_wg_perf_dir) {

    proc_enable_file   = create_proc_entry("enable",   0644, proc_wg_perf_dir);
    if (proc_enable_file) {
      proc_enable_file->read_proc    = proc_read_enable;
      proc_enable_file->write_proc   = proc_write_enable;
    }

    proc_counters_file = create_proc_entry("counters", 0644, proc_wg_perf_dir);
    if (proc_counters_file) {
      proc_counters_file->read_proc  = proc_read_counters;
      proc_counters_file->write_proc = proc_write_counters;
    }
  }

  // Reset counters
  wg_perf_reset();
  printk(KERN_SHOW "Map %p Ticks %9lu\n", pmgc0, *pmc[0]);

  // Hook the timer to collect 32 bit  counters
  rc = register_timer_hook(wg_perf_overflow);
  if (rc)
    printk(KERN_EMERG "Performance Hook returned %d\n", rc);

  // Return no errors
  return 0;
}

static void __exit wg_perf_exit(void)
{
  // Unhook timer
  unregister_timer_hook(wg_perf_overflow);

  // Unhook performance monitor
  if (pm_counter > 0)     release_pmc_hardware();

  // Remove /proc entries
  if (proc_counters_file) remove_proc_entry("counters", proc_wg_perf_dir);
  if (proc_enable_file)   remove_proc_entry("enable",   proc_wg_perf_dir);
  if (proc_wg_perf_dir)   remove_proc_entry("wg_perf",  NULL);
}

module_init(wg_perf_init);
module_exit(wg_perf_exit);

MODULE_LICENSE("GPL");

#endif	// CONFIG_WG_ARCH_FREESCALE
