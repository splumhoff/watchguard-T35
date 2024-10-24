#ifndef	_LINUX_WG_FIFO_H
#define	_LINUX_WG_FIFO_H

// The items below must match kmap.c in wg_linux

// Base of User and Kern mappings of Kernel address space

#define	USER_BASE	0x80000000
#define	KERN_BASE	0xC0000000
      
// Major device numbers for FIFO devices

#define	FIFO_RX_MAJOR	222
#define	FIFO_TX_MAJOR	223

// ITEM has only a head

struct wg_item {
  struct wg_item* head;
};

// FIFO has a head and a tail

struct wg_fifo {
  struct wg_item* head;
  struct wg_item* tail;
  struct wg_item* gate;
  unsigned        spin;
  atomic_t        lock;
};

// FIFO set has a work list and a free list

struct wg_fifo_set {
  struct wg_fifo  Work;
  struct wg_fifo  Free;
};

#ifndef	UNIT_TEST
int   wg_fifo_create(struct cdev* cdev, const struct file_operations* fops,
                     int major, int minor,
                     struct wg_fifo_set* fifo_set, int count);
int   wg_fifo_delete(struct cdev* cdev);
#endif

void  wg_fifo_setup(struct wg_fifo* fifo, int insert_flag);
int   wg_fifo_insert(struct wg_fifo* fifo, struct wg_item* item);
int   wg_fifo_insert_list(struct wg_fifo* fifo,
                          struct wg_item* head,
                          struct wg_item* tail);
void* wg_fifo_remove(struct wg_fifo* fifo);

extern int wg_vma_user_kern;

// Grab a FIFO lock, count up spinning time

static inline void wg_fifo_grab_lock(struct wg_fifo* fifo)
{
  while (!atomic_inc_and_test(&fifo->lock)) fifo->spin++;
}

// Free a FIFO lock

static inline void wg_fifo_free_lock(struct wg_fifo* fifo)
{
  atomic_set(&fifo->lock, -1);
}

#endif	// _LINUX_WG_FIFO_H
