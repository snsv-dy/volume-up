#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include "scull.h"

// Najpierw:
// 1. Module opens usb device
// 2. Reads on interrupt data
// 3. (opcjonalnie) wysyłanie po interrupcie, tak dla picu.
// Kolejnie:
// 1. Moduł uruchamia/komunikuje się z audio.c
// 2. Wysyła żądania od urządzenia
// 3. Odbiera dane od audio.c i przesyła do urządzenia.
// Ostatecznie:
// 1. Hotplug
// 2. Sink inputy itp
// ===============
// Testowanie na virtualnej mazsynie?
// mockowanie urządzenia gadgetem? czy tym co kiedyś patrzyłeś.
// Sterownik windowsa? (resume driven dev)

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);


MODULE_AUTHOR("Various artists");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;

loff_t scull_llseek(struct file *, loff_t, int);
ssize_t scull_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_write(struct file *, const char __user *, size_t, loff_t *);
long scull_unlocked_ioctl(struct file *, unsigned int, unsigned long);
int scull_open(struct inode *, struct file *);
int scull_release(struct inode *, struct file *);

struct file_operations scull_fops = {
    .owner              = THIS_MODULE,
    .llseek             = scull_llseek,
    .read               = scull_read,
    .write              = scull_write,
    .unlocked_ioctl     = scull_unlocked_ioctl,
    .open               = scull_open,
    .release            = scull_release,
};

struct scull_dev 
{
    struct scull_qset *data;
    int quantum;
    int qset;
    unsigned long size;
    unsigned int access_key;
	struct mutex lock;     /* mutual exclusion semaphore     */
    struct cdev cdev;
};

struct scull_qset
{
    void **data;
    struct scull_qset* next;
};


void scull_cleanup_module(void);
int scull_trim(struct scull_dev* dev);
struct scull_qset *scull_follow(struct scull_dev *dev, int n);

int scull_trim(struct scull_dev* dev)
{
    struct scull_qset *next;
    int qset = dev->qset;

    for (struct scull_qset *dptr = dev->data; dptr; dptr = next)
    {
        if (dptr->data)
        {
            for (int i = 0; i < qset; i++)
            {
                // legal to pass a NULL pointer to kfree
                kfree(dptr->data[i]);
            }
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }

    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;

    return 0;
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
    }
    printk(KERN_NOTICE "[scull_setup_cdev] scull %d setuped", index);
}
void scull_cleanup_module(void)
{
    printk(KERN_ALERT "scull_cleanup_module\n");

    dev_t devno = MKDEV(scull_major, scull_minor);

    if (scull_devices)
    {
        printk(KERN_ALERT "[scull_cleanup_module] scull_devices\n");
        for (int i = 0; i < scull_nr_devs; i++)
        {
            printk(KERN_ALERT "[scull_cleanup_module] scull_devices %d\n", i);
            scull_trim(scull_devices + i);
            cdev_del(&scull_devices[i].cdev);
        }
        kfree(scull_devices);
    }

    unregister_chrdev_region(devno, scull_nr_devs);
    printk(KERN_ALERT "scull_cleanup_module end\n");
}

static int scull_init_module(void)
{
    printk(KERN_ALERT "scull init module");

    int result;
    dev_t dev = 0;
    
    if (scull_major)
    {
        printk(KERN_ALERT "scull_major %d", scull_major);
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, "scull");
    }
    else
    {
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
        scull_major = MAJOR(dev);
        printk(KERN_ALERT "scull major else %d", scull_major);
    }

    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
    if (!scull_devices)
    {
        printk(KERN_ALERT "[scull_init_module] !scull_devices");
        result = -ENOMEM;
        goto fail; // I know right?
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

    for (int i = 0; i < scull_nr_devs; i++)
    {
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        mutex_init(&scull_devices[i].lock);
        scull_setup_cdev(&scull_devices[i], i);
    }

    dev = MKDEV(scull_major, scull_minor + scull_nr_devs);
    // dev += scull_p_init(dev);
    // dev += scull_access_init(dev);

    printk(KERN_ALERT "Hello, world\n");

    printk(KERN_ALERT "scull init module end");
    return 0;

    fail:
        scull_cleanup_module();
        printk(KERN_ALERT "scull init fail!!");
        return result;
}


loff_t scull_llseek(struct file *, loff_t, int)
{
    return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
    struct scull_qset *qs = dev->data;

    if (!qs)
    {
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qs == NULL)
        {
            return NULL;
        }
        memset(qs, 0, sizeof(struct scull_qset));
    }


    for (int i = 0; i < n; i++)
    {
        if (!qs->next)
        {
            qs->next = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qs->next == NULL)
            {
                return NULL;
            }
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
    }

    return qs;
}

ssize_t scull_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
    
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;

    int itemsize = quantum * qset;
    int item;
    int set_pos;
    int quantum_pos;
    int rest;

    ssize_t retval = 0;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }
    
    if (*f_pos >= dev->size)
    {
        // TODO: (minecraft villager hmm.ogg)
        goto out;
    }

    if (*f_pos + count > dev->size)
    {
        count = dev->size - *f_pos;
    }

    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    set_pos = rest / quantum;
    quantum_pos = rest % quantum;

    dptr = scull_follow(dev, item);
    if (dptr == NULL || !dptr->data || !dptr->data[set_pos])
    {
        // as mentioned previously.
        goto out;
    }

    if (count > quantum - quantum_pos)
    {
        count = quantum - quantum_pos;
    }

    if (copy_to_user(buffer, dptr->data[set_pos] + quantum_pos, count))
    {
        retval = -EFAULT;
        // AAaa
        goto out;
    }

    *f_pos += count;
    retval = count;

    out:
        mutex_unlock(&dev->lock);
        return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buffer, size_t count, loff_t * f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;

    int itemsize = quantum * qset;
    int item;
    int set_pos;
    int quantum_pos;
    int rest;

    ssize_t retval = -ENOMEM;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    set_pos = rest / quantum;
    quantum_pos = rest % quantum;


    printk(KERN_ALERT "[scull_write] "
                    ""
                    "itemsize: %d, "
                    "item: %d, "
                    "set_pos: %d, "
                    "quantum_pos: %d, "
                    "rest: %d\n", 
                    itemsize, item, set_pos, quantum_pos, rest);

    dptr = scull_follow(dev, item);
    if (dptr == NULL)
    {
        printk(KERN_ALERT "[scull_write] dptr == NULL\n");
        // as mentioned previously.
        goto out;
    }
    
    if (!dptr->data)
    {
        // printk(KERN_ALERT "[scull_write] !dptr->data\n");
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
        {
            // printk(KERN_ALERT "[scull_write] !dptr->data x2\n");
            // stop posting and go outside.
            goto out;
        }
        memset(dptr->data, 0, qset * sizeof(char *));
    }
    
    if (!dptr->data[set_pos])
    {
        // printk(KERN_ALERT "[scull_write] !dptr->data[set_pos]\n");
        dptr->data[set_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[set_pos])
        {
            // printk(KERN_ALERT "[scull_write] !dptr->data[set_pos] x2\n");
            // stop posting and go outside.
            goto out;
        }
    }

    if (count > quantum - quantum_pos)
    {
        count = quantum - quantum_pos;
    }

    if (copy_from_user(dptr->data[set_pos] + quantum_pos, buffer, count))
    {
        retval = -EFAULT;
        // run out of reactions.
        goto out;
    }

    *f_pos += count;
    retval = count;

    if (dev->size < *f_pos)
    {
        dev->size = *f_pos;
    }

    out:
        // printk(KERN_ALERT "[scull_write] out:\n");
        mutex_unlock(&dev->lock);
        return retval;
}

long scull_unlocked_ioctl(struct file *, unsigned int, unsigned long)
{

    return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
    printk(KERN_ALERT "Scull open\n");
    struct scull_dev *dev;

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    // Check if dev is null???
    filp->private_data = dev;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        scull_trim(dev);
    }
    printk(KERN_ALERT "Scull open [end]\n");

    return 0;
}

int scull_release(struct inode *, struct file *)
{
    printk(KERN_ALERT "Scull release\n");

    return 0;
}


module_init(scull_init_module);
module_exit(scull_cleanup_module);