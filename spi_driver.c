#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include "ioctl_spi.h"
#define CLASS_NAME "SPI"
#define DEVICE_NAME "WS2812"

/*Structures representing spi class,
too hold the major& minor number corresponding to device,
the detected spi device and the task that needs to operated
in background*/
static struct class *spi_class;
static dev_t spi_dev_num;
static struct spi_device *spi_led_dev;
static struct task_struct *btask;

/*The data structure representing the spi driver as character
file and the message container to be transmitted across*/
typedef struct spi_led {
    struct cdev spi_cdev;
    char *spi_name;
    struct spi_message spi_led_msg;
    struct spi_transfer spi_led_txr;
} spi_led_data;

/*A wrapper data structure to bind with actual data
to be transmitted*/
typedef struct led_data_plus_buffer {
    spi_led_data *spi_led_dat;
    char *transmission_buffer;
} led_data_plus_buffer_t;

/*A pointer to the driver data structure*/
spi_led_data *spi_led_info;

static int spi_led_reset(spi_led_data *spi) {
    /* As per our clock cycle which needs 0.15 microseconds to send on bit
     * therefore to hold the line a low voltage for 60microseconds (more than
     * 50 microseconds as per specification) we need to send 60/0.15 = 400 bits
     * which is equivalent to 50 bytes of data. Hence we are using data size=50
     */
    int data_size = 50;
    int spi_reset_return_value;
    unsigned char reset_data[data_size];
    unsigned char rx[2];
    memset(&reset_data, 0, sizeof (reset_data));
    printk(KERN_INFO "size of reset data = %d\n", sizeof (reset_data));
    /*giving the reference of the buffer which contains the
    reset data to be transmitted*/
    spi->spi_led_txr.tx_buf = &reset_data[0];
    spi->spi_led_txr.rx_buf = &rx[0];
    /*All the necessary protocol parameters like, the length of the whole
    data set to be transmitted(in bytes),the chip select toggle,number of bits
    per word transmission, frequency of transmission*/
    spi->spi_led_txr.len = data_size;
    spi->spi_led_txr.cs_change = 1;
    spi->spi_led_txr.bits_per_word = 8;
    spi->spi_led_txr.speed_hz = 6666666;
    printk(KERN_INFO "SPI LED RESET DONE!!!\n");
    /*Initiating the transmission and handling if there is any error*/
    spi_reset_return_value = spi_write(spi_led_dev, reset_data, sizeof (reset_data));
    printk("spi_reset_return_value = %d\n", spi_reset_return_value);
    printk(KERN_INFO "RESET MESSAGE SENT\n");
    return 0;
}

/*The background task which actually transmits the encoded data
to glow led strip*/
static int glow_colors(void *data_from_write) {
    int spi_write_return_value;
    led_data_plus_buffer_t *led_data_plus_buffer = (led_data_plus_buffer_t *) data_from_write;
    spi_led_data *spi = led_data_plus_buffer->spi_led_dat;
    unsigned char *tx = led_data_plus_buffer->transmission_buffer;
    unsigned char rx[2];
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);

    spi->spi_led_txr.tx_buf = tx;
    spi->spi_led_txr.rx_buf = &rx[0];
    /*The length would 16*24 as there are 16 led, each of which takes
    24 bits to represent the intensity of Red, green and blue colors*/
    spi->spi_led_txr.len = PIXEL_COUNT*BITS_PER_PIXEL;
    spi->spi_led_txr.cs_change = 1;
    spi->spi_led_txr.bits_per_word = 8;
    spi->spi_led_txr.speed_hz = 6666666;
    spi_write_return_value = spi_write(spi_led_dev, tx, PIXEL_COUNT * BITS_PER_PIXEL);
    printk("spi_write_return_value = %d\n", spi_write_return_value);

    btask = NULL;
    printk(KERN_INFO "out of the glow direct task\n");
    return 0;
}

/*Invoked when the device file is opened and stores the
reference of the driver data structure in file's private
data for caching purpose*/
int spi_open(struct inode *inode, struct file *fileptr) {
    spi_led_data *spi_led_dat;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    spi_led_dat = container_of(inode->i_cdev, spi_led_data, spi_cdev);
    fileptr->private_data = spi_led_dat;
    return 0;
}

/*Invoked when some data representing the no. of led to glow
and the pixel representation of each led for a specific color,
is written to device file*/
ssize_t spi_write_led_data(struct file *pfile, const char __user *buffer,
        size_t length, loff_t *offset) {
    n_led_data_t user_data;
    int index;
    unsigned char tx_buffer[24 * PIXEL_COUNT];
    spi_led_data *spi_led_dat;
    led_data_plus_buffer_t data_plus_buffer;
    printk(KERN_ALERT "Inside the %s function \n", __FUNCTION__);
    if (copy_from_user(&user_data, buffer, length) != 0) {
        return EFAULT;
    }
    printk("User send data for first %d LEDs", user_data.n);
    printk("Updating transmission buffer\n");

    for (index = 0; index < 24 * PIXEL_COUNT; index++) {
        /*Initializing whole buffer for 24 LEDs with encoded low data*/
        tx_buffer[index] = 0xC0;
    }
    /*Copy user data to the whole transmission buffer*/
    for (index = 0; index < 24 * user_data.n; index++) {
        tx_buffer[index] = user_data.data[index];
    }
    /*k-thread run after this with transmission data buffer*/
    spi_led_dat = (spi_led_data *) pfile->private_data;
    if (btask == NULL) {
        memset(&data_plus_buffer, 0, sizeof (led_data_plus_buffer_t));
        /*Binds the data buffer to be transmitted with the driver
        structure which has message queue structure to be transmitted
        copying the spi message*/
        data_plus_buffer.spi_led_dat = spi_led_dat;
        data_plus_buffer.transmission_buffer = tx_buffer;
        /*work deferred to kernel thread pack the data queue and transmit*/
        btask = kthread_run(&glow_colors, &data_plus_buffer, "glow_colors");
    }
    return 0;
}

/*Conditional check to differentiate between ioctl prototype w.r.t kernel
 * version and to account for portability
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int spi_ioctl_handler(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else

/*ioctl to configure behavior of read function*/
static long spi_ioctl_handler(struct file *fileptr, unsigned int cmd, unsigned long arg)
#endif
{
    spi_led_data *spi_led_dat;
    printk(KERN_ALERT "Inside the %s function with cmd = %d\n", __FUNCTION__, cmd);
    switch (cmd) {
            /*clearing the ioctl parameter-i,e read min value*/
        case IOCTL_RESET:
            printk("IOCTL reset called by user");
            spi_led_dat = (spi_led_data*) fileptr->private_data;
            /*As per the command,the reset data is sent to the led strip*/
            spi_led_reset(spi_led_dat);
            break;
        default:
            return -EINVAL; /*In case any other parameter passed apart from 0 or 1 then EFAULT is set*/
    }
    return 0;
}

/*Invoked when the file descriptor representing
the device file is being closed*/
int spi_close(struct inode *inode, struct file *fileptr) {
    spi_led_data *spi_led_dat;
    spi_led_dat = (spi_led_data*) fileptr->private_data;
    printk(KERN_INFO "device %s called close\n", spi_led_dat->spi_name);
    return 0;
}

/*Linking the implemented file operations with the
spi character device file being created*/
static struct file_operations spi_led_fops = {
    .open = spi_open,
    .write = spi_write_led_data,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = rb530_ioctl,
#else
    .unlocked_ioctl = spi_ioctl_handler,
#endif
    .release = spi_close,
    .owner = THIS_MODULE,
};

/*Probe function called when the spi driver and its corresponding
device match each other*/
static int spi_driver_probe(struct spi_device *spi_dev) {
    spi_led_dev = spi_dev;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*Setting the master output slave input pin as output
    with all the auxiliary setting of direction,multiplexer
    pins to attain  the objective*/
    gpio_request(24, "mosi");
    gpio_export(24, 0);
    gpio_direction_output(24, 0);

    gpio_request(25, "mosi_pull_up");
    gpio_export(25, 0);
    gpio_direction_output(25, 0);

    gpio_request(44, "mosi_mux_1");
    gpio_export(44, 0);
    gpio_direction_output(44, 1);

    gpio_request(72, "mosi_mux_2");
    gpio_export(72, 0);
    gpio_set_value_cansleep(72, 0);
    return 0;
}

/*function to free the gpio pins, in order to clean up
resources,and also unexports gpio pin files from sysfs*/
static void free_gpios(void) {
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    gpio_unexport(24);
    gpio_free(24);
    gpio_unexport(25);
    gpio_free(25);
    gpio_unexport(44);
    gpio_free(44);
    gpio_unexport(72);
    gpio_free(72);
    gpio_unexport(30);
    gpio_free(30);
    gpio_unexport(31);
    gpio_free(31);
    gpio_unexport(46);
    gpio_free(46);
}

/*Invoked when the device is removed from system, which
inturn free the gpio pins that were allocated*/
static int spi_driver_remove(struct spi_device *spi_dev) {
    spi_led_dev = NULL;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    free_gpios();
    return 0;
}

/*Populating the neccesary data to represent the spi driver
as in, the name which is used to match for device-driver connection,
the probe and remove function to be called when the device is found or
removed from the system*/
static struct spi_driver spi_custom_driver = {
    .driver =
    {
        .name = "spi_dev",
        .owner = THIS_MODULE,
    },
    .probe = spi_driver_probe,
    .remove = spi_driver_remove
};

/*The init function invoked when the driver is installed and
carries out all initialization tasks necessary to build the device file*/
static int spi_driver_init(void) {
    int status = 0;
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    /*allocates major and minor number dynamically*/
    if (alloc_chrdev_region(&spi_dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_DEBUG "Can't register device\n");
        return -1;
    }
    /*creates a class under which the device tends to be appear*/
    spi_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(spi_class)) {
        printk(KERN_INFO "Error creating class\n");
        unregister_chrdev_region(spi_dev_num, 1);
        return PTR_ERR(spi_class);
    }
    /*allocates the memory for driver data structure*/
    spi_led_info = (spi_led_data*) kmalloc(sizeof (spi_led_data), GFP_KERNEL);
    if (!spi_led_info) {
        printk(KERN_INFO "Memory allocation for kernel data structure failed\n");
        return -1;
    }
    /*fills the device information necessary for creating the node under/dev by udev
    daemon process*/
    if (device_create(spi_class, NULL, spi_dev_num, NULL, DEVICE_NAME) == NULL) {
        printk(KERN_INFO "Error creating device\n");
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }
    spi_led_info->spi_name = "spi_led_strip";
    /*Connects the file operations with the character device file
    being created*/
    cdev_init(&spi_led_info->spi_cdev, &spi_led_fops);
    spi_led_info->spi_cdev.owner = THIS_MODULE;
    /*Links the major -minor number with the file so that kernel
    can decode via VFS if it is device file or not
    while data is being written to read from this file*/
    status = cdev_add(&spi_led_info->spi_cdev, spi_dev_num, 1);
    if (status < 0) {
        printk(KERN_INFO "Error adding the cdev\n");
        device_destroy(spi_class, spi_dev_num);
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }

    /*register the driver as spi driver and holds an entry in table
    to be searched while getting a match for spi device*/
    status = spi_register_driver(&spi_custom_driver);
    if (status < 0) {
        printk(KERN_INFO "Error registering spi driver\n");

        cdev_del(&spi_led_info->spi_cdev);
        device_destroy(spi_class, spi_dev_num);
        kfree(spi_led_info);
        class_destroy(spi_class);
        unregister_chrdev_region(spi_dev_num, 1);
        return -1;
    }
    printk(KERN_INFO "Successfull registration done\n");
    return 0;
}

/*Invoked when the driver is unloaded from kernel and carries out
all clean up tasks like freeing the resources, unlinking the registrations,
deleting the corresponding nodes under /dev */
static void spi_driver_exit(void) {
    printk(KERN_INFO "Inside %s function\n", __FUNCTION__);
    //    free_gpios();
    spi_unregister_driver(&spi_custom_driver);
    device_destroy(spi_class, spi_dev_num);
    cdev_del(&spi_led_info->spi_cdev);
    kfree(spi_led_info);
    class_destroy(spi_class);
    unregister_chrdev_region(spi_dev_num, 1);
}

module_init(spi_driver_init);
module_exit(spi_driver_exit);
MODULE_LICENSE("GPL");
