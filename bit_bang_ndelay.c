/*
 * Refered code from the link provided by Professor in Slides:
 * https://gist.github.com/maggocnx/5946907

 * Below code uses ndelay() mechanism for a possible
 * implementation of bit-banging approach to drive WS2812-based LED strip
 */

#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/spi/spi.h>
#include<linux/module.h>
#include<linux/timer.h>
#include<linux/hrtimer.h>
#include<linux/ktime.h>
#include<linux/gpio.h>
#include<linux/pci.h>
#include<linux/delay.h>
#include<linux/spinlock.h>
#include<linux/kthread.h>
#define GIP_GPIO_BAR 1
#define DATA_SIZE 48
int global_counter = 0;

/*
Creating data for single pixel as per specification.
O bit Encoding - 350ns high and 800ns low voltage
1 bit Encoding - 700ns high and 600ns low voltage
*/
int const_led_data[DATA_SIZE] = {
    350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800,
    700, 600, 700, 600, 700, 600, 700, 600, 700, 600, 700, 600, 700, 600, 700, 600,
    350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800, 350, 800,
};


int *var_data;
int var_data_size;
/*task representing the kernel thread which
carries out the data transmission for bit banging*/
struct task_struct *light_led = NULL;

unsigned long start, len;
spinlock_t lock;
void __iomem *reg_base;
unsigned int read_value;

/*The kernel thread which achieves the bit bang*/
int ndelay_light(void *data) {
    /*runs until all data bits are transferred*/
    while ((!kthread_should_stop())&&(global_counter != var_data_size)) {
        /*achieving lock before reading or writing to
    memory mapped I/O memory*/
        spin_lock(&lock);
    /*reading the 32 bit data at the address in reg_base*/
        read_value = ioread32(reg_base);
        /*sets the bit at 4th position which represents data
    @ gpio 12 to high or low depending if global counter is
    even or odd*/
        if (global_counter % 2) {
            iowrite32(read_value & (~BIT(4)), reg_base);
        } else {
            iowrite32(read_value | (BIT(4)), reg_base);
        }
        spin_unlock(&lock);
        /*Sets the delay such that data is latched for a specific
    amount of time to glow the led strip as per its data bit encoding standard*/
        ndelay(var_data[global_counter++]);
    }
    light_led = NULL;
    return 0;
}

/*Init function used to initialize resources when the module
is loaded into kernel*/
static int ndelay_dev_init(void) {
    int ret = 0;
    int i;
    int n;
    /*Gets the pci device structure representing the gip controller
    which is attached to pci bus, which has vendor ID 8086 and device ID
    as 0934*/
    struct pci_dev *pdev = pci_get_device(0x8086, 0x0934, NULL);
    printk(KERN_INFO "Inside %s\n",__FUNCTION__);
    /*Sets the IO pin 1 as output pin so that the bit bang
    data can come out of this pin*/
    gpio_request(12, "ndelay");
    gpio_direction_output(12, 0);

    printk(KERN_INFO "Device name %s\n", pdev->driver->name);
    /*gets the start and length parameters register base of the
    corresponding pci device, so the virtual address can be formulated
    for the kernel usage*/
    start = pci_resource_start(pdev, GIP_GPIO_BAR);
    len = pci_resource_len(pdev, GIP_GPIO_BAR);
    /*virtual address is formed and stored in reg_base
    NOTE: No offset is added to get PORTA data register as
    it starts with offset 0X00*/
    reg_base = ioremap_nocache(start, len);
    printk(KERN_INFO "addr of base reg is %p\n", reg_base);
    /*Initialization of lock is done here*/
    spin_lock_init(&lock);
    /*Each data set is being sent for the 16 led*/
    for (n = 1; n <= 16; n++) {
        global_counter = 0;
        var_data = (int *) kmalloc(n * sizeof (int)*DATA_SIZE, GFP_KERNEL);
        /*copy data from constant data*/
        for (i = 0; i < n * DATA_SIZE; i++) {
            var_data[i] = const_led_data[i % DATA_SIZE];
        }
        var_data_size = n*DATA_SIZE;
        /*Initiates the kthread to run in order to transmit the data*/
        if(light_led == NULL){
            light_led = kthread_run(&ndelay_light, NULL, "light_up");
        }
        /*sleep for 1 second as callback might take at max 1 second to transfer
        encoded data bits, max 16*24*8 bits*/
        msleep(1000);
        /*clean up memory each time so that no junk bits will remain
        which can lead to display of junk colors*/
        kfree(var_data);
    }

    return ret;
}

/*Invoked when module is unloaded , which
stops the kernel thread and free the allocated gpio pins*/
static void ndelay_dev_exit(void) {
    printk(KERN_INFO "Inside %s\n",__FUNCTION__);
    if(light_led != NULL){
        kthread_stop(light_led);
        light_led = NULL;
    }
    printk("ndelay module uninstalling\n");
}

module_init(ndelay_dev_init);
module_exit(ndelay_dev_exit);
MODULE_LICENSE("GPL");
