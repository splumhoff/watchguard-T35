#ifndef	_LINUX_WG_QUEUE_H
#define	_LINUX_WG_QUEUE_H

#ifdef	CONFIG_X86_64
#ifndef	CONFIG_CRASH_DUMP
#define	CONFIG_WG_PLATFORM_QUEUE	1
#endif
#endif

#ifdef	CONFIG_WG_PLATFORM_QUEUE

#include <linux/slab.h>

#ifdef	CONFIG_WG_ARCH_X86
#define	CPU_CLOCK	__native_read_tsc()
#define	CPU_QUOTA	2000000
#endif

#ifdef	CONFIG_WG_ARCH_FREESCALE
#ifdef	CONFIG_PPC64
#define	CPU_CLOCK	mftb()
#define	CPU_QUOTA	1000000
#else
#define	CPU_CLOCK	mftbl()
#define	CPU_QUOTA	1000000
#endif
#endif

#ifdef	CONFIG_64BIT

#define	atomicPTR_t		atomic64_t
#define	atomicPTR_set		atomic64_set
#define	atomicPTR_read		atomic64_read
#define	atomicPTR_xchg		atomic64_xchg

#define	atomicCTR_t		atomic64_t
#define	atomicCTR_set		atomic64_set
#define	atomicCTR_read		atomic64_read
#define	atomicCTR_inc		atomic64_inc
#define	atomicCTR_inc_return	atomic64_inc_return
#define	atomicCTR_xchg		atomic64_xchg

#else

#define	atomicPTR_t		atomic_t
#define	atomicPTR_set		atomic_set
#define	atomicPTR_read		atomic_read
#define	atomicPTR_xchg		atomic_xchg

#define	atomicCTR_t		atomic_t
#define	atomicCTR_set		atomic_set
#define	atomicCTR_read		atomic_read
#define	atomicCTR_inc		atomic_inc
#define	atomicCTR_inc_return	atomic_inc_return
#define	atomicCTR_xchg		atomic_xchg

#endif

#define	NR_QUEUES		(NR_CPUS)

typedef	int8_t			que_t;

#define	QUE_OFF			(1<<((8*sizeof(que_t))-1))
#define	QUE_END			(-1)

typedef	unsigned long long	queset_t;
typedef	unsigned long long	netset_t;
typedef	unsigned long		cpuset_t;

#define	QUE1			((queset_t)1)
#define	NET1			((netset_t)1)
#define	CPU1			((cpuset_t)1)

typedef	unsigned long		tick_t;

typedef	struct	sk_buff		skb_t;
typedef	struct	list_head	list_t;

typedef	struct	QueueClass_S {

  char			Name[16];

  unsigned		CPUs;
  unsigned		QUEs;

  volatile cpuset_t	CPU_Flag;

  int			CPU_Wait;
  tick_t		CPU_Quota;
  tick_t		CPU_Start[NR_CPUS];

  que_t			CPU_QLoad[NR_CPUS];
  que_t			CPU_QList[NR_CPUS][NR_QUEUES+1];

  atomicCTR_t		QUE_Wait[NR_QUEUES];

  atomicCTR_t		QUE_Op[NR_QUEUES];
  atomicCTR_t		QUE_Wr[NR_QUEUES];
  atomicCTR_t		QUE_Rd[NR_QUEUES];

  atomic_t		QUE_Busy[NR_QUEUES];
  atomic_t		QUE_Spin[NR_QUEUES];

  atomicPTR_t		QUE_Head[NR_QUEUES];
  atomicPTR_t		QUE_Tail[NR_QUEUES];
  atomicPTR_t		QUE_Work[NR_QUEUES];

  struct timer_list	QUE_Timer;
  int			QUE_Depth;

  int			SET_Count;
  tick_t		SET_Quota;
  tick_t		SET_Start[NR_CPUS];

  int			(*QUE_Process)(struct QueueClass_S*, unsigned, tick_t);
  list_t*		(*QUE_AddItem)(struct QueueClass_S*, unsigned, list_t*);
  list_t*		(*QUE_Service)(struct QueueClass_S*, unsigned, list_t*);

#ifdef	WG_QUEUE_COUNTERS
  atomicCTR_t		CPU_Matrix[NR_CPUS][NR_QUEUES];

  atomicCTR_t		CPU_Wake[NR_CPUS];
  atomicCTR_t		CPU_Over[NR_CPUS];

  int			QUE_High[NR_QUEUES];
#endif

} QueueClass_t;

typedef
int	(*QueueClass__Process_FP)(QueueClass_t*, unsigned, tick_t);
int	  QueueClass__Process    (QueueClass_t*, unsigned, tick_t);

typedef
list_t*	(*QueueClass__AddItem_FP)(QueueClass_t*, unsigned, list_t*);
list_t*	  QueueClass__AddItem    (QueueClass_t*, unsigned, list_t*);

typedef
list_t*	(*QueueClass__Service_FP)(QueueClass_t*, unsigned, list_t*);
list_t*	  QueueClass__Service    (QueueClass_t*, unsigned, list_t*);

typedef
void	(*QueueClass__Balance_FP)(unsigned long);
void	  QueueClass__Balance    (unsigned long);

typedef
QueueClass_t* (*QueueClass__Constructor_FP)(QueueClass_t*, char*,
                                            QueueClass__Service_FP,
                                            QueueClass__Balance_FP,
                                            int, int, tick_t, tick_t);
QueueClass_t*   QueueClass__Constructor    (QueueClass_t*, char*,
                                            QueueClass__Service_FP,
                                            QueueClass__Balance_FP,
                                            int, int, tick_t, tick_t);

#endif	// CONFIG_WG_PLATFORM_QUEUE

#endif	// _LINUX_WG_QUEUE_H
