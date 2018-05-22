#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
//#include <linux/platform_device.h>

#define DRV_DEBUG	1

#if DRV_DEBUG
#define DPRINTK(x...) printk(KERN_EMERG "DRV DEBUG:" x)
#else
#define DPRINTK(x...)
#endif


#define DEVICE_NAME			"Evan"
#define DEV_MINOR_NUM		2
#define DEV_MAJOR			0
#define DEV_MINOR 			0
#define REGDEV_SIZE			1024

#define IOCTL_CLEAN			0
#define IOCTL_GET_VALUE		1
#define IOCTL_SET_VALUE		2


MODULE_AUTHOR("Evan Wang");
MODULE_LICENSE("Dual BSD/GPL");


struct reg_dev
{
	char data[REGDEV_SIZE];
	unsigned int size;
	struct cdev cdev;
};

static struct reg_dev *reg_devs;
static struct class *reg_class;

static int dev_major = DEV_MAJOR;
static int dev_minor = DEV_MINOR;


module_param(dev_major, int, S_IRUSR);
module_param(dev_minor, int, S_IRUSR);

static int dev_node_open(struct inode *inode, struct file *file)
{
	struct reg_dev *cd;

	DPRINTK("dev_node_open success!\n");

	cd = container_of(inode->i_cdev, struct reg_dev, cdev);
	file->private_data = cd;

	return 0;
}

static int dev_node_release(struct inode *inode, struct file *file)
{
	DPRINTK("dev_node_release success!\n");
	return 0;
}

static long dev_node_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
    struct reg_dev *cd = file->private_data;

    DPRINTK("dev_node_ioctl cmd=%d, arg=%ld\n", cmd, arg);
    switch(cmd) {
        case IOCTL_CLEAN:
            DPRINTK("CMD: CLEAN\n");
            memset(cd->data, 0, sizeof(cd->data));
            break;
        case IOCTL_SET_VALUE:
            DPRINTK("CMD: SET VALUE\n");
            cd->size = (int)arg;
            break;
        case IOCTL_GET_VALUE:
            DPRINTK("CMD: GETVALUE\n");
            ret = put_user(cd->size, (int *)arg);
            break;
        default:
            return -EFAULT;
    }
    return ret;
}

static ssize_t dev_node_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	int n, ret;
    char *kbuf;
    struct reg_dev *cd = file->private_data;

    DPRINTK("dev_node_read position: %lld\n", *f_pos);

    if (*f_pos == cd->size) {
        return 0;
    }
    if (count > cd->size - *f_pos) {
        n = cd->size - *f_pos;
    } else {
        n = count;
    }
    kbuf = cd->data + *f_pos;
    ret = copy_to_user(buf, kbuf, n);

    if (ret != 0) {
        return -EFAULT;
    }
    *f_pos += n;

    DPRINTK("dev_node_read success: %d\n", n);

    return n;
}

static ssize_t dev_node_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
	int n, ret;
    char *kbuf;
    struct reg_dev *cd = file->private_data;

    DPRINTK("dev_node_write position: %lld\n", *f_pos);

    if (*f_pos == sizeof(cd->data)) {
        return -1;
    }
    if (count > sizeof(cd->data) - *f_pos) {
        n = sizeof(cd->data) - *f_pos;
    } else {
        n = count;
    }
    kbuf = cd->data + *f_pos;
    ret = copy_from_user(kbuf, buf, n);

    if(ret != 0) {
        return -EFAULT;
    }
    *f_pos += n;
    cd->size += n;

    printk("dev_node_write success: %d\n", n);

    return n;
}

static loff_t dev_node_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t ret = 0;

   	switch (whence) {
     	case 0:   // 相对文件开始位置偏移
       		if (offset < 0) {
         		ret =  - EINVAL;
         		goto err;
       		}
       		if ((unsigned int)offset > REGDEV_SIZE) {
         		ret =  - EINVAL;
         		goto err;
       		}
       		file->f_pos = (unsigned int)offset;
       		ret = file->f_pos;
       		break;
     	case 1:   // 相对文件当前位置偏移
       		if ((file->f_pos + offset) > REGDEV_SIZE) {
         		ret =  - EINVAL;
         		goto err;
       		}
       		if ((file->f_pos + offset) < 0) {
         		ret =  - EINVAL;
         		goto err;
       		}
       		file->f_pos += offset;
       		ret = file->f_pos;
       		break;
     	default:
       		goto err;
   	}
   	DPRINTK("dev_node_llseek success: offset=%lld, whence=%d\n", offset, whence);
   	return ret;
   	err:
   	DPRINTK("dev_node_llseek: Invalid param!\n");
	return ret;
}

static const struct file_operations reg_fops = {
	.owner = THIS_MODULE,
	.open = dev_node_open,
	.release = dev_node_release,
	.unlocked_ioctl = dev_node_ioctl,
	.read = dev_node_read,
	.write = dev_node_write,
	.llseek = dev_node_llseek,
};

static void reg_init_cdev(struct reg_dev *dev, int index)
{
	int err, dev_no = MKDEV(dev_major, dev_minor + index);

	cdev_init(&dev->cdev, &reg_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &reg_fops;

	err = cdev_add(&dev->cdev, dev_no, 1);
	if (err != 0) {
		DPRINTK("cdev_add %d failed: %d\n", index, err);
	}
}

static int __init char_driver_init(void)
{
	int i, ret = 0;
	dev_t dev;

	DPRINTK("Init driver ---> %s\n", DEVICE_NAME);
	if (dev_major > 0) {
		DPRINTK("Register driver staticly!!!\n");
		dev = MKDEV(dev_major, dev_minor);
		ret = register_chrdev_region(dev, DEV_MINOR_NUM, DEVICE_NAME);
	} else {
		DPRINTK("Register driver dynmicaly!!!\n");
		ret = alloc_chrdev_region(&dev,
			0,
			DEV_MINOR_NUM,
			DEVICE_NAME);
		dev_major = MAJOR(dev);
		dev_minor = MINOR(dev);
	}
	if (ret < 0) {
		DPRINTK("Register chrdev failed: %d, exit!!!\n", ret);
		return -1;
	} else {
		DPRINTK("Register chrdev success...\n");
	}
	reg_class = class_create(THIS_MODULE, DEVICE_NAME);
	reg_devs = kzalloc(DEV_MINOR_NUM * sizeof(struct reg_dev), GFP_KERNEL);
	if (!reg_devs) {
		ret = -ENOMEM;
		goto fail;
	}
	for (i = 0; i < DEV_MINOR_NUM; i++) {
		// 设备注册到系统
		reg_init_cdev(&reg_devs[i], i);
		// 创建设备节点
		device_create(reg_class,
			NULL,
			MKDEV(dev_major, dev_minor+i),
			NULL,
			DEVICE_NAME"%d", i);
	}
	DPRINTK("Init %s end: major=%d, minor=%d\n", DEVICE_NAME, dev_major, dev_minor);
	return 0;

	fail:
	unregister_chrdev_region(MKDEV(dev_major, dev_minor),
		DEV_MINOR_NUM);
	DPRINTK("kmalloc failed!!!\n");
	return ret;
}

static void __exit char_driver_exit(void)
{
	int i;
	DPRINTK("Driver %s exit!\n", DEVICE_NAME);

	for (i = 0; i < DEV_MINOR_NUM; i++) {
		device_destroy(reg_class, MKDEV(dev_major, dev_minor+i));
		cdev_del(&(reg_devs[i].cdev));
	}
	class_destroy(reg_class);
	kzfree(reg_devs);
	unregister_chrdev_region(MKDEV(dev_major, dev_minor),
		DEV_MINOR_NUM);
}

module_init(char_driver_init);
module_exit(char_driver_exit);
