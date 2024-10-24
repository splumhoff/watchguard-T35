#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>

#define	WG_QUEUE_LIMIT		100000

#define	WG_QUEUE_NETPOLL	on
#define	WG_QUEUE_COUNTERS	on
//#define	WG_QUEUE_TEST	on

#include <linux/wg_queue.h>

#ifdef	CONFIG_WG_PLATFORM_QUEUE

#ifdef	CONFIG_PPC64
#define	raise_softirq_on_cpu(b,k)	set_bit(b+32,(void*)&((&per_cpu(irq_stat,k))->__softirq_pending))
#else
#define	raise_softirq_on_cpu(b,k)	set_bit(b,   (void*)&((&per_cpu(irq_stat,k))->__softirq_pending))
#endif

// Debug flags
static	int wg_queue_debug;

// Check skb function, returns non-zero on error
inline	int check_skb(const char* func, list_t* item, int cpu)
{
  int    cnt;
  int    err = wg_queue_debug;
  skb_t* skb = (struct sk_buff*)item;

  if (cpu < 0) cpu = raw_smp_processor_id();

  if (unlikely(!in_atomic())) {
    printk(KERN_EMERG "%s: skb %p not atomic\n", __FUNCTION__, skb);
    err = -1;
  }

  if (unlikely((cnt = atomic_read(&skb->users)) <= 1)) {
    printk(KERN_EMERG "%s: skb %p count %d\n",   __FUNCTION__, skb, cnt);
    err = -1;
  }

  if (unlikely(skb->next || skb->prev)) {
    printk(KERN_EMERG "%s: skb %p next %p prev %p\n",
           __FUNCTION__, skb, skb->next, skb->prev);
    err = -1;
  }

  if (unlikely(err & 1))
    printk(KERN_EMERG "%s: skb %p cpu %d %s#%d\n",
           func, skb, cpu, skb->dev->name, skb->dev->ifindex);

  if (unlikely(err & 2))
    dump_stack();

  return err & INT_MIN;
}

#define	CHECK_SKB(s,q)	0	// check_skb(__FUNCTION__,s,q)

//
// Add work to a queue
//
// If the queue isn't empty return the item back.
//

list_t* QueueClass__AddItem(QueueClass_t* pThis, unsigned q, list_t* item)
{
  unsigned cpu  = q;
  list_t*  swap = item;

  if (unlikely(q   >= pThis->QUEs)) q   %= pThis->QUEs;
  if (unlikely(cpu >= pThis->CPUs)) cpu %= pThis->CPUs;

  if (CHECK_SKB(item, cpu))
    return NULL;

  swap->next = NULL;

  // smp_mb();

  while (1)
  if (likely(atomic_inc_and_test(&pThis->QUE_Spin[q]))) {
    swap = (list_t*)atomicPTR_xchg(&pThis->QUE_Tail[q], (long)swap);
    swap->next = item;
    atomic_set(&pThis->QUE_Spin[q], -1);

    atomicCTR_inc(&pThis->QUE_Wr[q]);

    if (unlikely(!test_and_set_bit(cpu, &pThis->CPU_Flag))) {

#ifdef	WG_QUEUE_COUNTERS
      atomicCTR_inc(&pThis->CPU_Wake[cpu]);
#endif
      raise_softirq_on_cpu(NET_RX_SOFTIRQ, cpu);

      if (unlikely(000|000)) {
        struct task_struct *tsk = __this_cpu_read(ksoftirqd);
        if (tsk && tsk->state != TASK_RUNNING) {
          wake_up_process(tsk);
        }
      }
    }

    return item;

  } else
    atomicCTR_inc(&pThis->QUE_Wait[q]);

}
EXPORT_SYMBOL(QueueClass__AddItem);

//
// Service a work queue
//
// This is a place holder that does nothing, it should never actually
// be called and will generate a kernel error message.
//

list_t* QueueClass__Service(QueueClass_t* pThis, unsigned q, list_t* item)
{
  printk(KERN_EMERG "%s: No service function for %s[%d]\n",
         __FUNCTION__, pThis->Name, q);
  return NULL;
}
EXPORT_SYMBOL(QueueClass__Service);

//
// Process the work queues
//
// The given CPU iterates through its Queue list up to its CPU
// quota. Each Queue is then checked to make sure it is not being
// run by another CPU, if it is not busy then we atomically grab
// its work list and process it if not empty.
//

int QueueClass__Process(QueueClass_t* pThis, unsigned cpu, tick_t quota)
{
  int      k;
  int      q;
  que_t*   p;
  queset_t g;
  queset_t b;
  tick_t   limit;

  if (likely(!test_and_clear_bit(cpu, &pThis->CPU_Flag))) return -ENODATA;

  pThis->CPU_Start[cpu] = pThis->SET_Start[cpu] = CPU_CLOCK;

  g     = 0;
  b     = (QUE1 << 1);
  limit = quota;

  if (unlikely(pThis->SET_Count > 1)) {
    g = (QUE1 << pThis->SET_Count);
    limit /= pThis->SET_Count;
    if (likely(limit > pThis->SET_Quota))
      limit = pThis->SET_Quota;
  }

  for (k = 0, p = &pThis->CPU_QList[cpu][0]; (q = *p++) != QUE_END;) {
    if (likely(q >= 0))
    if (likely(atomic_inc_and_test(&pThis->QUE_Busy[q]))) {
      list_t* next = (list_t*)atomicPTR_read(&pThis->QUE_Work[q]);

      // Do we have an empty work list?
      if (unlikely(next == NULL)) {
        list_t* list;
        list_t* head;
        list_t* tail;

        list = head = tail = (list_t*)&pThis->QUE_Head[q];

        while (1)
        if (likely(atomic_inc_and_test(&pThis->QUE_Spin[q]))) {
          head = (list_t*)atomicPTR_xchg(&pThis->QUE_Head[q], (long)head);
          tail = (list_t*)atomicPTR_xchg(&pThis->QUE_Tail[q], (long)tail);
          atomic_set(&pThis->QUE_Spin[q], -1);

          if (unlikely(head != list)) {
            tail->next = NULL;
            next = head;
          }

          break;

        } else
          atomicCTR_inc(&pThis->QUE_Wait[q]);
      }

      // Process the work list
      if (likely(next)) {
#ifdef	WG_QUEUE_COUNTERS
        int z = atomicCTR_read(&pThis->QUE_Wr[q]) -
                atomicCTR_read(&pThis->QUE_Rd[q]);
        if (pThis->QUE_High[q] < z) pThis->QUE_High[q] = z;
#endif
        while (1) {
          list_t* item = next;
          next  = item->next;

          atomicCTR_inc(&pThis->QUE_Rd[q]); k++;
#ifdef	WG_QUEUE_COUNTERS
          atomicCTR_inc(&pThis->CPU_Matrix[cpu][q]);
#endif

          if (unlikely(!pThis->QUE_Service(pThis, q, item))) break;

          // Exit if no more work
          if (unlikely(!next)) break;

          // Check time exhausted
          if (likely(limit > 0)) {
            tick_t now = CPU_CLOCK;

            // If time is used up, break
            if (  unlikely((now -= pThis->SET_Start[cpu]) >= limit)) {
              if (unlikely((now -= pThis->CPU_Start[cpu]) >= quota)) {
                g = 0;
                p = &pThis->CPU_QList[cpu][pThis->QUEs-1];
                set_bit(cpu, &pThis->CPU_Flag);
#ifdef	WG_QUEUE_COUNTERS
                atomicCTR_inc(&pThis->CPU_Over[cpu]);
#endif
                raise_softirq_on_cpu(NET_RX_SOFTIRQ, cpu);
              }
              break;
            }
          }
        }

        // Flag that we processed items
        b |= 1;
      }

      // Update work list
      atomicPTR_set(&pThis->QUE_Work[q], (long)next);

      // Free this queue to run later
      atomic_set(&pThis->QUE_Busy[q], -1);
    }

    // Continue if we don't have sets
    if (likely(g <= 1)) continue;

    // Did we finish a set?
    if   (unlikely(b >= g)) {

      // Backup if we processed items in this set
      if (unlikely(b >  g)) {
        p -= pThis->SET_Count;
        b  = 2;
        continue;
      }

      // Move to next set
      pThis->SET_Start[cpu] = CPU_CLOCK;
      b = 2;
      continue;
    }

    // Move to next set member
    b <<= 1;
  }

  return k;
}
EXPORT_SYMBOL(QueueClass__Process);

int QueueClass__Statistics(QueueClass_t* pThis, char* page, int z)
{
  int  j;

  if (!pThis) return z;

  z += sprintf(&page[z], "\n%s\n\n", pThis->Name);

#ifdef	WG_QUEUE_COUNTERS
  for (j = 0; j < pThis->CPUs; j++) {
    long wq, ov;

    wq = 0;
    wq = atomicCTR_xchg(&pThis->CPU_Wake[j], wq);

    ov = 0;
    ov = atomicCTR_xchg(&pThis->CPU_Over[j], ov);

    if ((wq | ov) == 0) continue;

    z += sprintf(&page[z], "CPU%-2d Wake %8lu", j, wq);

    if (ov != 0)
      z += sprintf(&page[z], " Over %7lu", ov);

    z += sprintf(&page[z], "\n");
  }

  z += sprintf(&page[z], "\n");
#endif

  for (j = 0; j < pThis->QUEs; j++) {
    long wr, rd, op, wt, hi;

    wr = atomicCTR_read(&pThis->QUE_Wr[j]);
    rd = atomicCTR_read(&pThis->QUE_Rd[j]);

    if (wr == -1) wr = rd;
    if (rd == -1) continue;

    op = rd;
    op = atomicCTR_xchg(&pThis->QUE_Op[j],   op);

    wt = 0;
    wt = atomicCTR_xchg(&pThis->QUE_Wait[j], wt);

    op = rd - op;

    hi = 0;
    hi = xchg(&pThis->QUE_High[j], hi);

    z += sprintf(&page[z], "QUE%-2d OP %10lu WT %9lu WT/OP %4lu ",
                 j, op, wt, op ? wt / op : 0);
    z += sprintf(&page[z], "HI %5ld LEN %5ld = WR %lu - RD %lu\n",
                 hi, wr - rd, wr, rd);
  }

  z += sprintf(&page[z], "\n");

  return z;
}
EXPORT_SYMBOL(QueueClass__Statistics);

int QueueClass__ShowMatrix(QueueClass_t* pThis, char* page, int z)
{
#ifdef	WG_QUEUE_COUNTERS
  int   j;
  int   q;
  long  t;
  long  ct;
  long* qt;

  if (!pThis) return z;

  qt = kmalloc(q = pThis->QUEs * sizeof(long), GFP_KERNEL);
  if (!qt) return z; else memset(qt, 0, q);

  z += sprintf(&page[z], "\n%-5s", pThis->Name);
  for (q = 0; q < pThis->QUEs; q++)
  z += sprintf(&page[z], "%11d", q);
  z += sprintf(&page[z], "\n\n");

  for (j = 0; j < pThis->CPUs; j++) {

    z += sprintf(&page[z], "CPU%-2d", j);

    for (t = q = 0; q < pThis->QUEs; q++) {
      ct = 0;
      ct = atomicCTR_xchg(&pThis->CPU_Matrix[j][q], ct);
      t += ct; qt[q] += ct;

      if (ct)
        z += sprintf(&page[z], " %10lu", ct);
      else
        z += sprintf(&page[z], "           ");
    }

    z += sprintf(&page[z], " | %10lu\n", t);
  }

  z += sprintf(&page[z], "\nTotal");

  for (t = q = 0;   q < pThis->QUEs; q++) {
    t += qt[q];
    if (qt[q])
      z += sprintf(&page[z], " %10lu", qt[q]);
    else
      z += sprintf(&page[z], "           ");
  }

  z += sprintf(&page[z], " | %10lu\n\n", t);

  kfree(qt);
#endif

  return z;
}
EXPORT_SYMBOL(QueueClass__ShowMatrix);

int QueueClass__ShowQList(QueueClass_t* pThis, char* page, int z)
{
  int   j;
  int   q;
  char* tag;

  if (!pThis) return z;

  tag = kmalloc((4*pThis->QUEs)+1, GFP_KERNEL);
  if (!tag) return z;

  for (j = 0; j < pThis->CPUs; j++) {

    memset(tag, ' ', 4*pThis->QUEs); tag[4*pThis->QUEs] = 0;

    for (q = 0; q < pThis->QUEs; q++)
    if (pThis->CPU_QList[j][q] >= 0)
      sprintf(&tag[4*q], "%3d ", pThis->CPU_QList[j][q]);
    else
      sprintf(&tag[4*q], "%3d~", pThis->CPU_QList[j][q] & (QUE_OFF-1));

    if (page)
      z += sprintf(&page[z], "%s CPU%-2d  %s\n", pThis->Name, j, tag);

    else
      printk(KERN_DEBUG      "%s CPU%-2d  %s\n", pThis->Name, j, tag);
  }

  kfree(tag);
  return z;
}
EXPORT_SYMBOL(QueueClass__ShowQList);

int QueueClass__SetQList(QueueClass_t* pThis, int load)
{
  int  j;
  int  q;

  if (!pThis) return -ENOENT;

  if (unlikely((load < 0) || (load > pThis->QUEs))) return -EINVAL;

  pThis->QUE_Depth = load = (load > 0) ? load : pThis->QUEs;

  for (j = 0; j < pThis->CPUs; j++) {
    pThis->CPU_QLoad[j] = load;
    for (q = 0; q < pThis->QUEs; q++)
    if (q >= pThis->CPU_QLoad[j])
      pThis->CPU_QList[j][q] |=  QUE_OFF;
    else
      pThis->CPU_QList[j][q] &= (QUE_OFF-1);
  }

  return load;
}
EXPORT_SYMBOL(QueueClass__SetQList);

//
// Set up QueueClass including the queue matrix
//
// We create work queues that are independent of the # of CPUs
// Typically we have as many queues as we have CPUs, having more
// than that generally won't help except in special cases since
// you can only run as many queues as CPUs. You can create fewer
// queues than you have CPUs but this will leave some of the system
// idle from perspective of the queues work load. You might want to
// do this if you want to reserve one CPU for control procesing.
//
// Each CPU has a primary Queue which is the first item in its
// CPU_QList. You can allow that CPU to process other queues by
// adding them to that CPU_QList. If you add a second item, it will
// run only those 2. This is useful where you have bonded CPUs. For
// example you could have the work for Queue 0 generated by CPU 0
// and for Queue 1 by CPU 1, CPU 0 will run Queue 0 as its primary
// and Queue 1 as its secondary. Since the CPUs are bonded, and
// thus share an L2 cache, there will be minimal thrashing. The
// table below is created in this way with odd CPUs being flipped
// to even before advancing the pair.
//
// You can also implement a scheduler that looks at CPU load and
// adjusts CPU_QList accordingly, making them longer on the more
// idle CPUs, and shorter on the more busy ones. The extreme cases
// are a CPU can potentially process all Queues, or none. If you
// set the CPU_QList to be empty be sure another CPU has that CPU's
// primary Queue as a secondary.

QueueClass_t* QueueClass__Constructor(QueueClass_t* pThis, char* name,
                                      QueueClass__Service_FP service,
                                      QueueClass__Balance_FP balance,
                                      int load, int count,
                                      tick_t set_quota, tick_t cpu_quota)
{
  int j, q, n;

  if (count >= (sizeof(queset_t) * 8)) {
    printk(KERN_EMERG "%s: Set count %d too large for %s\n",
           __FUNCTION__, count, pThis->Name);
    return NULL;
  }

  n = num_online_cpus() * ((count > 0) ? count : 1);
  if (n > NR_QUEUES) {
    printk(KERN_EMERG "%s: Queue count %d too large for %s\n",
           __FUNCTION__, n, pThis->Name);
    return NULL;
  }

  if (!pThis) pThis = kmalloc(sizeof(QueueClass_t), GFP_KERNEL);
  if (!pThis) {
    printk(KERN_EMERG "%s: No memory for %s\n", __FUNCTION__, pThis->Name);
    return NULL;
  }

  memset(pThis, 0, sizeof(QueueClass_t));

  if (name)
  strncpy(pThis->Name, name, sizeof(pThis->Name)-1);

  pThis->CPUs = num_online_cpus();
  pThis->QUEs = n;

  memset(pThis->CPU_QList, QUE_END, sizeof(pThis->CPU_QList));

  for (       j = 0;   j < pThis->CPUs; j++)
  for (n = j, q = 0;   q < pThis->QUEs; q++, n++) {
    pThis->CPU_QList[j][q] = n % pThis->QUEs;
    if (j & 1) n += (n & 1) ? -2 : 2;
  }

  for (q = 0;   q < pThis->QUEs; q++) {
    atomicCTR_set( &pThis->QUE_Wr[q],   -1);
    atomicCTR_set( &pThis->QUE_Rd[q],   -1);
    atomic_set(    &pThis->QUE_Busy[q], -1);
    atomic_set(    &pThis->QUE_Spin[q], -1);
    atomicPTR_set( &pThis->QUE_Head[q], (long)&pThis->QUE_Head[q]);
    atomicPTR_set( &pThis->QUE_Tail[q], (long)&pThis->QUE_Head[q]);
    atomicPTR_set( &pThis->QUE_Work[q], (long)NULL);
  }

  pThis->SET_Count = count;
  pThis->SET_Quota = set_quota;

  pThis->CPU_Wait  = 0;
  pThis->CPU_Quota = cpu_quota;

  // Set up balance
  QueueClass__SetQList(pThis, load);
  if (balance) balance((unsigned long)pThis);
  QueueClass__ShowQList(pThis, NULL, 0);

  // Set up function vectors
  if (!service)
    pThis->QUE_Service = QueueClass__Service;
  else
    pThis->QUE_Service = service;

  pThis->QUE_Process   = QueueClass__Process;
  pThis->QUE_AddItem   = QueueClass__AddItem;

  return pThis;
}
EXPORT_SYMBOL(QueueClass__Constructor);

#ifdef	WG_QUEUE_NETPOLL

QueueClass_t*		pNetQ;
EXPORT_SYMBOL	       (pNetQ);

static	atomic_t	 NetQ_wake[NR_CPUS];
static	atomic_t	 NetQ_call[NR_CPUS];

static	void NetQ_trigger_softirq(void *data)
{
  int cpu = raw_smp_processor_id();
  raise_softirq_on_cpu(NET_RX_SOFTIRQ, cpu);
  atomic_set(&NetQ_call[cpu], -1);
}

#ifdef	WG_QUEUE_TEST

#include <linux/kthread.h>

#define	NTEST	4
#define	NITEM	(1 << 10)

QueueClass_t*	pTest;

typedef	struct	Test_S {
  list_t	list;
  long		seq;
} test_t;

test_t		TestQ[NITEM];

atomic_t	Test_Wr;
atomic_t	Test_Rd;

atomic_t	Threads;
int		Testing;

long		Expect[NR_QUEUES][NTEST];

extern	void	yield(void);

list_t* wg_Q_service(QueueClass_t* pThis, unsigned q, list_t* item)
{
  long seq = ((test_t*)item)->seq;
  int  j   = seq & (NTEST - 1);

  seq /= NTEST;

  if (seq != Expect[q][j])
    printk(KERN_DEBUG "Seq[%d][%d] %ld - %ld = %ld\n",
           q, j, seq, Expect[q][j], seq - Expect[q][j]);

  Expect[q][j] = ++seq;

  atomic_inc(&Test_Rd);

  yield();

  return item->next;
}

int wg_Q_insert(void* arg)
{
  long seq = 0;
  int  j   = (*((char*)arg)) & (NTEST - 1);

  atomic_inc(&Threads);

  while (1) {
    int wr = atomic_read(&Test_Wr);
    int rd = atomic_read(&Test_Rd);
    unsigned sz = wr - rd;

    if (sz < ((NITEM / 2) - NR_CPUS)) {
      int ix = atomic_inc_return(&Test_Wr) & (NITEM - 1);
      TestQ[ix].seq = (seq * NTEST) + j; seq++;
      pTest->QUE_AddItem(pTest, 0, &TestQ[ix].list);
    } else
    if (unlikely(sz >= (NITEM / 2)))
      printk(KERN_EMERG "%s: WR %d - RD %d = %d\n", __FUNCTION__, wr, rd, sz);

    if (Testing <= 0) break;

    yield();
  }

  atomic_dec(&Threads);

  return 0;
}

int wg_Q_remove(void* arg)
{
  atomic_inc(&Threads);

  while (1) {
    int wr = atomic_read(&Test_Wr);
    int rd = atomic_read(&Test_Rd);
    unsigned sz = wr - rd;

    if (sz > 0)
      pTest->QUE_Process(pTest, 0, 0);
    else
    if (Testing <= 0) break;

    yield();
  }

  atomic_dec(&Threads);

  return 0;
}

static	struct proc_dir_entry* proc_wg_queue_test_file;

static	int proc_write_wg_queue_test(struct file *file, const char *buffer,
                                     unsigned long count, void *data)
{
  char str[16];

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  if (str[0] == '1') {

    for (Testing = 0; atomic_read(&Threads) > 0; yield());

    memset(Expect, 0, sizeof(Expect));

    atomic_set(&Test_Wr, -1);
    atomic_set(&Test_Rd, -1);

    pTest = QueueClass__Constructor(pTest, "Test", wg_Q_service,
                                    NULL, 0, 0, 0, 0);
    if (!pTest) {
      return Testing = -ENOMEM;
    }

    Testing = 1;

    wake_up_process(kthread_create(wg_Q_insert, "0", "wg_Q_insert0"));
    wake_up_process(kthread_create(wg_Q_insert, "1", "wg_Q_insert1"));
    wake_up_process(kthread_create(wg_Q_remove,  0,  "wg_Q_remove"));

  } else {

    for (Testing = 0; atomic_read(&Threads) > 0; yield());

  }

  return count;
}

#endif	// WG_QUEUE_TEST

static	netset_t  NetQ_Set;
static	netset_t  NetQ_All;

static	struct proc_dir_entry* proc_wg_queue_map_file;

// Get NetQ_Set
static int proc_read_wg_queue_map(char *page, char **start, off_t off,
                                  int count, int *eof, void *data)
{
  return sprintf(page, "%llX %llX\n", NetQ_Set, NetQ_All);
}

// Set NetQ_Set
static	int proc_write_wg_queue_map(struct file* file,   const char* buffer,
                                    unsigned long count, void* data)
{
  char   str[256];
  u64    value = 0;
  int    len = (count > (sizeof(str)-1)) ? sizeof(str)-1 : count;

  // Get string from user and null terminate it
  if (copy_from_user(str, buffer, len))
    return -EFAULT;
  str[len] = 0;

  // Set interface map
  if (sscanf(str, "%llx", &value) == 1)
  if (value &= NetQ_All) NetQ_Set = value;

  return count;
}

static	struct proc_dir_entry* proc_wg_queue_qlist_file;

// Get CPU qlist level
static int proc_read_wg_queue_qlist(char *page, char **start, off_t off,
                                    int count, int *eof, void *data)
{
  return QueueClass__ShowQList(pNetQ, page, 0);
}

// Set CPU qlist level
static int proc_write_wg_queue_qlist(struct file *file, const char *buffer,
                                     unsigned long count, void *data)
{
  int  load;
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  load = simple_strtoul(str, &strend, 10);

  if (unlikely(QueueClass__SetQList(pNetQ, load) < 0)) return -EINVAL;

  return count;
}

static	struct proc_dir_entry* proc_wg_queue_debug_file;

// Get debug flag
static int proc_read_wg_queue_debug(char *page, char **start, off_t off,
                                    int count, int *eof, void *data)
{
  return sprintf(page, "%X\n", wg_queue_debug);
}

// Set debug flag
static int proc_write_wg_queue_debug(struct file *file, const char *buffer,
                                     unsigned long count, void *data)
{
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  wg_queue_debug = simple_strtoul(str, &strend, 16);

  return count;
}

static	struct proc_dir_entry* proc_wg_queue_quota_file;

// Get quota
static int proc_read_wg_queue_quota(char *page, char **start, off_t off,
                                    int count, int *eof, void *data)
{
  if (pNetQ) return sprintf(page, "%ld\n", pNetQ->CPU_Quota);

  return 0;
}

// Set quota
static int proc_write_wg_queue_quota(struct file *file, const char *buffer,
                                     unsigned long count, void *data)
{
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  if (pNetQ) pNetQ->CPU_Quota = simple_strtoul(str, &strend, 10);

  return count;
}

static	struct proc_dir_entry* proc_wg_queue_wait_file;

// Get wait flag
static int proc_read_wg_queue_wait(char *page, char **start, off_t off,
                                   int count, int *eof, void *data)
{
  if (pNetQ) return sprintf(page, "%d\n", pNetQ->CPU_Wait);

  return 0;
}

// Set wait flag
static int proc_write_wg_queue_wait(struct file *file, const char *buffer,
                                    unsigned long count, void *data)
{
  char str[256], *strend;

  if (count > (sizeof(str)-1)) count = sizeof(str)-1;
  if(copy_from_user(str, buffer, count))
    return -EFAULT;

  str[count] = '\0';

  if (pNetQ) pNetQ->CPU_Wait = simple_strtol(str, &strend, 10);

  return count;
}

static	struct proc_dir_entry* proc_wg_queue_stats_file;

static	int proc_read_wg_queue_stats(char *page, char **start, off_t off,
                                     int count, int *eof, void *data)
{
  int z = 0;

  z = QueueClass__Statistics(pNetQ, page, z);
#ifdef	WG_QUEUE_TEST
  z = QueueClass__Statistics(pTest, page, z);
#endif

  return z;
}

static	struct proc_dir_entry* proc_wg_queue_matrix_file;

static	int proc_read_wg_queue_matrix(char *page, char **start, off_t off,
                                      int count, int *eof, void *data)
{
  return QueueClass__ShowMatrix(pNetQ, page, 0);
}

static	struct proc_dir_entry* proc_wg_queue_dir;

static	int NetQ_AddItem(struct sk_buff* skb, int q)
{
  int wg_queue_smp_wake(int, smp_call_func_t, int);

  if (likely(NetQ_Set & (NET1 << skb->dev->ifindex))) {
    skb_get(skb);
#if WG_QUEUE_LIMIT > 1
    if (likely((atomicCTR_read(&pNetQ->QUE_Wr[q]) -
                atomicCTR_read(&pNetQ->QUE_Rd[q])) < WG_QUEUE_LIMIT))
#endif
    if     (likely(pNetQ->QUE_AddItem(pNetQ, q, (list_t*)skb))) {
      if (unlikely(atomic_inc_and_test(&NetQ_wake[q])))
      if (unlikely(atomic_inc_and_test(&NetQ_call[q])))
      if (unlikely(wg_queue_smp_wake(q, &NetQ_trigger_softirq, pNetQ->CPU_Wait) < 0)) {
          raise_softirq_on_cpu(NET_RX_SOFTIRQ, q);
          atomic_set(&NetQ_call[q], -1);
      }
      return NET_RX_SUCCESS;
    }

	atomic_dec(&skb->users);  // FBX-14261, dec the skb->users since we just called skb_get(skb)
    kfree_skb(skb);
    return NET_RX_DROP;
  }

  return -ENODEV;
}

static	list_t* NetQ_Service(QueueClass_t* x, unsigned q, list_t* item)
{
  int k = atomic_read(&((skb_t*)item)->users);
  extern int wg_netif_receive_skb(skb_t*);

  item->next = NULL;

  if (likely(k > 1)) {
    kfree_skb((skb_t*)item);

    if (likely(!CHECK_SKB(item, -1))) {
      wg_netif_receive_skb((skb_t*)item);
      return item;
    }
  }

  kfree_skb((skb_t*)item);
  return item;
}

static	void	NetQ_Balance(unsigned long arg)
{
  QueueClass_t* pThis = (QueueClass_t*)arg;
  unsigned long expire = jiffies + (600*HZ);

  if (likely(pThis->QUE_Timer.function)) {
    QueueClass__SetQList(pThis, pThis->QUE_Depth);
    mod_timer(&pThis->QUE_Timer, expire);
  } else {
    init_timer(&pThis->QUE_Timer);
    pThis->QUE_Timer.expires  = expire;
    pThis->QUE_Timer.data     = (unsigned long)pThis;
    pThis->QUE_Timer.function = NetQ_Balance;
    add_timer(&pThis->QUE_Timer);
  }
}

static	int	NetQ_Process(void)
{
  int k;
  int cpu = raw_smp_processor_id();

  k = pNetQ->QUE_Process(pNetQ, cpu, pNetQ->CPU_Quota);
  if (likely(k)) raise_softirq_on_cpu(NET_TX_SOFTIRQ, cpu);

  atomic_set(&NetQ_wake[cpu], -1);

  return k;
}

// Add wg_NetQ externs
extern	int	(*wg_NetQ_poll)(void);
extern	int	(*wg_NetQ_add)(struct sk_buff*, int);

int  __init wg_queue_init(void)
{
  int      j = 1;
  netset_t b = (NET1 << j);
  netset_t a = 0, x= 0;
  struct   net_device* netdev;

  // Check size of softirq_pending
  if (sizeof(((irq_cpustat_t*)0)->__softirq_pending) != sizeof(int)) {
    printk(KERN_EMERG "%s: sizeof(softirq_pending) = %d\n",
           __FUNCTION__, (int)sizeof(((irq_cpustat_t*)0)->__softirq_pending));
    return -EINVAL;
  }

#ifdef	CONFIG_64BIT
  // Check size of long
  if (sizeof(long)      != 8) {
    printk(KERN_EMERG "%s: sizeof(long) = %d\n",
           __FUNCTION__, (int)sizeof(long));
    return -EINVAL;
  }
#endif

  // Check size of long long
  if (sizeof(long long) != 8) {
    printk(KERN_EMERG "%s: sizeof(long long) = %d\n",
           __FUNCTION__, (int)sizeof(long long));
    return -EINVAL;
  }

  // Create the polling interface map
  for (; (netdev = dev_get_by_index(&init_net, j)); j++, b += b) {
    if (b == 0) {
      printk(KERN_EMERG "%s: Too many interfaces %d\n", __FUNCTION__, j);
      return -EINVAL;
    }

    if (strncmp(netdev->name, "sw",    2) == 0) a |= b;
    if (strncmp(netdev->name, "eth",   3) == 0) a |= b;
#ifdef	CONFIG_X86
    if (strncmp(netdev->name, "eth0",  4) == 0) x |= b;
    if (strncmp(netdev->name, "eth25", 5) == 0) x |= b;
    if (strncmp(netdev->name, "eth26", 5) == 0) x |= b;
#endif
#ifdef	CONFIG_PPC
    if (strncmp(netdev->name, "sw",    2) == 0) x |= b;
#ifdef	CONFIG_PPC64
    if (strncmp(netdev->name, "eth0",  4) == 0) x |= b;
    if (strncmp(netdev->name, "eth1",  4) == 0) x |= b;
    if (strncmp(netdev->name, "eth2",  4) == 0) x |= b;
#endif
#endif

    NetQ_Set = (NetQ_All = a) & ~x;
  }

  // Print herald
  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ " NetQ Set %llX\n\n",
         __FUNCTION__, NetQ_Set);

  // Create /proc/wg_queue
  proc_wg_queue_dir = proc_mkdir("wg_queue", NULL);
  if (!proc_wg_queue_dir) return -EPERM;

  // Create /proc/wg_queue/map to set NetQ interface map
  proc_wg_queue_map_file    = create_proc_entry("map",
                                                0666, proc_wg_queue_dir);
  if (proc_wg_queue_map_file) {
    proc_wg_queue_map_file->read_proc    = proc_read_wg_queue_map;
    proc_wg_queue_map_file->write_proc   = proc_write_wg_queue_map;
  }

  // Create /proc/wg_queue/qlist  to set CPU qlist level
  proc_wg_queue_qlist_file  = create_proc_entry("qlist",
                                                0666, proc_wg_queue_dir);
  if (proc_wg_queue_qlist_file) {
    proc_wg_queue_qlist_file->read_proc  = proc_read_wg_queue_qlist;
    proc_wg_queue_qlist_file->write_proc = proc_write_wg_queue_qlist;
  }

  // Create /proc/wg_queue/stats
  proc_wg_queue_stats_file  = create_proc_entry("stats",
                                                0444, proc_wg_queue_dir);
  if (proc_wg_queue_stats_file) {
    proc_wg_queue_stats_file->read_proc  = proc_read_wg_queue_stats;
  }

  // Create /proc/wg_queue/debug to enable/disable debug flags
  proc_wg_queue_debug_file  = create_proc_entry("debug",
                                                0666, proc_wg_queue_dir);
  if (proc_wg_queue_debug_file) {
    proc_wg_queue_debug_file->read_proc  = proc_read_wg_queue_debug;
    proc_wg_queue_debug_file->write_proc = proc_write_wg_queue_debug;
  }

  // Create /proc/wg_queue/quota to set/get quota for processing
  proc_wg_queue_quota_file  = create_proc_entry("quota",
                                                0666, proc_wg_queue_dir);
  if (proc_wg_queue_quota_file) {
    proc_wg_queue_quota_file->read_proc  = proc_read_wg_queue_quota;
    proc_wg_queue_quota_file->write_proc = proc_write_wg_queue_quota;
  }

  // Create /proc/wg_queue/wait  to set/get wait  for processing
  proc_wg_queue_wait_file   = create_proc_entry("wait",
                                                0666, proc_wg_queue_dir);
  if (proc_wg_queue_wait_file) {
    proc_wg_queue_wait_file->read_proc   = proc_read_wg_queue_wait;
    proc_wg_queue_wait_file->write_proc  = proc_write_wg_queue_wait;
  }

  // Create /proc/wg_queue/matrix
  proc_wg_queue_matrix_file = create_proc_entry("matrix",
                                                0444, proc_wg_queue_dir);
  if (proc_wg_queue_matrix_file) {
    proc_wg_queue_matrix_file->read_proc = proc_read_wg_queue_matrix;
  }

#ifdef	WG_QUEUE_TEST
  // Create /proc/wg_queue/test
  proc_wg_queue_test_file   = create_proc_entry("test",
                                                0200, proc_wg_queue_dir);
  if (proc_wg_queue_test_file) {
    proc_wg_queue_test_file->write_proc  = proc_write_wg_queue_test;
  }
#endif	// WG_QUEUE_TEST

  pNetQ = QueueClass__Constructor(pNetQ, "NetQ", NetQ_Service,
                                  NetQ_Balance, 0, 0, 0, CPU_QUOTA);
  if (pNetQ) {
    wg_NetQ_add  = NetQ_AddItem;
    wg_NetQ_poll = NetQ_Process;
  }

  for (j = 0; j < pNetQ->CPUs; j++) atomic_set(&NetQ_wake[j], -1);
  for (j = 0; j < pNetQ->CPUs; j++) atomic_set(&NetQ_call[j], -1);

  return 0;
}
module_init(wg_queue_init);

void __exit wg_queue_exit(void)
{
#ifdef	WG_QUEUE_TEST
  // Stop any tests
  for (Testing = 0; atomic_read(&Threads) > 0; yield());

  // Free test
  if (pTest) kfree(pTest);

  if (proc_wg_queue_test_file)
    remove_proc_entry("test",     proc_wg_queue_dir);
#endif	// WG_QUEUE_TEST

  // Remove timer if any
  if (pNetQ->QUE_Timer.function)
    del_timer(&pNetQ->QUE_Timer);

  // Shutdown NetQ polling
  wg_NetQ_add  = NULL;
  wg_NetQ_poll = NULL;

  if (pNetQ) {
    int j;

    for (j = 0; j < pNetQ->CPUs; j++)
    while (pNetQ->QUE_Process(pNetQ, j, 0) > 0);

    kfree(pNetQ);
  }

  // Remove /proc/wg_queue entries
  if (proc_wg_queue_map_file)
    remove_proc_entry("map",      proc_wg_queue_dir);

  if (proc_wg_queue_qlist_file)
    remove_proc_entry("qlist",    proc_wg_queue_dir);

  if (proc_wg_queue_stats_file)
    remove_proc_entry("stats",    proc_wg_queue_dir);

  if (proc_wg_queue_debug_file)
    remove_proc_entry("debug",    proc_wg_queue_dir);

  if (proc_wg_queue_quota_file)
    remove_proc_entry("quota",    proc_wg_queue_dir);

  if (proc_wg_queue_wait_file)
    remove_proc_entry("wait",     proc_wg_queue_dir);

  if (proc_wg_queue_matrix_file)
    remove_proc_entry("matrix",   proc_wg_queue_dir);

#ifdef	WG_QUEUE_TEST
  if (proc_wg_queue_test_file)
    remove_proc_entry("test",     proc_wg_queue_dir);
#endif

  if (proc_wg_queue_dir)
    remove_proc_entry("wg_queue", NULL);
}
module_exit(wg_queue_exit);

#endif	// WG_QUEUE_NETPOLL

MODULE_LICENSE("GPL");

#endif	// CONFIG_WG_PLATFORM_QUEUE
