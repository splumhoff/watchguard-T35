#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>
#include <linux/profile.h>

#ifdef	CONFIG_WG_PLATFORM
#ifdef	CONFIG_PROFILING

#ifdef	CONFIG_DEBUG_SPINLOCK
#define	DO_RAS	1
#endif

#if	NR_CPUS > 1
static	int CPUs = NR_CPUS;
#define	CPU_Id	(((unsigned)raw_smp_processor_id()))
#else
#define	CPUs	1
#define	CPU_Id	0
#endif

#define	PCS_BITS 10
#define	PCS_SIZE (1<<PCS_BITS)
#define	PCS_MASK (PCS_SIZE-1)

static	unsigned flip, flop, samples, ticker[2*NR_CPUS];

static	unsigned int   pre[2*NR_CPUS][PCS_SIZE];
static	unsigned long  pcs[2*NR_CPUS][PCS_SIZE];
static	unsigned short pid[2*NR_CPUS][PCS_SIZE];
#ifdef	DO_RAS
static	unsigned long  ras[2*NR_CPUS][PCS_SIZE];
#endif

void SamplePC(unsigned long pc)
{
  unsigned cpu = CPU_Id;

  if (likely(cpu < NR_CPUS))
  if (likely(pc)) {
    unsigned q   = cpu + flip;
    unsigned j   = (ticker[q]++ & PCS_MASK);

    samples++;

    pre[q][j]    = preempt_count();
    pcs[q][j]    = pc;
    pid[q][j]    = current->pid;
#ifdef	DO_RAS
    {
	extern unsigned long LockRA[NR_CPUS];
	ras[q][j] =          LockRA[cpu];
	//LockRA[cpu]  = 0;
    }
#endif
  }
}
EXPORT_SYMBOL(SamplePC);

int Hw_SampleHook(struct pt_regs *regs)
{
  SamplePC(profile_pc(regs));
  return 0;
}
EXPORT_SYMBOL(Hw_SampleHook);

static unsigned pcs_pos;

static struct proc_dir_entry *wg_sample_dir;
static struct proc_dir_entry *pcs_file;
static struct proc_dir_entry *enable_file;

static void seq_printf_symbol(struct seq_file* m,
                              unsigned long cpu,
                              unsigned long pc,
                              unsigned long ra,
                              unsigned long px,
                              unsigned long pd)
{
  int   n;
  char* modname;
  char  txt[256];
  char  namebuf[KSYM_NAME_LEN+1];
  const char* name;
  unsigned long offset, size;

  if (pc == 0) return;

  memset(txt, ' ', sizeof(txt));

  n = sprintf(&txt[0], "%2lu ", cpu);

  if (pc) {
    if (!(name = kallsyms_lookup(pc, &size, &offset, &modname, namebuf)))
      n += sprintf(&txt[n], "0x%lx", pc);
    else
    if (modname)
      n += sprintf(&txt[n], "%s+%#lx/%#lx [%s]", name, offset, size, modname);
    else
      n += sprintf(&txt[n], "%s+%#lx/%#lx",      name, offset, size);

    txt[n] = ' ';
    if (n < 64) n = 64;
    if (px)
      n += sprintf(&txt[n], "%8lx", px);
    else
      n += 8;
    if (pd)
      n += sprintf(&txt[n], "%6ld", pd);
    else
      n += 6;
    txt[n] =  0;
    seq_printf(m, "%s", txt);
  }

  //if (in_lock_functions(pc))
  if (ra) {
    seq_printf(m, "  @");
    if (!(name = kallsyms_lookup(ra, &size, &offset, &modname, namebuf)))
      seq_printf(m, "0x%lx", ra);
    else
    if (modname)
      seq_printf(m, "%s+%#lx/%#lx [%s]", name, offset, size, modname);
    else
      seq_printf(m, "%s+%#lx/%#lx",      name, offset, size);
  }

  seq_printf(m, "\n");
}

static int s_show(struct seq_file *m, void *v)
{
  unsigned long cpu, q, j;

  if ((cpu = (pcs_pos >> PCS_BITS)) < CPUs) {
    j = ((ticker[q = (cpu + flop)] + pcs_pos) & PCS_MASK);
#ifdef	DO_RAS
    seq_printf_symbol(m, cpu, pcs[q][j], ras[q][j], pre[q][j], pid[q][j]);
#else
    seq_printf_symbol(m, cpu, pcs[q][j],         0, pre[q][j], pid[q][j]);
#endif
  }

  return 0;
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
  (*pos)++;
  pcs_pos++;

  if (pcs_pos >= NR_CPUS*PCS_SIZE) return NULL;
  return p;
}

static void *s_start(struct seq_file *m, loff_t *pos)
{
  if (pcs_pos >= NR_CPUS*PCS_SIZE) return NULL;
  return &pcs_pos;
}

static void s_stop(struct seq_file *m, void *p)
{
}

static const struct seq_operations pcs_op = {
	.start = s_start,
	.next  = s_next,
	.stop  = s_stop,
	.show  = s_show
};

static int wg_sample_proc_pcs_open(struct inode *inode, struct file *file)
{
  if (samples == 0) {
    pcs_pos = NR_CPUS*PCS_SIZE;
    return -ENOENT;
  }

  pcs_pos = samples = 0;

  flop = flip;
  flip = flip ? 0 : NR_CPUS;

  return seq_open(file, &pcs_op);
}

static int wg_sample_proc_pcs_release(struct inode *inode, struct file *file)
{
  pcs_pos = NR_CPUS*PCS_SIZE;
  return seq_release(inode, file);
}

static struct file_operations wg_sample_proc_pcs_operations = {
  .open    = wg_sample_proc_pcs_open,
  .read    = seq_read,
  .llseek  = seq_lseek,
  .release = wg_sample_proc_pcs_release,
};

static char enable[4];

static int proc_write_enable(struct file *file, const char *buffer,
                             unsigned long count, void *data)
{
  char str[4];

  if (copy_from_user(str, buffer, 1))
    return -EFAULT;

  wg_sample_pc = wg_sample_nop;

  switch (str[0]) {

  case '1': case 'T': case 't': case 'Y': case 'y':
    if (enable[0] <= '0') {
      int rc = register_timer_hook(Hw_SampleHook);
      printk(KERN_INFO "wg_sample: register_timer_hook returned %d\n", rc);
      if (rc < 0)
        return rc;
    }

    enable[0] = str[0];

    break;

  default:
    if (enable[0] > '0')
      unregister_timer_hook(Hw_SampleHook);

    enable[0] = '0';

    if (str[0] == '$') {
      enable[0] = '$';
      wg_sample_pc = SamplePC;
    }
  }

  return count;
}

static int proc_read_enable(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
  return sprintf(page, "%s\n", enable);
}

static int  __init wg_sample_init(void)
{
  printk(KERN_EMERG "\n%s: Built " __DATE__ " " __TIME__ " CPUs %d\n\n",
         __FUNCTION__, CPUs);

  wg_sample_dir = proc_mkdir("wg_sample", NULL);
  if (!wg_sample_dir)
    return -ENOMEM;

  enable_file = create_proc_entry("enable", 0644, wg_sample_dir);
  if (!enable_file)
    return -ENOMEM;

  enable_file->read_proc  = proc_read_enable;
  enable_file->write_proc = proc_write_enable;

  pcs_file = create_proc_entry("pcs", 0444, wg_sample_dir);
  if (!pcs_file)
    return -ENOMEM;

  pcs_file->proc_fops = &wg_sample_proc_pcs_operations;

  return 0;
}

module_init(wg_sample_init);
MODULE_LICENSE("GPL");

#endif	// CONFIG_PROFILING
#endif	// CONFIG_WG_PLATFORM
