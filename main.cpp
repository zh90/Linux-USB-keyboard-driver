
#include <linux/kernel.h>      /*内核头文件，含有内核一些常用函数的原型定义*/
#include <linux/slab.h>           /*定义内存分配的一些函数*/
#include <linux/module.h>                   /*模块编译必须的头文件*/
#include <linux/input.h>               /*输入设备相关函数的头文件*/
#include <linux/init.h>                /*linux初始化模块函数定义*/
#include <linux/usb.h>               /*USB设备相关函数定义*/
#include <linux/kbd_ll.h>

#define DRIVER_VERSION ""
#define DRIVER_AUTHOR " TGE HOTKEY "
#define DRIVER_DESC "USB HID Tge hotkey driver"
#define USB_HOTKEY_VENDOR_ID 0x07e4
#define USB_HOTKEY_PRODUCT_ID 0x9473

static unsigned char usb_kbd_keycode[256] = {        /*使用第一套键盘扫描码表:A-1E;B-30;C-2E…*/

    0, 0, 0, 0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,

    50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 2, 3,

    4, 5, 6, 7, 8, 9, 10, 11, 28, 1, 14, 15, 57, 12, 13, 26,

    27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,

    65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,

    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,

    72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,

    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,

    115,114, 0, 0, 0,121, 0, 89, 93,124, 92, 94, 95, 0, 0, 0,

    122,123, 90, 91, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,

    150,158,159,128,136,177,178,176,142,152,173,140
}; 

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );

struct usb_kbd {
	struct input_dev dev;
	struct usb_device *usbdev;
	unsigned char new[8];
	unsigned char old[8];
	struct urb irq, led;
	struct usb_ctrlrequest dr;
	unsigned char leds, newleds;
	char name[128];
	int open;
};
/*static struct usb_device_id usb_kbd_id_table [] = {

	{ USB_INTERFACE_INFO(3, 1, 1) },//3,1,1分别表示接口类,接口子类,接口协议;3,1,1为键盘接口类;鼠标为3,1,2

	{ }           // Terminating entry 


};*/
MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);/*指定设备ID表*/ 
/*static void usb_kbd_irq(struct urb *urb)           //中断请求处理函数，有中断请求到达时调用该函数
{
	struct usb_kbd *kbd = urb->context;
	int *new;
	new = (int *) kbd->new;
	if(kbd->new[0] == (char)0x01){

	if(((kbd->new[1]>>4)&0x0f)!=0x7){

	handle_scancode(0xe0,1);
	handle_scancode(0x4b,1);
	handle_scancode(0xe0,0);
	handle_scancode(0x4b,0);
}

	else
	{ handle_scancode(0xe0,1);
	handle_scancode(0x4d,1);
	handle_scancode(0xe0,0);
	handle_scancode(0x4d,0);
}
}*/
/*中断请求处理函数，有中断请求到达时调用该函数*/
static void usb_kbd_irq(struct urb *urb, struct pt_regs *regs)
{
    struct usb_kbd *kbd = urb->context;

    int i;

      switch (urb->status) {

    case 0:       /* success */

        break;

    case -ECONNRESET: /* unlink */

    case -ENOENT:

    case -ESHUTDOWN:

        return;

    /* -EPIPE: should clear the halt */

    default:   /* error */

    goto resubmit;

    }

    for (i = 0; i < 8; i++)/*8次的值依次是:29-42-56-125-97-54-100-126*/

    {

    input_report_key(kbd->dev, usb_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);

    }

/*若同时只按下1个按键则在第[2]个字节,若同时有两个按键则第二个在第[3]字节，类推最多可有6个按键同时按下*/

    for (i = 2; i < 8; i++) {

    /*获取键盘离开的中断*/

    if (kbd->old > 3 && memscan(kbd->new + 2, kbd->old, 6) == kbd->new + 8) {/*同时没有该KEY的按下状态*/|

        if (usb_kbd_keycode[kbd->old])J

        {

        input_report_key(kbd->dev, usb_kbd_keycode[kbd->old], 0);

        }

        else

          info("Unknown key (scancode %#x) released.", kbd->old);

    }


    /*获取键盘按下的中断*/

    if (kbd->new > 3 && memscan(kbd->old + 2, kbd->new, 6) == kbd->old + 8) {/*同时没有该KEY的离开状态*/

        if (usb_kbd_keycode[kbd->new])

        {

          input_report_key(kbd->dev, usb_kbd_keycode[kbd->new], 1);

        }

        else

          info("Unknown key (scancode %#x) pressed.", kbd->new);

    }

    }

    /*同步设备,告知事件的接收者驱动已经发出了一个完整的报告*/ 

    input_sync(kbd->dev);

    memcpy(kbd->old, kbd->new, 8);/*防止未松开时被当成新的按键处理*/

resubmit:

    i = usb_submit_urb (urb, GFP_ATOMIC);/*发送USB请求块*/

    if (i)

    err ("can't resubmit intr, %s-%s/input0, status %d",

        kbd->usbdev->bus->bus_name,

        kbd->usbdev->devpath, i);_
} 
/*static void usb_kbd_irq(struct urb *urb, struct pt_regs *regs)

{

struct usb_kbd *kbd = urb->context;

int i;
switch (urb->status) {

	case 0:       //success 

	break;

	case -ECONNRESET: // unlink 

	case -ENOENT:

	case -ESHUTDOWN:

	return;

	default:   // error 

	goto resubmit;

    }
}*/
struct usb_kbd {                                 //  定义USB键盘结构体： 

    struct input_dev *dev; /*定义一个输入设备*/
    struct usb_device *usbdev;/*定义一个usb设备*/
    struct urb *irq/*usb键盘之中断请求块*/
    struct usb_ctrlrequest *cr;/*控制请求结构*

    unsigned char old[8]; /*按键离开时所用之数据缓冲区*/

    unsigned char newleds;/*目标指定灯状态*/

    char name[128];/*存放厂商名字及产品名字*/

    char phys[64];/*设备之节点*/

    unsigned char *new;/*按键按下时所用之数据缓冲区*/

    unsigned char *leds;/*当前指示灯状态*/

    dma_addr_t cr_dma; /*控制请求DMA缓冲地址*/

    dma_addr_t new_dma; /*中断urb会使用该DMA缓冲区*/

    dma_addr_t leds_dma; /*指示灯DAM缓冲地址*/

}; 

static void *usb_kbd_probe(struct usb_device *dev, unsigned int ifnum, const struct


usb_device_id *id)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_kbd *kbd;
	int pipe, maxp;
	iface = &dev->actconfig->interface[ifnum];
	interface = &iface->altsetting[iface->act_altsetting];

	if ((dev->descriptor.idVendor != USB_HOTKEY_VENDOR_ID) ||
	(dev->descriptor.idProduct != USB_HOTKEY_PRODUCT_ID) || (ifnum != 1))
	{
	return NULL;
}

if (dev->actconfig->bNumInterfaces != 2)
	{
	return NULL;
	}
if (interface->bNumEndpoints != 1) return NULL;
endpoint = interface->endpoint + 0;
pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
usb_set_protocol(dev, interface->bInterfaceNumber, 0);
usb_set_idle(dev, interface->bInterfaceNumber, 0, 0);
printk(KERN_INFO "GUO: Vid = %.4x, Pid = %.4x, Device = %.2x, ifnum = %.2x,
bufCount = %.8x\\n",
dev->descriptor.idVendor,dev->descriptor.idProduct,dev->descriptor.bcdDevice, ifnum,
maxp);
if (!(kbd = kmalloc(sizeof(struct usb_kbd), GFP_KERNEL))) return NULL;
memset(kbd, 0, sizeof(struct usb_kbd));
kbd->usbdev = dev;
FILL_INT_URB(&kbd->irq, dev, pipe, kbd->new, maxp > 8 ? 8 : maxp,
usb_kbd_irq,kbd, endpoint->bInterval); kbd->irq.dev = kbd->usbdev;
if (dev->descriptor.iManufacturer) usb_string(dev, dev->descriptor.iManufacturer,
kbd->name, 63);
if (usb_submit_urb(&kbd->irq)) {
	kfree(kbd); return NULL; }
	printk(KERN_INFO "input%d: %s on usb%d:%d.%d\\n", kbd->dev.number,
	kbd->name, dev->bus->busnum, dev->devnum, ifnum);
	return kbd; }
static void usb_kbd_disconnect(struct usb_device *dev, void *ptr)
{
	struct usb_kbd *kbd = ptr;
	usb_unlink_urb(&kbd->irq);
	kfree(kbd);
}
static struct usb_device_id usb_kbd_id_table [] = {
	{ USB_DEVICE(USB_HOTKEY_VENDOR_ID, USB_HOTKEY_PRODUCT_ID) },
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);
static struct usb_driver usb_kbd_driver = {               /*USB键盘驱动结构体*/
	name: "Topkey",                                      /*驱动名字*/
	probe: usb_kbd_probe,                            /*驱动探测函数,加载时用到*/
	disconnect: usb_kbd_disconnect,                   /*驱动断开函数,在卸载时用到*/
	id_table: usb_kbd_id_table,                      /*驱动设备ID表,用来指定设备或接口*/
	NULL,
};
static int __init usb_kbd_init(void)                   /*驱动程序生命周期的开始点，向 USB core 注册这个键盘驱动程序。*/
{
	int result = usb_register(&usb_kbd_driver);/*注册USB键盘驱动*/
	if (result == 0) /*注册失败*/
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return result;
	//usb_register(&usb_kbd_driver);
	//info(DRIVER_VERSION ":" DRIVER_DESC);
	//return 0;
}
static void __exit usb_kbd_exit(void)                  /* 驱动程序生命周期的结束点，向 USB core 注销这个键盘驱动程序。 */
{
	usb_deregister(&usb_kbd_driver);              /*注销USB键盘驱动*/
}
module_init(usb_kbd_init);                               /* 指定模块初始化函数(被指定的函数在insmod驱动时调用)*/
module_exit(usb_kbd_exit);                           /* 指定模块退出函数(被指定的函数在rmmod驱动时调用)：, a/ ^; m; k. ]+ W/ c- I) _! o*/
