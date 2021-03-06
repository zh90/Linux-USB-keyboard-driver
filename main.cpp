/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
*/


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

static unsigned char usb_kbd_keycode[256] = {        /*第一套键盘扫描码表:A-1E-30;B-30-48;C-2E-46*/


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
    //goto resubmit;

    /*  should clear the halt */

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
//	/*获取键盘按下的中断*/
//		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) {/*同时没有该KEY的离开状态*/
//			if (usb_kbd_keycode[kbd->new[i]])
//			{
//				input_report_key(kbd->dev, usb_kbd_keycode[kbd->new[i]], 1);
//			}
//			else
//				info("Unknown key (scancode %#x) pressed.", kbd->new[i]);
//		}
//	}


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

//编写事件处理函数：

/*事件处理函数*/

static int usb_kbd_event(struct input_dev *dev, unsigned int type,unsigned int code, int value)

{

    struct usb_kbd *kbd = dev->private;

    if (type != EV_LED) /*不支持LED事件 */

    return -1;

    /*获取指示灯的目标状态*/

    kbd->newleds = (!!test_bit(LED_KANA,   dev->led) << 3) | (!!test_bit(LED_COMPOSE, dev->led) << 3) |

   (!!test_bit(LED_SCROLLL, dev->led) << 2) | (!!test_bit(LED_CAPSL,   dev->led) << 1) |

   (!!test_bit(LED_NUML,   dev->led));

    if (kbd->led->status == -EINPROGRESS)

    return 0;

    /*指示灯状态已经是目标状态则不需要再做任何操作*/

    if (*(kbd->leds) == kbd->newleds)

    return 0;

    *(kbd->leds) = kbd->newleds;

    kbd->led->dev = kbd->usbdev;

    /*发送usb请求块*/
    if (usb_submit_urb(kbd->led, GFP_ATOMIC))

    err("usb_submit_urb(leds) failed");

    return 0;

}

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

 //编写LED事件处理函数：
/*接在event之后操作，该功能其实usb_kbd_event中已经有了，该函数的作用可能是防止event的操作失败，一般注释掉该函数中的所有行都可以正常工作*/

static void usb_kbd_led(struct urb *urb, struct pt_regs *regs)

{

    struct usb_kbd *kbd = urb->context;

    if (urb->status)

    warn("led urb status %d received", urb->status);

    if (*(kbd->leds) == kbd->newleds)/*指示灯状态已经是目标状态则不需要再做任何操作*/

    return;

    *(kbd->leds) = kbd->newleds;

    kbd->led->dev = kbd->usbdev;

    if (usb_submit_urb(kbd->led, GFP_ATOMIC))

    err("usb_submit_urb(leds) failed");

}

 //编写USB设备打开函数:

/*打开键盘设备时，开始提交在 probe 函数中构建的 urb，进入 urb 周期。 */

static int usb_kbd_open(struct input_dev *dev)

{

    struct usb_kbd *kbd = dev->private;

    kbd->irq->dev = kbd->usbdev;

    if (usb_submit_urb(kbd->irq, GFP_KERNEL))

      return -EIO;

    return 0;

}

// 编写USB设备关闭函数
/*关闭键盘设备时，结束 urb 生命周期。 */

static void usb_kbd_close(struct input_dev *dev)
{
    struct usb_kbd *kbd = dev->private;

    usb_kill_urb(kbd->irq); /*取消kbd->irq这个usb请求块*/
}

//创建URB

/*分配URB内存空间即创建URB*/

static int usb_kbd_alloc_mem(struct usb_device *dev, struct usb_kbd *kbd)
{

    if (!(kbd->irq = usb_alloc_urb(0, GFP_KERNEL)))

    return -1;

    if (!(kbd->led = usb_alloc_urb(0, GFP_KERNEL)))

    return -1;

    if (!(kbd->new = usb_buffer_alloc(dev, 8, GFP_ATOMIC, &kbd->new_dma)))

    return -1;

    if (!(kbd->cr = usb_buffer_alloc(dev, sizeof(struct usb_ctrlrequest), GFP_ATOMIC, &kbd->cr_dma)))

    return -1;

    if (!(kbd->leds = usb_buffer_alloc(dev, 1, GFP_ATOMIC, &kbd->leds_dma)))

    return -1;

    return 0;
}
// 销毁URB

/*释放URB内存空间即销毁URB*/

static void usb_kbd_free_mem(struct usb_device *dev, struct usb_kbd *kbd)
{

    if (kbd->irq)

    usb_free_urb(kbd->irq);

    if (kbd->led)

    usb_free_urb(kbd->led);

    if (kbd->new)

    usb_buffer_free(dev, 8, kbd->new, kbd->new_dma);

    if (kbd->cr)

    usb_buffer_free(dev, sizeof(struct usb_ctrlrequest), kbd->cr, kbd->cr_dma);

    if (kbd->leds)

    usb_buffer_free(dev, 1, kbd->leds, kbd->leds_dma);

}
/*static void *usb_kbd_probe(struct usb_device *dev, unsigned int ifnum, const structusb_device_id *id)
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
}*/
/*USB键盘驱动探测函数，初始化设备并指定一些处理函数的地址*/

static int usb_kbd_probe(struct usb_interface *iface,const struct usb_device_id *id)

{

    struct usb_device *dev = interface_to_usbdev(iface);

    struct usb_host_interface *interface;

    struct usb_endpoint_descriptor *endpoint;

    struct usb_kbd *kbd;

    struct input_dev *input_dev;

    int i, pipe, maxp;

    /*当前选择的interface*/

    interface = iface->cur_altsetting;

    /*键盘只有一个中断IN端点*/

    if (interface->desc.bNumEndpoints != 1)

    return -ENODEV;

    /*获取端点描述符*/

    endpoint = &interface->endpoint[0].desc;

    if (!(endpoint->bEndpointAddress & USB_DIR_IN))

      return -ENODEV;

    if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_INT)

      return -ENODEV;

    /*将endpoint设置为中断IN端点*/

    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);

    /*获取包的最大值*/

    maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

    kbd = kzalloc(sizeof(struct usb_kbd), GFP_KERNEL);

    input_dev = input_allocate_device();

    if (!kbd || !input_dev)

      goto fail1;

    if (usb_kbd_alloc_mem(dev, kbd))

      goto fail2;

    /* 填充 usb 设备结构体和输入设备结构体 */

    kbd->usbdev = dev;

    kbd->dev = input_dev;


    /*以"厂商名字 产品名字"的格式将其写入kbd->name*/

    if (dev->manufacturer)

      strlcpy(kbd->name, dev->manufacturer, sizeof(kbd->name));

    if (dev->product) {

    if (dev->manufacturer)

      strlcat(kbd->name, " ", sizeof(kbd->name));

      strlcat(kbd->name, dev->product, sizeof(kbd->name));

    }

    /*检测不到厂商名字*/

    if (!strlen(kbd->name))

    snprintf(kbd->name, sizeof(kbd->name),

        "USB HIDBP Keyboard %04x:%04x",

        le16_to_cpu(dev->descriptor.idVendor),

        le16_to_cpu(dev->descriptor.idProduct));

    struct input_dev *dev; /*定义一个输入设备*/

    struct usb_device *usbdev;/*定义一个usb设备*/

    unsigned char old[8]; /*按键离开时所用之数据缓冲区*/

    struct urb *irq/*usb键盘之中断请求块*/, *led/*usb键盘之指示灯请求块*/;


    unsigned char newleds;/*目标指定灯状态*/

    char name[128];/*存放厂商名字及产品名字*/


    char phys[64];/*设备之节点*/


    unsigned char *new;/*按键按下时所用之数据缓冲区*/
    /*设备链接地址*/

    usb_make_path(dev, kbd->phys, sizeof(kbd->phys));

    strlcpy(kbd->phys, "/input0", sizeof(kbd->phys));

    input_dev->name = kbd->name;


    input_dev->phys = kbd->phys;

//* input_dev 中的 input_id 结构体，用来存储厂商、设备类型和设备的编号，这个函数是将设备描述符 * 中的编号赋给内嵌的输入子系统结构体 usb_to_input_id(dev, &input_dev->id);

    /* cdev 是设备所属类别（class device） */

    input_dev->cdev.dev = &iface->dev;

/* input_dev 的 private 数据项用于表示当前输入设备的种类，这里将键盘结构体对象赋给它 */

    input_dev->private = kbd;

    input_dev->evbit[0] = BIT(EV_KEY)/*键码事件*/ | BIT(EV_LED)/*LED事件*/ | BIT(EV_REP)/*自动重覆数值*/;

    input_dev->ledbit[0] = BIT(LED_NUML)/*数字灯*/ | BIT(LED_CAPSL)/*大小写灯*/ | BIT(LED_SCROLLL)/*滚动灯*/ | BIT(LED_COMPOSE) | BIT(LED_KANA);

    for (i = 0; i < 255; i++)

    set_bit(usb_kbd_keycode, input_dev->keybit);

    clear_bit(0, input_dev->keybit);

    input_dev->event = usb_kbd_event;/*注册事件处理函数入口*/

    input_dev->open = usb_kbd_open;/*注册设备打开函数入口*/

    input_dev->close = usb_kbd_close;/*注册设备关闭函数入口*/

    /*初始化中断URB*/

    usb_fill_int_urb(kbd->irq/*初始化kbd->irq这个urb*/, dev/*这个urb要发送到dev这个设备*/, pipe/*这个urb要发送到pipe这个端点*/,

    kbd->new/*指向缓冲的指针*/, (maxp > 8 ? 8 : maxp)/*缓冲长度*/,

    usb_kbd_irq/*这个urb完成时调用的处理函数*/, kbd/*指向数据块的指针，被添加到这个urb结构可被完成处理函数获取*/, endpoint->bInterval/*urb应当被调度的间隔*/);

    kbd->irq->transfer_dma = kbd->new_dma; /*指定urb需要传输的DMA缓冲区*/

    kbd->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;/*本urb有一个DMA缓冲区需要传输*/

    kbd->cr->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;/*操作的是类接口对象*/

    kbd->cr->bRequest = 0x09; /*中断请求编号*/

    kbd->cr->wValue = cpu_to_le16(0x200);

    kbd->cr->wIndex = cpu_to_le16(interface->desc.bInterfaceNumber);/*接口号*/

    kbd->cr->wLength = cpu_to_le16(1);/*数据传输阶段传输多少个bytes*/

    /*初始化控制URB*/

    usb_fill_control_urb(kbd->led, dev, usb_sndctrlpipe(dev, 0),

    (void *) kbd->cr, kbd->leds, 1,

    usb_kbd_led, kbd);

    kbd->led->setup_dma = kbd->cr_dma;

    kbd->led->transfer_dma = kbd->leds_dma;

    kbd->led->transfer_flags |= (URB_NO_TRANSFER_DMA_MAP | URB_NO_SETUP_DMA_MAP/*如果使用DMA传输则urb中setup_dma指针所指向的缓冲区是DMA缓冲区而不是setup_packet所指向的缓冲区*/);

    /*注册输入设备*/

    input_register_device(kbd->dev);

    usb_set_intfdata(iface, kbd);/*设置接口私有数据*/

    return 0;

    fail2:   usb_kbd_free_mem(dev, kbd);

    fail1:   input_free_device(input_dev);

    kfree(kbd);

    return -ENOMEM;
}
/*断开连接(如键盘设备拔出)的处理函数*/

static void usb_kbd_disconnect(struct usb_interface *intf)
{

    struct usb_kbd *kbd = usb_get_intfdata (intf);/*获取接口的私有数据给kbd*/

    usb_set_intfdata(intf, NULL);/*设置接口的私有数据为NULL*/

    if (kbd) {

    usb_kill_urb(kbd->irq);/*取消中断请求*/

    input_unregister_device(kbd->dev);/*注销设备*/

    usb_kbd_free_mem(interface_to_usbdev(intf), kbd);/*释放内存空间*/

    kfree(kbd);

    }
}

if (dev->actconfig->bNumInterfaces != 2)
	{
	return NULL;
	}

//if (!(kbd = kmalloc(sizeof(struct usb_kbd), GFP_KERNEL))) return NULL;
//memset(kbd, 0, sizeof(struct usb_kbd));
//kbd->usbdev = dev;
//FILL_INT_URB(&kbd->irq, dev, pipe, kbd->new, maxp > 8 ? 8 : maxp,
//usb_kbd_irq,kbd, endpoint->bInterval); kbd->irq.dev = kbd->usbdev;
/*if (dev->descriptor.iManufacturer) usb_string(dev, dev->descriptor.iManufacturer,
   kbd->name, 63);
if (usb_submit_urb(&kbd->irq)) {
	kfree(kbd); return NULL; }
	printk(KERN_INFO "input%d: %s on usb%d:%d.%d\\n", kbd->dev.number,
	kbd->name, dev->bus->busnum, dev->devnum, ifnum);
	return kbd; }*/
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
    printk("Registering usb keyboard driver driver...\n");
	int result = usb_register(&usb_kbd_driver);/*注册USB键盘驱动*/
	if (result == 0) /*注册失败*/
     //   info(DRIVER_VERSION ":" DRIVER_DESC);
    printk("Registered successfully!\n");
	return result;
	//usb_register(&usb_kbd_driver);
	//info(DRIVER_VERSION ":" DRIVER_DESC);
	//return 0;
}
static void __exit usb_kbd_exit(void)                  /* 驱动程序生命周期的结束点，向 USB core 注销这个键盘驱动程序。 */
{
        printk("Deregistering usb keyboard driver...\n");
	usb_deregister(&usb_kbd_driver);              /*注销USB键盘驱动*/
	printk("Derigistered successfully!\n");
}
module_init(usb_kbd_init);                               /* 指定模块初始化函数(被指定的函数在insmod驱动时调用)*/
module_exit(usb_kbd_exit);                           /* 指定模块退出函数(被指定的函数在rmmod驱动时调用)：, a/ ^; m; k. ]+ W/ c- I) _! o*/


---------------------input 文件相关的重定义 ---------------------------------------------------------------------------------------------------

#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */
#define EVIOCGID		_IOR('E', 0x02, struct input_id)	/* get device ID */
#define EVIOCGKEYCODE		_IOR('E', 0x04, int[2])			/* get keycode */
#define EVIOCSKEYCODE		_IOW('E', 0x04, int[2])			/* set keycode */

#define EVIOCGNAME(len)		_IOC(_IOC_READ, 'E', 0x06, len)		/* get device name */
#define EVIOCGPHYS(len)		_IOC(_IOC_READ, 'E', 0x07, len)		/* get physical location */
#define EVIOCGUNIQ(len)		_IOC(_IOC_READ, 'E', 0x08, len)		/* get unique identifier */

#define EVIOCGKEY(len)		_IOC(_IOC_READ, 'E', 0x18, len)		/* get global keystate */
#define EVIOCGLED(len)		_IOC(_IOC_READ, 'E', 0x19, len)		/* get all LEDs */
#define EVIOCGSND(len)		_IOC(_IOC_READ, 'E', 0x1a, len)		/* get all sounds status */
#define EVIOCGSW(len)		_IOC(_IOC_READ, 'E', 0x1b, len)		/* get all switch states */

#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + ev, len)	/* get event bits */
#define EVIOCGABS(abs)		_IOR('E', 0x40 + abs, struct input_absinfo)		/* get abs value/limits */
#define EVIOCSABS(abs)		_IOW('E', 0xc0 + abs, struct input_absinfo)		/* set abs value/limits */

#define EVIOCSFF		_IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect))	/* send a force effect to a force feedback device */
#define EVIOCRMFF		_IOW('E', 0x81, int)			/* Erase a force effect */
#define EVIOCGEFFECTS		_IOR('E', 0x84, int)			/* Report number of effects playable at the same time */

#define EVIOCGRAB		_IOW('E', 0x90, int)			/* Grab/Release device */



/*
 * Structures used in ioctls to upload effects to a device
 * The first structures are not passed directly by using ioctls.
 * They are sub-structures of the actually sent structure (called ff_effect)
 */


struct ff_replay {
	__u16 length; /* Duration of an effect in ms. All other times are also expressed in ms */
	__u16 delay;  /* Time to wait before to start playing an effect */
};

struct ff_trigger {
	__u16 button;   /* Number of button triggering an effect */
	__u16 interval; /* Time to wait before an effect can be re-triggered (ms) */
};

struct ff_envelope {
	__u16 attack_length;	/* Duration of attack (ms) */
	__u16 attack_level;	/* Level at beginning of attack */
	__u16 fade_length;	/* Duration of fade (ms) */
	__u16 fade_level;	/* Level at end of fade */
};

/* FF_CONSTANT */
struct ff_constant_effect {
	__s16 level;	    /* Strength of effect. Negative values are OK */
	struct ff_envelope envelope;
};

/* FF_RAMP */
struct ff_ramp_effect {
	__s16 start_level;
	__s16 end_level;
	struct ff_envelope envelope;
};

/* FF_SPRING of FF_FRICTION */
struct ff_condition_effect {
	__u16 right_saturation; /* Max level when joystick is on the right */
	__u16 left_saturation;  /* Max level when joystick in on the left */

	__s16 right_coeff;	/* Indicates how fast the force grows when the
				   joystick moves to the right */
	__s16 left_coeff;	/* Same for left side */

	__u16 deadband;	/* Size of area where no force is produced */
	__s16 center;	/* Position of dead zone */

};

/* FF_PERIODIC */
struct ff_periodic_effect {
	__u16 waveform;	/* Kind of wave (sine, square...) */
	__u16 period;	/* in ms */
	__s16 magnitude;	/* Peak value */
	__s16 offset;	/* Mean value of wave (roughly) */
	__u16 phase;		/* 'Horizontal' shift */

	struct ff_envelope envelope;

/* Only used if waveform == FF_CUSTOM */
	__u32 custom_len;	/* Number of samples */
	__s16 *custom_data;	/* Buffer of samples */
/* Note: the data pointed by custom_data is copied by the driver. You can
 * therefore dispose of the memory after the upload/update */
};

/* FF_RUMBLE */
/* Some rumble pads have two motors of different weight.
   strong_magnitude represents the magnitude of the vibration generated
   by the heavy motor.
*/
struct ff_rumble_effect {
	__u16 strong_magnitude;  /* Magnitude of the heavy motor */
	__u16 weak_magnitude;    /* Magnitude of the light one */
};

/*
 * Structure sent through ioctl from the application to the driver
 */
struct ff_effect {
	__u16 type;
/* Following field denotes the unique id assigned to an effect.
 * If user sets if to -1, a new effect is created, and its id is returned in the same field
 * Else, the user sets it to the effect id it wants to update.
 */
	__s16 id;

	__u16 direction;	/* Direction. 0 deg -> 0x0000 (down)
					     90 deg -> 0x4000 (left)
					    180 deg -> 0x8000 (up)
					    270 deg -> 0xC000 (right)
				*/

	struct ff_trigger trigger;
	struct ff_replay replay;

	union {
		struct ff_constant_effect constant;
		struct ff_ramp_effect ramp;
		struct ff_periodic_effect periodic;
		struct ff_condition_effect condition[2]; /* One for each axis */
		struct ff_rumble_effect rumble;
	} u;
};
