#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netdevice.h>

#include <net/dsa.h>

#include <asm/atomic.h>

char	wg_cloud[16];	// Cloud environment (if any)
EXPORT_SYMBOL(wg_cloud);

char*	wg_vm_name;	// VM  name
EXPORT_SYMBOL(wg_vm_name);

int	wg_cpus	= 1;	// CPU count
EXPORT_SYMBOL(wg_cpus);

int	wg_cpu_model;	// CPU model
EXPORT_SYMBOL(wg_cpu_model);

int	wg_cpu_version;	// CPU version
EXPORT_SYMBOL(wg_cpu_version);

int wg_get_cpu_model(void)
{
  return wg_cpu_model;
}
EXPORT_SYMBOL(wg_get_cpu_model);

spinlock_t    wg_fault_lock;	   // Lock for       fault reporter
int	      wg_fault_used;	   // Used bytes in  fault reporter
char	      wg_fault_text[2000]; // Fault reporter error buffer

atomic_t      wg_dma_errors;	   // DMA null errors
EXPORT_SYMBOL(wg_dma_errors);

atomic_long_t wg_drop_backlog;	   // Drop due to backlog queue full
EXPORT_SYMBOL(wg_drop_backlog);

atomic_long_t wg_drop_length;	   // Drop due to length error
EXPORT_SYMBOL(wg_drop_length);

atomic_long_t wg_drop_unknown;	   // Drop due to unknown type
EXPORT_SYMBOL(wg_drop_unknown);

int	      wg_backlog_high;	   // Backlog queue high    water mark
EXPORT_SYMBOL(wg_backlog_high);

int	      wg_backlog_highest;  // Backlog queue highest water mark ever
EXPORT_SYMBOL(wg_backlog_highest);

int	      wg_fips_sha;	   // FIPS SHA type
EXPORT_SYMBOL(wg_fips_sha);

int	      wg_fips_sha_err;	   // FIPS SHA auth errors
EXPORT_SYMBOL(wg_fips_sha_err);

int	      wg_fips_sha_len;	   // FIPS SHA key  length
EXPORT_SYMBOL(wg_fips_sha_len);

u8*	      wg_fips_sha_key;	   // FIPS SHA key  address
EXPORT_SYMBOL(wg_fips_sha_key);

int	      wg_fips_sha_mode0;   // FIPS SHA QAT  mode  0
EXPORT_SYMBOL(wg_fips_sha_mode0);

int	      wg_fips_aad_len;	   // FIPS AAD length
EXPORT_SYMBOL(wg_fips_aad_len);


u8*	      wg_fips_iv = NULL;   // FIPS deterministic IV
EXPORT_SYMBOL(wg_fips_iv);

// NOOP function pointer
void	      wg_noop(void)    {}
EXPORT_SYMBOL(wg_noop);

#ifdef	CONFIG_X86

#include <asm/cpufeature.h>
#include <asm/processor.h>
#include <asm/hypervisor.h>

char* wg_get_vm_name(void)
{
#ifdef	CONFIG_HYPERVISOR_GUEST
  return (x86_hyper) ? ((char*)x86_hyper->name) : wg_vm_name;
#else
  return wg_vm_name;
#endif
}
EXPORT_SYMBOL(wg_get_vm_name);

int  wg_nitrox_model;		// Nitrox    crypto chip model (if any)
EXPORT_SYMBOL(wg_nitrox_model);

int  wg_cavecreek_model;	// Cavecreek crypto chip model (if any)
EXPORT_SYMBOL(wg_cavecreek_model);

struct akcipher_alg;

int  crypto_register_akcipher(struct akcipher_alg* alg)	  { return 0; }
EXPORT_SYMBOL(crypto_register_akcipher);

void crypto_unregister_akcipher(struct akcipher_alg* alg) {}
EXPORT_SYMBOL(crypto_unregister_akcipher);

#endif

#ifdef	CONFIG_PPC

char* wg_get_vm_name(void)
{
  // Freescale don't have VMs
  return 0;
}
EXPORT_SYMBOL(wg_get_vm_name);

#endif

int	wg_dsa_count;		// DSA switch chip count
EXPORT_SYMBOL(wg_dsa_count);

int	wg_dsa_debug;		// DSA switch chip debug flags
EXPORT_SYMBOL(wg_dsa_debug);

#ifdef	CONFIG_X86
// Normalized to actual PHY map
s8		   wg_dsa_phy_map[WG_DSA_PHY];
EXPORT_SYMBOL     (wg_dsa_phy_map);

// Pointer to DSA ETH Device
struct net_device* wg_dsa_dev[2];
EXPORT_SYMBOL	  (wg_dsa_dev);

// Pointer to DSA MII PHY Bus
struct mii_bus*	   wg_dsa_bus;
EXPORT_SYMBOL	  (wg_dsa_bus);

// Pointer to PSS MII PHY Bus
struct mii_bus*    wg_pss_bus;
EXPORT_SYMBOL	  (wg_pss_bus);

// Pointer to PSS untag
int		 (*wg_pss_untag)(struct sk_buff*, __u8*);
EXPORT_SYMBOL_GPL (wg_pss_untag);
#endif

// MDIO bus release function pointer
void        (*wg_dsa_mdio_release)(void) = NULL;
EXPORT_SYMBOL(wg_dsa_mdio_release);

// SGMII link poll function pointer
int         (*wg_dsa_sgmii_poll)(int)  = NULL;
EXPORT_SYMBOL(wg_dsa_sgmii_poll);

// Global mutex for DSA
DEFINE_MUTEX (wg_dsa_mutex);
EXPORT_SYMBOL(wg_dsa_mutex);

#ifdef	CONFIG_PPC

#include <linux/of_platform.h>

int	wg_boren;		// Boren  model # if any
EXPORT_SYMBOL(wg_boren);

int	wg_talitos_model;	// Freescale talitos crypto
EXPORT_SYMBOL(wg_talitos_model);

int	wg_caam_model;		// Freescale caam    crypto
EXPORT_SYMBOL(wg_caam_model);

int	wg_dpa_bug;		// Set if the FMAN has the checksum bug
EXPORT_SYMBOL(wg_dpa_bug);

#ifdef	CONFIG_PPC64 // WG:JB Dynamic flags for Freescale errata

int FSL_ERRATUM_A_005127 =  0;
EXPORT_SYMBOL(FSL_ERRATUM_A_005127);

int FSL_ERRATUM_A_005337 =  0;
EXPORT_SYMBOL(FSL_ERRATUM_A_005337);

// This is always on now so not need for dynamic flag

// int FSL_ERRATUM_A_006184 = 43;
// EXPORT_SYMBOL(FSL_ERRATUM_A_006184);

// These are not currently used

// int FSL_ERRATUM_A_006198 =  0;
// EXPORT_SYMBOL(FSL_ERRATUM_A_006198);

// int FSL_ERRATUM_A_008007 =  0;
// EXPORT_SYMBOL(FSL_ERRATUM_A_008007);

#endif	// CONFIG_PPC64

#endif

int	wg_crash_memory;	// Min crash memory for kdump
EXPORT_SYMBOL(wg_crash_memory);

// Return where the PC is now for debugging purposes
void* wg_pc(void)
{
  return (void*)_RET_IP_;
}
EXPORT_SYMBOL(wg_pc);

// Add to error text to fault report
int   wg_fault_report(char* err)
{
  int rc = -ENOSPC;
  int z  = strlen(err);

  printk(KERN_EMERG "%s\n", err);

  spin_lock_bh  (&wg_fault_lock);

  if ((wg_fault_used + z) < (sizeof(wg_fault_text)-2)) {

    strcpy(&wg_fault_text[wg_fault_used], err);
    wg_fault_used += z;

    if (wg_fault_text[wg_fault_used-1] != '\n')
        wg_fault_text[wg_fault_used++] =  '\n';

        wg_fault_text[wg_fault_used]   =  '\0';

    rc = wg_fault_used;
  }

  spin_unlock_bh(&wg_fault_lock);

  return rc;
}
EXPORT_SYMBOL(wg_fault_report);

// Stuck mutex reporting
void wg_mutex_error(const char* what, struct mutex* lock, unsigned long from)
{
  char sym[KSYM_SYMBOL_LEN];

  sprint_symbol(sym, (unsigned long)lock);
  printk(KERN_EMERG  "%s: stuck mutex [%p] count %5d %s\n",
	 what, lock, atomic_read(&lock->count), sym);

  sprint_symbol(sym, from);
  printk(KERN_EMERG  "%s: called from [%p] %s\n", what, (void*)from, sym);

  printk(KERN_EMERG  "%s: task  pid %6d state %4ld  %s\n",
	 what, current->pid, current->state, current->comm);
#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
  printk(KERN_EMERG  "%s: owner pid %6d state %4ld  %s\n",
	 what, lock->owner->pid, lock->owner->state, lock->owner->comm);
#endif
}

/*
 * wg_mutex_lock - acquire a mutex
 * @lock:    the mutex to be acquired
 * @timeout: the timeout period in jiffies
 *
 * Lock the mutex exclusively for this task. If the mutex is not
 * available right now, it will sleep for the timeout specified.
 *
 */

int wg_mutex_lock(void* lock, int timeout)
{
  if (likely(timeout == 0)) {
    // Zero timeout means do the old behavior

    mutex_lock(lock);
    return 1;

  } else {
    // Non-zero timeout so use trylock

    int ret;
    int finish = jiffies + (timeout);	   // Finish time
    int report = jiffies + (timeout >> 2); // Report time

    // Try to lock the mutex, loop on failure
    while (!(ret = mutex_trylock(lock))) {

      // See if time to abort
      if (unlikely((jiffies - finish) > 0)) {
	wg_mutex_error(__FUNCTION__, (struct mutex*)lock, _RET_IP_);
	printk(KERN_EMERG "%s: abort mutex [%p]\n", __FUNCTION__, lock);
	return -EWOULDBLOCK;
      }

      // See if time for a report
      if (unlikely((jiffies - report) > 0)) {
	wg_mutex_error(__FUNCTION__, (struct mutex*)lock, _RET_IP_);
	report = jiffies + (timeout >> 2);
      }

      // Go to sleep
      set_current_state(TASK_INTERRUPTIBLE);
      schedule_timeout(1);
    }

    // Return trylock code
    return ret;
  }
}
EXPORT_SYMBOL(wg_mutex_lock);

static	struct proc_dir_entry* proc_wg_kernel_dir;
static	struct proc_dir_entry* proc_cloud_file;
static	struct proc_dir_entry* proc_hyperv_file;
static	struct proc_dir_entry* proc_arch_model_file;
static	struct proc_dir_entry* proc_cpu_model_file;
static	struct proc_dir_entry* proc_cpu_version_file;
static	struct proc_dir_entry* proc_counters_file;
static	struct proc_dir_entry* proc_drops_file;
static	struct proc_dir_entry* proc_dma_errors_file;
static	struct proc_dir_entry* proc_fault_report_file;
static	struct proc_dir_entry* proc_licensed_cpus_file;
static	struct proc_dir_entry* proc_sw_type_file;
#ifdef	CONFIG_WG_ARCH_FREESCALE // WG:JB Only Freesclae has T Series
static	struct proc_dir_entry* proc_t_series_file;
#endif

int proc_read_cloud(char *page, char **start, off_t off,
		    int count, int *eof, void *data)
{
  return sprintf(page, "%s\n", wg_cloud);
}

int proc_read_hyperv(char *page, char **start, off_t off,
		     int count, int *eof, void *data)
{
  char*  name = wg_get_vm_name();
  return sprintf(page, "%s\n", name ? name : "");
}

int proc_read_arch_model(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
#ifdef	CONFIG_X86
#ifdef	CONFIG_X86_64
  return sprintf(page, "x86_64\n");
#else
  return sprintf(page, "x86_32\n");
#endif
#endif
#ifdef	CONFIG_PPC
#ifdef	CONFIG_PPC64
  return sprintf(page, "ppc_64\n");
#else
  return sprintf(page, "ppc_32\n");
#endif
#endif
}

int proc_read_cpu_model(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", wg_get_cpu_model());
}

int proc_read_cpu_version(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
#ifdef	CONFIG_PPC
  return sprintf(page, "%x\n", wg_cpu_version);
#else
  return sprintf(page, "%d\n", wg_cpu_version);
#endif
}

static int proc_read_drops(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
  int n;

  if (wg_backlog_high > wg_backlog_highest) wg_backlog_highest = wg_backlog_high;

  n = sprintf(page, "Backlog: High %d Highest %d  Errors: Backlog %ld Unknown %ld Length %ld\n",
	      wg_backlog_high, wg_backlog_highest,
	      atomic_long_read(&wg_drop_backlog),
	      atomic_long_read(&wg_drop_unknown),
	      atomic_long_read(&wg_drop_length));

  wg_backlog_high = 0;

  atomic_long_set(&wg_drop_backlog, 0);
  atomic_long_set(&wg_drop_unknown, 0);
  atomic_long_set(&wg_drop_length,  0);

  return n;
}

static int proc_read_dma_errors(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", atomic_read(&wg_dma_errors));
}

static int proc_read_fault_report(char *page, char **start, off_t off,
                                  int count, int *eof, void *data)
{
  int z;

  spin_lock_bh  (&wg_fault_lock);
  z = sprintf(page, "%s", wg_fault_text);
  spin_unlock_bh(&wg_fault_lock);

  return z;
}

static int proc_write_fault_report(struct file *file, const char *buffer,
                                   unsigned long count, void *data)
{
  int  z = count;
  char str[sizeof(wg_fault_text)/4];

  if (z > (sizeof(str)-2))
      z = (sizeof(str)-2);

  if (z < 3)
      z = 0;
  else
  if (copy_from_user(str, buffer, z))
      return -EFAULT;

  spin_lock_bh  (&wg_fault_lock);
  str[wg_fault_used = z] = '\0';
  strcpy(wg_fault_text, str);
  spin_unlock_bh(&wg_fault_lock);

  return count;
}

static int proc_read_licensed_cpus(char *page, char **start, off_t off,
                                   int count, int *eof, void *data)
{
  int z;

  z = sprintf(page, "%d\n", wg_cpus);

  return z;
}

static int proc_write_licensed_cpus(struct file *file, const char *buffer,
                                    unsigned long count, void *data)
{
  int  cpus;
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  cpus = simple_strtoul(str, &strend, 10);
  if (cpus > 0) wg_cpus = cpus;

  return count;
}

static int proc_read_sw_type(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
  int t = (wg_dsa_count > 0) ? 886171 : 0;

  if (has88E6176)  t = 886176;
  if (has88E6190)  t = 886190;
  if (has98DX3035) t = 983035;

  return sprintf(page, "%d\n", t);
}

#ifdef	CONFIG_WG_ARCH_FREESCALE // WG:JB Only Freesclae has T Series
static int proc_read_t_series(char *page, char **start, off_t off,
                              int count, int *eof, void *data)
{
  return sprintf(page, "%d\n", isTx0xx ? wg_cpu_model : 0);
}
#endif

#ifdef	CONFIG_CRASH_DUMP
static	struct proc_dir_entry* proc_log_file;

static int proc_write_log(struct file *file, const char *buffer,
                          unsigned long count, void *data)
{
  char        str[256];
  static char log[512];
  static int  z = 0;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
      return -EFAULT;

  str[count] = '\0';

  if ((z +  count) >= (sizeof(log)-1))
      return -EFAULT;
  else
       z += count;

  if (strchr(strcat(log, str), '\n'))  {
      printk(log);
      memset(log, z = 0, sizeof(log));
  }

  return count;
}
#endif

#ifdef	CONFIG_X86

int	wg_vashon;	// Vashon    model # if any
EXPORT_SYMBOL(wg_vashon);

int	wg_spokane;	// Spokane   model # if any
EXPORT_SYMBOL(wg_spokane);

int	wg_seattle;	// Seattle   model # if any
EXPORT_SYMBOL(wg_seattle);

int	wg_kirkland;	// Kirkland  model # if any
EXPORT_SYMBOL(wg_kirkland);

int	wg_colfax;	// Colfax    model # if any
EXPORT_SYMBOL(wg_colfax);

int	wg_rangeley;	// Rangeley  model # if any
EXPORT_SYMBOL(wg_rangeley);

int	wg_westport;	// Westport  model # if any
EXPORT_SYMBOL(wg_westport);

int	wg_winthrop;	// Winthrop  model # if any
EXPORT_SYMBOL(wg_winthrop);

int	wg_filton;	// Filton    model # if any
EXPORT_SYMBOL(wg_filton);

int	wg_rdtsc_trusted_uid; // UIDs above this are denied RDTSC

char* __init wg_get_x86_id(char* id)
{
  unsigned int *v;
  char *p, *q;

  v = (unsigned int *)&id[0];
  cpuid(0x80000002, &v[0], &v[1], &v[2],  &v[3]);
  cpuid(0x80000003, &v[4], &v[5], &v[6],  &v[7]);
  cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
  id[48] = 0;

  /*
   * Intel chips right-justify this string for some dumb reason.
   */
  p = q = &id[0];
  while (*p == ' ')
    p++;
  if (p != q) {
    while (*p)
      *q++ = *p++;
    while (q <= &id[48])
      *q++ = 0;
  }

  return id;
}

void __init wg_kernel_get_x86_model(struct cpuinfo_x86* c)
{
  wg_cpu_model = WG_CPU_X86(0);

  if (c) {
    char  ch;
    int   m = 0, v = 0;
    char* p = &c->x86_model_id[0];

    // Check for model 6
    if (c->x86 == 6) {
      wg_cpu_model = WG_CPU_686(0);
      // Get ID if we don't already have it
      if (c->extended_cpuid_level >= 0x80000004)
      if (*p == 0)
        wg_get_x86_id(p);
    }

    // Look for model numbers
    while ((ch = *p++)) {
      if  ((ch == '@')) break;
      if  ((ch >= '0') && (ch <= '9'))
	v = (v * 10) + (ch - '0');
      else {
	if  (v >= 400) m = v;
        v = (INT_MIN);
      }
      if ((m >= 400) && (v >= 0)) break;
    }

    wg_cpu_model  +=  m;
    wg_cpu_version = (v > 0) ? v : 0;

    wg_vashon   = 0;
    wg_spokane  = 0;
    wg_seattle  = 0;
    wg_kirkland = 0;
    wg_colfax   = 0;
    wg_rangeley = 0;
    wg_westport = 0;
    wg_filton   = 0;
    wg_winthrop = 0;

    if (wg_vm_name) return;

    if (wg_cpu_model == WG_CPU_686_440)  wg_vashon   = -1;
    if (wg_cpu_model == WG_CPU_686_3400) wg_vashon   =  1;
    if (wg_cpu_model == WG_CPU_686_5300) wg_vashon   =  2;

    if (wg_cpu_model == WG_CPU_686_1225) {
      if (v < 5)                         wg_spokane  =  1;
      else                               wg_winthrop =  6;
    }
    if (wg_cpu_model == WG_CPU_686_1275) {
      if (v > 2)                         wg_colfax   =  1;
      else                               wg_spokane  =  2;
    }
    if (wg_cpu_model == WG_CPU_686_5645) wg_filton   =  1;
    if (wg_cpu_model == WG_CPU_686_3558) wg_winthrop =  2;
    if (wg_cpu_model == WG_CPU_686_3538) wg_winthrop =  2;
    if (wg_cpu_model == WG_CPU_686_3338) wg_winthrop =  2;
    if (wg_cpu_model == WG_CPU_686_3508) wg_winthrop =  2;
    if (wg_cpu_model == WG_CPU_686_3308) wg_winthrop =  2;
    if (wg_cpu_model == WG_CPU_686_2758) wg_seattle  =  1;
    if (wg_cpu_model == WG_CPU_686_2758) wg_rangeley =  8;
    if (wg_cpu_model == WG_CPU_686_2558) wg_rangeley =  4;
    if (wg_cpu_model == WG_CPU_686_2358) wg_rangeley =  2;
    if (wg_cpu_model == WG_CPU_686_1820) wg_kirkland =  1;
    if (wg_cpu_model == WG_CPU_686_3420) wg_kirkland =  2;
    if (wg_cpu_model == WG_CPU_686_4360) wg_kirkland =  3;
    if (wg_cpu_model == WG_CPU_686_2630) wg_colfax   =  2;
    if (wg_cpu_model == WG_CPU_686_2680) wg_colfax   =  2;
    if (wg_cpu_model == WG_CPU_686_3050) wg_westport =  1;
    if (wg_cpu_model == WG_CPU_686_3060) wg_westport =  1;
    if (wg_cpu_model == WG_CPU_686_3150) wg_westport =  2;
    if (wg_cpu_model == WG_CPU_686_3160) wg_westport =  2;
    if (wg_cpu_model == WG_CPU_686_3900) wg_winthrop =  3;
    if (wg_cpu_model == WG_CPU_686_4400) wg_winthrop =  4;
    if (wg_cpu_model == WG_CPU_686_6100) wg_winthrop =  5;

    if (wg_cpu_model == WG_CPU_686_3400) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_5300) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_9400) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_5410) wg_cpus =  8;
    if (wg_cpu_model == WG_CPU_686_5645) wg_cpus = 24;
    if (wg_cpu_model == WG_CPU_686_1225) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_1275) wg_cpus =  8;
    if (wg_cpu_model == WG_CPU_686_2658) wg_cpus = 40;
    if (wg_cpu_model == WG_CPU_686_3558) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3538) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3508) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3338) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_3308) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_2758) wg_cpus =  8;
    if (wg_cpu_model == WG_CPU_686_2558) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_2358) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_3150) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3160) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_1820) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_3420) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_4360) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_2630) wg_cpus = 12;
    if (wg_cpu_model == WG_CPU_686_2680) wg_cpus = 20;
    if (wg_cpu_model == WG_CPU_686_3050) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_3060) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_3150) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3160) wg_cpus =  4;
    if (wg_cpu_model == WG_CPU_686_3900) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_4400) wg_cpus =  2;
    if (wg_cpu_model == WG_CPU_686_6100) wg_cpus =  4;

    if (has88E6176) wg_dsa_count = 1;
    if (has88E6190) wg_dsa_count = 1;

    wg_rdtsc_trusted_uid = 0;
  }
}
#endif

LIST_HEAD    (wg_counter_list);
spinlock_t    wg_counter_lock;

struct	wg_counter* wg_add_counter(char* name)
{
  struct wg_counter* item = kzalloc(sizeof(struct wg_counter), GFP_ATOMIC);

  if (item) {
    atomic_set(&item->count, 0);
    strncpy(    item->name,  name, sizeof(item->name)-1);

    spin_lock_bh(	    &wg_counter_lock);
    list_add(  &item->list, &wg_counter_list);
    spin_unlock_bh(	    &wg_counter_lock);
  }

  return item;
}
EXPORT_SYMBOL(wg_add_counter);

void  wg_del_counter(struct  wg_counter** item)
{
  if ( item)
  if (*item) {
    spin_lock_bh(	    &wg_counter_lock);
    list_del(&(*item)->list);
    kfree(*item);
    *item = NULL;
    spin_unlock_bh(	    &wg_counter_lock);
  }
}
EXPORT_SYMBOL(wg_del_counter);

int proc_read_counters(char *page, char **start, off_t off,
                       int count, int *eof, void *data)
{
  int	 z = 0;
  struct wg_counter* item;

  spin_lock_bh(		    &wg_counter_lock);

  for (item  = (struct wg_counter*) wg_counter_list.next;
       item != (struct wg_counter*)&wg_counter_list;
       item  = (struct wg_counter*)      item->list.next) {
    unsigned long k = atomic_read(&item->count);

    if (k == 0) continue;

    atomic_set(&item->count, 0);
    if (z <= (PAGE_SIZE-32))
        z += sprintf(page, "%s: %lu\n", item->name, k);
  }

  spin_unlock_bh(	    &wg_counter_lock);

  return  z;
}

int __init wg_kernel_init(void)
{
#ifdef	CONFIG_X86
  char*  name = wg_get_vm_name();
  static int run = 0;

  if (run++) return 0;

  wg_kernel_get_x86_model(&cpu_data(0));

  printk(" WG CPU Model %d Hypervisor '%s'\n",
	 wg_get_cpu_model(), name ? name : "Bare");
#if BITS_PER_LONG < 64
  printk(" WG VMALLOC   RESERVE %4d MB\n", __VMALLOC_RESERVE >> 20);
#endif
#endif

#ifdef	CONFIG_PPC
  struct device_node *root;
  static int run = 0;

  if (run++) return 0;

  wg_cpu_version = mfspr(SPRN_PVR);

  if ((root = of_find_node_by_path("/"))) {
    const char *model;

    if ((model = of_get_property(root, "model", NULL))) {
      if (strstr(model, "2081")) {
	wg_cpu_model     = WG_CPU_T2081;
	wg_dsa_count     = 1;
	wg_caam_model    = 1;
	wg_cpus		 = 8;
      } else
      if (strstr(model, "1042")) {
	wg_cpu_model     = WG_CPU_T1042;
	wg_dsa_count     = 1;
	wg_caam_model    = 1;
	wg_cpus		 = 4;
      } else
      if (strstr(model, "1024")) {
	wg_cpu_model     = WG_CPU_T1024;
	wg_dsa_count     = 1;
	wg_caam_model    = 1;
	wg_cpus		 = 2;
      } else
      if (strstr(model, "2020")) {
	wg_cpu_model     = WG_CPU_P2020;
	wg_dsa_count     = 2;
	wg_talitos_model = 1;
	wg_cpus		 = 2;
      } else
      if (strstr(model, "1020")) {
	wg_cpu_model     = WG_CPU_P1020;
	wg_dsa_count     = 1;
	wg_talitos_model = 1;
	wg_cpus		 = 2;
      } else
      if (strstr(model, "1011")) {
	wg_cpu_model     = WG_CPU_P1011;
	wg_dsa_count     = 1;
	wg_talitos_model = 1;
	wg_cpus		 = 1;
      } else
      if (strstr(model, "1010")) {
	wg_cpu_model     = WG_CPU_P1010;
	wg_dsa_count     = 0;
	wg_caam_model    = 1;
	wg_cpus		 = 1;
      }

      printk(" WG CPU Model %d Count %d\n", wg_get_cpu_model(), wg_cpus);
    }

    of_node_put(root);

    wg_boren   = 0;

#ifdef	CONFIG_PPC64	// WG:JB Set Dynamic FSL errata
    if (wg_cpu_model == WG_CPU_T1042) {
      FSL_ERRATUM_A_005337 = 1;
      printk(" Setting RSL Erratum A_005337\n");
    }
    else
    if (wg_cpu_model == WG_CPU_T2081) {
      FSL_ERRATUM_A_005127 = 1;
      printk(" Setting FSL Erratum A_005127\n");
    }

    wg_dpa_bug = 0;

#endif	// CONFIG_PPC64

    if ((strstr(model, "BOREN")) ||
	(ppc_proc_freq >= 666666666)) {
      if (wg_cpu_model == WG_CPU_P1011) wg_boren = 1;
      if (wg_cpu_model == WG_CPU_P1020) wg_boren = 2;
    }
  }
#endif

  printk(" WG CRASH MIN RESERVE %4d MB\n", wg_crash_memory >> 20);

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n", __FUNCTION__);

  atomic_long_set(&wg_drop_backlog, 0);
  atomic_long_set(&wg_drop_unknown, 0);
  atomic_long_set(&wg_drop_length,  0);

  // Create /proc/wg_kernel
  proc_wg_kernel_dir = proc_mkdir("wg_kernel", NULL);
  if (!proc_wg_kernel_dir) return -EPERM;

#ifdef  CONFIG_WG_PLATFORM_OLD_PROC_API // WG:JB Put old proc API back in

  // Create each wg_kernel entry

  proc_cloud_file = create_proc_entry(      "cloud",           0444,
					     proc_wg_kernel_dir);
  if (proc_cloud_file) {
    set_proc_read(proc_cloud_file,	     proc_read_cloud);
  }

  proc_hyperv_file = create_proc_entry(      "hyperv",         0444,
					     proc_wg_kernel_dir);
  if (proc_hyperv_file) {
    set_proc_read(proc_hyperv_file,	     proc_read_hyperv);
  }

  proc_arch_model_file = create_proc_entry(  "arch_model",     0444,
					     proc_wg_kernel_dir);
  if (proc_arch_model_file) {
    set_proc_read(proc_arch_model_file,	     proc_read_arch_model);
  }

  proc_cpu_model_file = create_proc_entry(   "cpu_model",      0444,
					     proc_wg_kernel_dir);
  if (proc_cpu_model_file) {
    set_proc_read(proc_cpu_model_file,	     proc_read_cpu_model);
  }

  proc_cpu_version_file = create_proc_entry( "cpu_version",    0444,
					     proc_wg_kernel_dir);
  if (proc_cpu_version_file) {
    set_proc_read(proc_cpu_version_file,     proc_read_cpu_version);
  }

  proc_counters_file = create_proc_entry(   "counters",        0444,
					     proc_wg_kernel_dir);
  if (proc_counters_file) {
    set_proc_read(proc_counters_file,	     proc_read_counters);
  }

  proc_drops_file = create_proc_entry(       "drops",          0444,
					     proc_wg_kernel_dir);
  if (proc_drops_file) {
    set_proc_read(proc_drops_file,	     proc_read_drops);
  }

  proc_dma_errors_file = create_proc_entry(  "dma_errors",     0444,
					     proc_wg_kernel_dir);
  if (proc_dma_errors_file) {
    set_proc_read(proc_dma_errors_file,	     proc_read_dma_errors);
  }

  proc_fault_report_file = create_proc_entry("fault_report",   0666,
					     proc_wg_kernel_dir);
  if (proc_fault_report_file) {
    set_proc_read(proc_fault_report_file,    proc_read_fault_report);
    set_proc_write(proc_fault_report_file,   proc_write_fault_report);
  }

  proc_licensed_cpus_file = create_proc_entry("licensed_cpus", 0666,
					     proc_wg_kernel_dir);
  if (proc_licensed_cpus_file) {
    set_proc_read(proc_licensed_cpus_file,   proc_read_licensed_cpus);
    set_proc_write(proc_licensed_cpus_file,  proc_write_licensed_cpus);
  }

  proc_sw_type_file = create_proc_entry(     "sw_type",        0444,
					     proc_wg_kernel_dir);
  if (proc_sw_type_file) {
    set_proc_read(proc_sw_type_file,         proc_read_sw_type);
  }

#ifdef	CONFIG_WG_ARCH_FREESCALE // WG:JB Only Freesclae has T Series
  proc_t_series_file = create_proc_entry(    "t_series",       0444,
					     proc_wg_kernel_dir);
  if (proc_t_series_file) {
    set_proc_read(proc_t_series_file,        proc_read_t_series);
  }
#endif

#ifdef	CONFIG_CRASH_DUMP
  proc_log_file  = create_proc_entry(        "log",            0222,
					     proc_wg_kernel_dir);
  if (proc_log_file) {
    set_proc_write(proc_log_file,	     proc_write_log);
  }
#endif

#endif	// CONFIG_WG_PLATFORM_OLD_PROC_API // WG:JB Put old proc API back in

  // Return no errors
  return 0;
}
early_initcall(wg_kernel_init);

void __init wg_setup_arch(char* boot_command_line, int size)
{
#ifdef	CONFIG_X86

  // WG:JB fix up root dev as needed
  char*   root  = boot_command_line;
  char*   xtmv  = strstr(root, "wg_ptfm=xtmv");
  char*   cloud = strstr(root, "wg_cloud=");

  for (; (root = strstr(root, "root=/dev/")); root += 10)
  if   (root[11] == 'd')
  if  ((root[12] == 'a') || (root[12] == 'b')) {
    if (root[10] == 'h') root[10] = 's';
    if (xtmv)
     if (root[13] == '3') root[13] = '2';
  }

  if (xtmv) {
    // WG:JB Set a generic vm name until we find the specific one
    wg_vm_name = "xtmv";

    // WG:JB Force running /sbin/init
    strlcat(boot_command_line, " init=/sbin/init", size);
  }

  if (cloud) {
    // WG:JB Copy cloud environment (if any)
    strncpy(wg_cloud, cloud += 9, sizeof(wg_cloud) - 1);
    for (cloud = wg_cloud; *cloud > ' '; cloud++);
    *cloud = 0;
  }

#ifndef	CONFIG_CRASH_DUMP
  // WG:JB Make sure we have as much kdump memory as the platform needs
  {
    int cm = 36;

#ifdef	CONFIG_X86_64 // These Winthrops need more space.
    wg_kernel_get_x86_model(&boot_cpu_data);
    if (wg_winthrop >= 5) cm = 38;
#endif

    // WG:JB Add a crash kernel if none specified
    if (!strstr(boot_command_line, "crashkernel=")) {
      char crash_string[32];
      sprintf(crash_string,       " crashkernel=%dM",  cm);
      strlcat(boot_command_line,    crash_string,    size);
    }

    wg_crash_memory = (cm << 20);
  }

#if BITS_PER_LONG < 64
  // WG:JB Make sure we have as much VM as the platform needs
  {
    int vm = 768;

	// WG: BUG78806 - increase the vmalloc size for xtm-1050 from 512M to 640M
    if (strstr(boot_command_line, "wg_ptfm=xtm10" )) vm = 640; else
    if (strstr(boot_command_line, "wg_ptfm=xtm8"  )) vm = 512; else
    if (strstr(boot_command_line, "wg_ptfm=xtm5"  )) vm = 320;

    // force reserving the resource if it is filton.
    if (strstr(boot_command_line, "wg_ptfm=xtm20" )) {
      strlcat(boot_command_line, " reserve=0x800,0x7f", size);
    }

    if (__VMALLOC_RESERVE < (vm << 20))
	__VMALLOC_RESERVE = (vm << 20);
  }
#endif
#endif
#endif
}

#ifndef	CONFIG_CC_STACKPROTECTOR
void __stack_chk_fail(void)
{
	printk(KERN_EMERG "%s()\n", __FUNCTION__);
}
EXPORT_SYMBOL_GPL(__stack_chk_fail);
#endif

#ifdef	CONFIG_WG_PLATFORM_TRACE

int	wg_trace_dump(const char* func, int line, int flag, int code)
{
	if (console_loglevel < (flag & ~WG_DUMP)) return code;

	if (unlikely(code))
	printk(KERN_EMERG "%s:%-4d code %d\n", func, line, code);
	else
	printk(KERN_EMERG "%s:%-4d\n",         func, line);

	if (flag & WG_DUMP) {
		int lev = console_loglevel;
		console_loglevel =   9;
		dump_stack();
		console_loglevel = lev;
	}

	return code;
}
EXPORT_SYMBOL_GPL(wg_trace_dump);

void	wg_dump_hex(const u8* buf, u32 len, const u8* tag)
{
	if (console_loglevel < 32) len = (len < 256) ? len : 256;

	if (buf)
	if (len > 0)
	print_hex_dump(KERN_CONT, tag, DUMP_PREFIX_OFFSET, 16, 1, buf, len, 0);
}
EXPORT_SYMBOL_GPL(wg_dump_hex);

void	wg_dump_sgl(const char* func, int line, int flag,
		    struct scatterlist* sgl, const char* tag)
{
	int j;

	if (console_loglevel < (flag & ~WG_DUMP)) return;

	for (j = 0; sgl; sgl = sg_next(sgl), j++) {

	printk(KERN_EMERG "%s:%-4d %s%d %p len %4d virt %p\n", func, line,
	       tag, j, sgl, sgl->length, sg_virt(sgl));

	if (flag & WG_DUMP) {
		int lev = console_loglevel;
		console_loglevel =   9;
		wg_dump_hex(sg_virt(sgl), sgl->length + 8, "");
		console_loglevel = lev;
	}

	}
}
EXPORT_SYMBOL_GPL(wg_dump_sgl);

#endif

void   wg_sample_nop(unsigned long pc) {}
EXPORT_SYMBOL_GPL(wg_sample_nop);

void (*wg_sample_pc)(unsigned long) = wg_sample_nop;
EXPORT_SYMBOL_GPL(wg_sample_pc);

void memzero_explicit(void *s, size_t count)
{
	memset(s, 0, count);
	barrier();
}
EXPORT_SYMBOL(memzero_explicit);

#include <linux/pci.h>

int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
			  int minvec, int maxvec)
{
	int nvec = maxvec;
	int rc;

	if (maxvec < minvec)
		return -ERANGE;

	do {
		rc = pci_enable_msix(dev, entries, nvec);
		if (rc < 0) {
			return rc;
		} else if (rc > 0) {
			if (rc < minvec)
				return -ENOSPC;
			nvec = rc;
		}
	} while (rc);

	return nvec;
}
EXPORT_SYMBOL(pci_enable_msix_range);

module_init(wg_kernel_init);

MODULE_LICENSE("GPL");
