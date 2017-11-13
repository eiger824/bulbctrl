#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#define DEVICE_NAME "bulbctrl"
#define CLASS_NAME  "bc-drv"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Santiago Pagola");
MODULE_DESCRIPTION("A button driver to control the current flow through a USB bulb");
MODULE_VERSION("0.1");

static unsigned int gpio_bulb         =   24;
static unsigned int gpio_button       =   23;
static unsigned int irq_number;
static unsigned int press_cnt         =   0;
static bool         led_state         =   0; 
static int          major_number;
static char         message[2]        =   {0};
static size_t       size_out;
static struct class*  bcclass   =  NULL;
static struct device* bcdev    =  NULL;

static irq_handler_t  irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

// File operations
static int      dev_open(struct inode *in, struct file *f);
static int      dev_release(struct inode *in, struct file *f);
static ssize_t  dev_write(struct file *f, const char* msg, size_t nb, loff_t *lf);
static ssize_t  dev_read(struct file *f, char* msg, size_t nb, loff_t *lf);

static struct file_operations fops = 
{
    .open = dev_open,
    .write = dev_write,
    .read = dev_read,
    .release = dev_release,
};

static int __init bulbctrl_init(void)
{
    int result = 0;
    printk(KERN_INFO "BULBCTRL: Initializing BULBCTRL\n");
    // Is the GPIO a valid GPIO number (e.g., the BBB has 4x32 but not all available)
    if (!gpio_is_valid(gpio_bulb)){
        printk(KERN_INFO "BULBCTRL: invalid BULB GPIO\n");
        return -ENODEV;
    }
    if (!gpio_is_valid(gpio_button)){
        printk(KERN_INFO "BULBCTRL: invalid BUTTON GPIO\n");
        return -ENODEV;
    }
    // Start with bulb off
    led_state = false;
    gpio_request(gpio_bulb, "sysfs");
    gpio_direction_output(gpio_bulb, led_state);
    gpio_export(gpio_bulb, false);
    gpio_request(gpio_button, "sysfs");
    gpio_direction_input(gpio_button);
    gpio_set_debounce(gpio_button, 200);
    gpio_export(gpio_button, false);

    printk(KERN_INFO "BULBCTRL: The button state is currently: %d\n", gpio_get_value(gpio_button));

    irq_number = gpio_to_irq(gpio_button);
    printk(KERN_INFO "BULBCTRL: The button is mapped to IRQ: %d\n", irq_number);

    result = request_irq(irq_number,
            (irq_handler_t) irq_handler,
            IRQF_TRIGGER_RISING,
            "ebb_gpio_handler",
            NULL);

    printk(KERN_INFO "BULBCTRL: The interrupt request result : %d\n", result);

    // Register character device and class
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        printk(KERN_ALERT "BULBCTRL failed to register major number\n");
        return major_number;
    }
    printk(KERN_INFO "BULBCTRL: registered major number %d\n", major_number);

    bcclass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(bcclass))
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(bcclass);
    }

    printk(KERN_INFO "BULBCTRL: device class successfully registered\n");

    bcdev = device_create(bcclass, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bcdev))
    {
        class_destroy(bcclass);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create device\n");
        return PTR_ERR(bcdev);
    }

    // return the result of the interrupt register 
    return result;
}

static void __exit bulbctrl_exit(void)
{
    printk(KERN_INFO "BULBCTRL: Button state is currently: %d (%d times pressed)\n"
            , gpio_get_value(gpio_button), press_cnt);
    gpio_set_value(gpio_bulb, 0);
    gpio_unexport(gpio_bulb);
    free_irq(irq_number, NULL);
    gpio_unexport(gpio_button);
    gpio_free(gpio_bulb);
    gpio_free(gpio_button);
    // Unregister device
    device_destroy(bcclass, MKDEV(major_number, 0));
    class_unregister(bcclass);
    class_destroy(bcclass);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "BULBCTRL: Bye bye!\n");
}

static int dev_open(struct inode* in, struct file* f)
{
    printk(KERN_INFO "BULBCTRL: Device opened\n");
    return 0;
}

static int dev_release(struct inode* in, struct file* f)
{
    printk(KERN_INFO "BULBCTRL: Device closed\n");
    return 0;
}

static ssize_t dev_write(struct file* f, const char* msg, size_t nb, loff_t *lf)
{
    printk(KERN_INFO "BULBCTRL: Received buffer size: %zu\n", nb);
    int err;
    if (nb != 1)
    {
        printk(KERN_INFO "Wrong size received (should be 1)\n");
        return -nb;
    }
    err = copy_from_user(message, msg, nb);
    message[nb] = 0;

    // Check command: turn on/off
    if (message[0] == '0')
    {
        //OFF: check current state
        if (led_state)
        {
            led_state = !led_state;
            gpio_set_value(gpio_bulb, led_state);
        }
    }
    else
    {
        if (!led_state)        
        {
            led_state = !led_state;
            gpio_set_value(gpio_bulb, led_state);
        }
    }
    return nb;
}

static ssize_t dev_read(struct file *f, char* msg, size_t nb, loff_t *lf)
{
   int err;
   sprintf(message, "%d", (int)led_state); 
   message[1] = 0;
   printk(KERN_INFO "BULBCTRL: Requesting bulb state: Returning [%s]\n", message);
   size_out = 1; // Always
   err = copy_to_user(msg, message, size_out);

   if (err == 0)
   {
       printk(KERN_INFO "BULBCTRL: Sent %zu bytes to user-space\n", size_out);
       return size_out;
   }
   else
   {
       printk(KERN_INFO "BULBCTRL: Failed to send %zu bytes to user-space\n", err);
       return -EFAULT;
   }

}
static irq_handler_t irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    led_state = !led_state;
    gpio_set_value(gpio_bulb, led_state);
    printk(KERN_INFO "BULBCTRL: Interrupt! (button state is %d)\n", gpio_get_value(gpio_button));
    press_cnt++;
    return (irq_handler_t) IRQ_HANDLED;
}

module_init(bulbctrl_init);
module_exit(bulbctrl_exit);
