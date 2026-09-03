#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include "scull.h"
#include "scull_p.h"

void scull_p_setup_cdev(void);

loff_t scull_p_llseek(struct file *, loff_t, int);
ssize_t scull_p_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_p_write(struct file *, const char __user *, size_t, loff_t *);
long scull_p_unlocked_ioctl(struct file *, unsigned int, unsigned long);
int scull_p_open(struct inode *, struct file *);
int scull_p_release(struct inode *, struct file *);

struct file_operations scull_pipe_fops = {
    .owner              = THIS_MODULE,
    .llseek             = scull_p_llseek,
    .read               = scull_p_read,
    .write              = scull_p_write,
    .unlocked_ioctl     = scull_p_unlocked_ioctl,
    .open               = scull_p_open,
    .release            = scull_p_release,
};

struct scull_pipe 
{
    wait_queue_head_t inq, outq;
    char *buffer, *end;
    int buffersize;
    char *rp;
    char *wp;
    int nreaders;
    int nwriters;

	struct mutex lock;     /* mutual exclusion semaphore     */
    struct cdev cdev;
};
int spacefree(struct scull_pipe *dev);
static int scull_getwritespace(struct scull_pipe *dev, struct file *filp);

dev_t scull_p_devno;
static struct scull_pipe *scull_p;

loff_t scull_p_llseek(struct file *, loff_t, int)
{
    return 0;
}

ssize_t scull_p_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
    printk(KERN_ALERT "Scull p read\n");
    struct scull_pipe *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    // Nothing to read
    while (dev->rp == dev->wp)
    {
        mutex_unlock(&dev->lock);
        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }
        printk(KERN_NOTICE "readng: going to sleep");

        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
        {
            return -ERESTARTSYS;
        }
        if (mutex_lock_interruptible(&dev->lock))
        {
            return -ERESTARTSYS;
        }
    }

    if (dev->wp > dev->rp)
    {
        count = min(count, (size_t)(dev->wp - dev->rp));
    }
    else
    {
        count = min(count, (size_t)(dev->end - dev->rp));
    }
    if (copy_to_user(buffer, dev->rp, count))
    {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->rp += count;
    if (dev->rp == dev->end)
    {
        dev->rp = dev->buffer;
    }
    mutex_unlock(&dev->lock);

    wake_up_interruptible(&dev->outq);
    printk(KERN_ALERT "Scull p read end\n");

    return count;
}

int spacefree(struct scull_pipe *dev)
{
    if (dev->rp == dev->wp)
    {
        return dev->buffersize - 1;
    }

    return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static int scull_getwritespace(struct scull_pipe *dev, struct file *filp)
{
    while (spacefree(dev) == 0)
    {
        DEFINE_WAIT(wait);

        mutex_unlock(&dev->lock);
        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        printk(KERN_NOTICE "writing: going to sleep");
        prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
        if (spacefree(dev) == 0)
        {
            schedule();
        }

        finish_wait(&dev->outq, &wait);
        if (signal_pending(current))
        {
            return -ERESTARTSYS;
        }
        if (mutex_lock_interruptible(&dev->lock))
        {
            return -ERESTARTSYS;
        }
    }

    return 0;
}

ssize_t scull_p_write(struct file *filp, const char __user * buffer, size_t count, loff_t *f_pos)
{
    printk(KERN_ALERT "Scull p write\n");
    struct scull_pipe *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    int result = scull_getwritespace(dev, filp);
    if (result)
    {
        return result;
    }

    count = min(count, (size_t)spacefree(dev));
    if (dev->wp >= dev->rp)
    {
        count = min(count, (size_t)(dev->end - dev->wp));
    }
    else
    {
        count = min(count, (size_t)(dev->wp - dev->end - 1));
    }
    printk(KERN_INFO "Going to accept %li bytes from %p to %p\n", (long)count, dev->wp, buffer);
    
    if (copy_from_user(dev->wp, buffer, count))
    {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->wp += count;
    if (dev->wp == dev->end)
    {
        dev->wp = dev->buffer; // Wrapped
    }
    mutex_unlock(&dev->lock);

    wake_up_interruptible(&dev->inq);
    printk(KERN_INFO "Write p end\n");    

    return count;
}

long scull_p_unlocked_ioctl(struct file *, unsigned int, unsigned long)
{
    return 0;
}

int scull_p_open(struct inode *inode, struct file *filp)
{
    printk(KERN_ALERT "Scull p open\n");
    struct scull_pipe *dev;

    dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
    // Check if dev is null???
    filp->private_data = dev;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    if (dev->buffer == NULL)
    {
        dev->buffer = kmalloc(SCULLP_BUFFER_SIZE, GFP_KERNEL);
        if (dev->buffer == NULL)
        {
            mutex_unlock(&dev->lock);
            return -ENOMEM;
        }
    }

    dev->buffersize = SCULLP_BUFFER_SIZE;
    dev->end = dev->buffer + SCULLP_BUFFER_SIZE;
    dev->rp = dev->wp = dev->buffer;

    if (filp->f_mode & FMODE_READ)
    {
        dev->nreaders++;
    }

    if (filp->f_mode & FMODE_WRITE)
    {
        dev->nwriters++;
    }

    mutex_unlock(&dev->lock);
    printk(KERN_ALERT "Scull p open [end]\n");

    return nonseekable_open(inode, filp);
}

int scull_p_release(struct inode *inode, struct file *filp)
{
    struct scull_pipe *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    if (filp->f_mode & FMODE_READ)
    {
        dev->nreaders--;
    }
    if (filp->f_mode & FMODE_WRITE)
    {
        dev->nwriters--;
    }

    if (dev->nreaders + dev->nwriters == 0)
    {
        kfree(dev->buffer);
        dev->buffer = NULL;
    }

    mutex_unlock(&dev->lock);
    return 0;
}


void scull_p_setup_cdev(void)
{
    cdev_init(&(scull_p->cdev), &scull_pipe_fops);
    scull_p->cdev.owner = THIS_MODULE;
    int err = cdev_add(&scull_p->cdev, scull_p_devno, 1);
    if (err)
    {
        printk(KERN_NOTICE "Error adding scullpipe %d\n", err);
    }
}

int scull_p_init(dev_t firstdev)
{
    printk(KERN_NOTICE "firstdev: %x\n", firstdev);
    int result = register_chrdev_region(firstdev, 1, "scullp");
    if (result < 0)
    {
        printk(KERN_NOTICE "Unable to register scullp region %d\n", result);
        return 0;
    }

    scull_p_devno = firstdev;
    scull_p = kmalloc(1 * sizeof(struct scull_pipe), GFP_KERNEL);
    if (scull_p == NULL)
    {
        unregister_chrdev_region(firstdev, 1);
        return 0;
    }
    memset(scull_p, 0, sizeof(struct scull_pipe));

    init_waitqueue_head(&(scull_p->inq));
    init_waitqueue_head(&(scull_p->outq));
    mutex_init(&(scull_p->lock));
    scull_p_setup_cdev();
    
    return 1; // Only one device for my humble needs.
}

void scull_p_cleanup(void)
{
    if (scull_p == NULL)
    {
        return;
    }

    cdev_del(&scull_p->cdev);
    kfree(scull_p->buffer);
    kfree(scull_p);

    unregister_chrdev_region(scull_p_devno, 1);
    scull_p = NULL;
}
