#ifdef	UNIT_TEST


// User mode build


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#define	TEST_MODE 1

#define	EXPORT_SYMBOL(a)

#define	likely(a)		(a)
#define	unlikely(a)		(a)

#define	atomic_t		int
#define	atomic_set(a,b)		((*a)=(b))
#define	atomic_inc_and_test(a)  ((++(*a))==0)

#include "../include/linux/wg_fifo.h"

#else	// UNIT_TEST


// Kernel build


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/wg_fifo.h>
#include <asm/uaccess.h>

#define	TEST_MODE 1


#endif	// UNIT_TEST


// This is the actual FIFO code

// Actual user/Kern VM fix up amount

int	wg_vma_user_kern = (KERN_BASE-USER_BASE);
#define	wg_vma_kern_user   (-wg_vma_user_kern)
EXPORT_SYMBOL(wg_vma_user_kern);

// Setup a FIFO, can be read only, or can allows inserts

void  wg_fifo_setup(struct wg_fifo* fifo, int insert_flag)
{
  // FIFO is empty
  fifo->head = NULL;

  // If a FIFO's tail is NULL inserts are not allowed
  fifo->gate = insert_flag ? (struct wg_item*)fifo : NULL;

  // When adding to empty FIFO non-NULL tail pointing
  // to the FIFO itself will set FIFO head
  fifo->tail = fifo->gate;

  // FIFO has no spin time
  fifo->spin = 0;

  // Free the FIFO lock
  wg_fifo_free_lock(fifo);
}
EXPORT_SYMBOL(wg_fifo_setup);

// Insert item into FIFO

int wg_fifo_insert(struct wg_fifo* fifo, struct wg_item* item)
{
  int ret = -ENOMEM;

  // Item must be non-NULL to insert
  if (unlikely(!item)) return ret;

  wg_fifo_grab_lock(fifo);

  // Tail must be non-NULL to insert
  if (likely(fifo->tail)) {

    // Old  tail now points to new item
    fifo->tail->head = item;

    // FIFO tail now points to new item
    fifo->tail = item;

    // Return insert completed
    ret = 0;
  };

  wg_fifo_free_lock(fifo);

  // Return status
  return ret;
}
EXPORT_SYMBOL(wg_fifo_insert);

// Insert item into FIFO

int wg_fifo_insert_list(struct wg_fifo* fifo,
                        struct wg_item* head,
                        struct wg_item* tail)
{
  int ret = -ENOMEM;

  // Head and Tail must be non-NULL to insert
  if (unlikely(!head)) return ret;
  if (unlikely(!tail)) return ret;

  wg_fifo_grab_lock(fifo);

  // Tail must be non-NULL to insert
  if (likely(fifo->tail)) {

    // Old  tail now points to head of list
    fifo->tail->head = head;

    // FIFO tail now points to tail of list
    fifo->tail = tail;

    // Return insert completed
    ret = 0;
  };

  wg_fifo_free_lock(fifo);

  // Return status
  return ret;
}
EXPORT_SYMBOL(wg_fifo_insert_list);

// Remove item from FIFO

void* wg_fifo_remove(struct wg_fifo* fifo)
{
  struct wg_item* item;

  wg_fifo_grab_lock(fifo);

  // Pull item off FIFO
  if (likely(item = fifo->head)) {

    // If FIFO will be empty reset the tail to point to FIFO itself
    if (unlikely(!item->head)) fifo->tail = fifo->gate;

    // FIFO head now points to next item in FIFO
    fifo->head = item->head;

    // If FIFO allows inserts, clear items head pointer
    if (likely(fifo->gate)) item->head = NULL;
  }

  wg_fifo_free_lock(fifo);

  // Return the item or NULL if FIFO is empty
  return item;
}
EXPORT_SYMBOL(wg_fifo_remove);

// Macro to fix up User/Kern VM addresses

#define	FIFO_VMA_FIX(a,b) ((struct wg_item*)(((char*)(a))+(b)))

// Refill a FIFO from Kernel to User contexts

// Because Kernel and User modes may not map virtual memory in
// the same place, we use a VMA fix up amount

static int wg_fifo_kern_user(struct wg_fifo* fifo, struct wg_fifo* list)
{
  // FIFO must be empty to refill, if not it is BUSY

  if (likely(!fifo->head))
  if (likely(!fifo->tail)) {
    struct wg_item* head;
    struct wg_item* item;
    struct wg_item* next;

    wg_fifo_grab_lock(list);

    // Get the list of items
    head = list->head;
    list->head = NULL;

    // Restart filling this FIFO, if inserts allowed
    list->tail = (struct wg_item*)list->gate;

    wg_fifo_free_lock(list);

    // Walk the list and adjust the virtual addresses
    for (item = head; item; item = next) {
      next = item->head;
      if (likely(next != NULL))
        item->head = FIFO_VMA_FIX(item->head, wg_vma_kern_user);
    }

    // Fix up head
    if (likely(head))
      head = FIFO_VMA_FIX(head, wg_vma_kern_user);

    // Store list into FIFO head
    fifo->head = head;

    return 0;
  }

  return -EAGAIN;
}

// Refill a FIFO from User to Kernel contexts

// Because Kernel and User modes may not map virtual memory in
// the same place, we use a VMA fix up amount

static int wg_fifo_user_kern(struct wg_fifo* fifo, struct wg_fifo* list)
{
  {
    struct wg_item* head;
    struct wg_item* tail;
    struct wg_item* item;
    struct wg_item* next;

    wg_fifo_grab_lock(list);

    // Get the list of items
    head = list->head;
    list->head = NULL;

    // Restart filling this FIFO, if inserts allowed
    list->tail = (struct wg_item*)list->gate;

    wg_fifo_free_lock(list);

    // Store list into FIFO head
    if (likely(head))
      head = FIFO_VMA_FIX(head, wg_vma_user_kern);

    // Walk the list and adjust the virtual addresses
    for (item = tail = head; item; item = next) {
      if (likely(item->head != NULL))
        item->head = tail = FIFO_VMA_FIX(item->head, wg_vma_user_kern);
      next = item->head;
    }

    // Store list into FIFO
    return wg_fifo_insert_list(fifo, head, tail);
  }
}


#ifdef	TEST_MODE

// Test Item with a display tag
struct	 test_item {
  struct test_item* head;
  struct test_item* zero;
  char   tag[24];
};

#endif


#ifdef	UNIT_TEST


// This is the Unit test code

#define	NITEMS	64

struct test_item Items_User[NITEMS], Items_Kern[NITEMS];

// Find item and display its tag

char* find(void* arg)
{
  int    m;
  struct test_item* item = arg;

  for (m = 0; m < NITEMS; m++) if (&Items_Kern[m] == item) return item->tag;
  for (m = 0; m < NITEMS; m++) if (&Items_User[m] == item) return item->tag;

  return 0;
}

// Show test items in FIFO by tag

void show(void* arg, const unsigned char* text)
{
  struct test_item* list = arg;

  printf("%s[", text);
  while (list) {
    printf("%s ", find(list));
    list = list->head;
  }
  printf("]\n");
}

// Add test items to a FIFO

int add_items(struct wg_fifo* fifo, int n)
{
  int    j;
  static int m = 0;

  for (j = 0; j < n; j++)
  if (m >= NITEMS)
    break;
  else {
    printf("Insert  %s\n", Items_Kern[m].tag);
    wg_fifo_insert(fifo, (struct wg_item*)&Items_Kern[m++]);
  }

  return j;
}

// A FIFO unit test

int main(void)
{
  int    j;
  int    m;
  int    x = 1;
  struct test_item* item;
  struct wg_fifo    Kern;
  struct wg_fifo    User;

  // Get the VM correction amount

  wg_vma_user_kern = (((char*)&Items_Kern)-((char*)&Items_User));

  // Set up FIFOs
  wg_fifo_setup(&Kern, 1);
  wg_fifo_setup(&User, 0);

  // Set up Item tags
  for (m = 0; m <  NITEMS; m++) {
    sprintf(Items_Kern[m].tag, "Kern%6d",  m + 1);
    sprintf(Items_User[m].tag, "User%6d",  m + 1);
  }

  // Perform test, add and remove items in various ways
  for (j = 0; j <= NITEMS; j++) {
    item = wg_fifo_remove(&User);
    if (item)
      printf(" Remove %s\n", item->tag);
    else
    if (add_items(&Kern, ++x) <= 0)
      printf(" EMPTY!\n");
    else {
      // show(Kern.head, "Kern");

      wg_fifo_kern_user(&User, &Kern);

      for (m = 0; m <  NITEMS; m++)
        Items_User[m].head = Items_Kern[m].head;
      // show(User.head, "User");
      --j;
    }
  }

  return 0;
}


#else	// UNIT_TEST


#ifdef	TEST_MODE
#define	cpu	(00)
#else
#if	NR_CPUS < 2
#define	cpu	(00)
#else
#define	cpu	((unsigned)raw_smp_processor_id())
#endif
#endif

// Create a FIFO device

int wg_fifo_create(struct cdev* cdev, const struct file_operations* fops,
                   int major, int minor,
                   struct wg_fifo_set* fifo_set, int count)
{
  int j;
  int ret = 0;

  cdev_init(cdev, fops);
  cdev->owner = THIS_MODULE;

  ret = cdev_add(cdev, MKDEV(major, minor), 1);
  if (ret)
    printk(KERN_ERR "wg_fifo: unable to add FIFO device (%d,%d)\n",
           major, minor);
  else

  // Set up RX FIFOs

  for (j = 0; j < count; j++) {
    wg_fifo_setup(&fifo_set->Work, 1);
    wg_fifo_setup(&fifo_set->Free, 1);
    fifo_set++;
  }

  return ret;
}
EXPORT_SYMBOL(wg_fifo_create);

// Delete a FIFO device

int wg_fifo_delete(struct cdev* cdev)
{
  cdev_del(cdev);
  return 0;
}
EXPORT_SYMBOL(wg_fifo_delete);

static int wg_fifo_release(struct inode *inode, struct file *file)
{
  return 0;
}

static unsigned int wg_fifo_poll(struct wg_fifo* rdfifo, struct wg_fifo* wrfifo)
{
  // Always writable
  unsigned int mask = (POLLOUT | POLLWRNORM);

  // Check for RX Work items
  if ( rdfifo->head)
    mask |= (POLLIN  | POLLRDNORM);

  return mask;
}

static ssize_t wg_fifo_read(struct wg_fifo* kern, char __user *buf,
                            size_t count, loff_t *ppos)
{
  int  z;
  struct wg_fifo User;

  // Validate args

  if (unlikely(!buf))
    return -EFAULT;

  if (unlikely(count != sizeof(struct wg_fifo)))
    return -EINVAL;

#ifdef	TEST_MODE
  // Create some test items
  {
    static unsigned tag;
    int  count = (jiffies & 7);

    for (z = 0; z < count; z++) {
      extern struct wg_fifo_set wg_RX_FIFOs[NR_CPUS];
      char which = (kern == &wg_RX_FIFOs[cpu].Work) ? 'R' : 'T';
      struct test_item* item = kmalloc(sizeof(struct test_item), GFP_KERNEL);

      item->head = item->zero = NULL;
      sprintf(item->tag, "%cX Kern %10u", which, ++tag);
      wg_fifo_insert(kern, (struct wg_item*)item);
      if (console_loglevel > 7)
        printk(KERN_DEBUG "FIFO %cX Grab Item 0x%p Tag %s\n",
               which, item, item->tag);
    }
  }
#endif

  // Create a new FIFO to hand to user

  wg_fifo_setup(&User, 0);

  // Convert kernel mode FIFO to user mode

  z = wg_fifo_kern_user(&User, kern);
  if (unlikely(z < 0))
    return z;

  // Send to user

  z = copy_to_user(buf, &User, count);
  if (unlikely(z < 0))
    return z;

  *ppos += count;
  return count;
}

static ssize_t wg_fifo_write(struct wg_fifo* kern, const char __user *buf,
                             size_t count, loff_t *ppos)
{
  int  z;
  struct wg_fifo User;

  // Validate args

  if (unlikely(!buf))
    return -EFAULT;

  if (unlikely(count != sizeof(struct wg_fifo)))
    return -EINVAL;

  // Get user mode copy of FIFO

  z = copy_from_user(&User, buf, count);
  if (unlikely(z < 0))
    return z;

  // Convert user mode FIFO to kernel mode

  z = wg_fifo_user_kern(kern, &User);
  if (unlikely(z < 0))
    return z;

#ifdef	TEST_MODE
  // Destroy some test items
  {
    static unsigned ticker;

    if (++ticker & 1) {
      extern struct wg_fifo_set wg_RX_FIFOs[NR_CPUS];
      char which = (kern == &wg_RX_FIFOs[cpu].Free) ? 'R' : 'T';
      struct test_item* item;

      while ((item = wg_fifo_remove(kern))) {
        if (console_loglevel > 7)
          printk(KERN_DEBUG "FIFO %cX Toss Item 0x%p Tag %s\n",
                 which, item, item->tag);
        kfree(item);
      }
      if (console_loglevel > 7)
        printk(KERN_DEBUG "\n\n\n");
    }
  }
#endif

  *ppos += count;
  return count;
}


// Clone the section below for each new device
// Add it to the __init and __exit code as well


// Data and Code for fifo_rx


// The RX char device

static struct cdev wg_fifo_rx_cdev;

// The RX Work and Free FIFOs

struct wg_fifo_set wg_RX_FIFOs[NR_CPUS];
EXPORT_SYMBOL(wg_RX_FIFOs);

static int wg_fifo_rx_open(struct inode *inode, struct file *file)
{
  file->private_data = &wg_RX_FIFOs;
  return nonseekable_open(inode, file);
}

static unsigned int wg_fifo_rx_poll(struct file *filp, poll_table *wait)
{
  return wg_fifo_poll (&wg_RX_FIFOs[cpu].Work, &wg_RX_FIFOs[cpu].Free);
}

static ssize_t wg_fifo_rx_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
  return wg_fifo_read (&wg_RX_FIFOs[cpu].Work, buf, count, ppos);
}

static ssize_t wg_fifo_rx_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  return wg_fifo_write(&wg_RX_FIFOs[cpu].Free, buf, count, ppos);
}

// RX file ops

static const struct file_operations wg_fifo_rx_fops =
{
  .owner =	THIS_MODULE,
  .open =	wg_fifo_rx_open,
  .poll =	wg_fifo_rx_poll,
  .read =	wg_fifo_rx_read,
  .write =	wg_fifo_rx_write,
  .release =	wg_fifo_release,
  .llseek =	no_llseek,
};


// Data and Code for fifo_tx


// The TX char device

static struct cdev wg_fifo_tx_cdev;

// The TX Work and Free FIFOs

struct wg_fifo_set wg_TX_FIFOs[NR_CPUS];
EXPORT_SYMBOL(wg_TX_FIFOs);

static int wg_fifo_tx_open(struct inode *inode, struct file *file)
{
  file->private_data = &wg_TX_FIFOs;
  return nonseekable_open(inode, file);
}

static unsigned int wg_fifo_tx_poll(struct file *filp, poll_table *wait)
{
  return wg_fifo_poll (&wg_TX_FIFOs[cpu].Free, &wg_TX_FIFOs[cpu].Work);
}

static ssize_t wg_fifo_tx_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
  return wg_fifo_read (&wg_TX_FIFOs[cpu].Free, buf, count, ppos);
}

static ssize_t wg_fifo_tx_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  return wg_fifo_write(&wg_TX_FIFOs[cpu].Work, buf, count, ppos);
}

// TX file ops

static const struct file_operations wg_fifo_tx_fops =
{
  .owner =	THIS_MODULE,
  .open =	wg_fifo_tx_open,
  .poll =	wg_fifo_tx_poll,
  .read =	wg_fifo_tx_read,
  .write =	wg_fifo_tx_write,
  .release =	wg_fifo_release,
  .llseek =	no_llseek,
};


// Remove devices on exit

static void __exit wg_fifo_exit_module (void)
{
  wg_fifo_delete(&wg_fifo_rx_cdev);
  wg_fifo_delete(&wg_fifo_tx_cdev);
}

// Install device on startup

static int __init wg_fifo_init_module (void)
{
  int ret;

  // Print herald

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n\n", __FUNCTION__);

  // Add RX device

  ret = wg_fifo_create(&wg_fifo_rx_cdev, &wg_fifo_rx_fops,
                       FIFO_RX_MAJOR, 0, &wg_RX_FIFOs[0], NR_CPUS);

  // Add TX device

  if (!ret)
    ret = wg_fifo_create(&wg_fifo_tx_cdev, &wg_fifo_tx_fops,
                         FIFO_TX_MAJOR, 0, &wg_TX_FIFOs[0], NR_CPUS);
  
  return ret;
}

module_init(wg_fifo_init_module);
module_exit(wg_fifo_exit_module);

MODULE_LICENSE("GPL");


#endif	// UNIT_TEST
