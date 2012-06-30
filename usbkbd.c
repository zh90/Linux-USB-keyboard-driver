#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "2.0"
#define DRIVER_AUTHOR "HITCS-39"
#define DRIVER_DESC "USB HID keyboard driver"
#define USB_KEYBOARD_VENDOR_ID 0x1c4f
#define USB_KEYBOARD_PRODUCT_ID 0x0002

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static const unsigned char usb_kbd_keycode[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	 65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};

struct usb_kbd {   //  定义USB键盘结构体：
	struct input_dev *dev;   //一个输入设备
	struct usb_device *usbdev;  //定义一个usb设备
	unsigned char old[8];  //前一次处理urb请求时按键状态的缓冲区
	struct urb *irq, *led;  //按键和控制的中断请求块urb
	unsigned char newleds; //键盘指定灯状态
	char name[128];  //厂商名字和产品名字
	char phys[64]; //设备节点

	unsigned char *new;  //按键按下时处理urb请求时当前按键状态的缓冲区
	struct usb_ctrlrequest *cr;  //控制请求结构
	unsigned char *leds;  //当前指示灯状态
	dma_addr_t cr_dma;  //控制请求URB的DMA缓冲地址
	dma_addr_t new_dma;  //中断urb会使用该DMA缓冲区
	dma_addr_t leds_dma; //指示灯DMA缓冲地址
};

//输入中断事件处理函数

static void usb_kbd_irq(struct urb *urb)
{
	printk("Starting irq\n");
	struct usb_kbd *kbd = urb->context;
	int i;

	switch (urb->status) {  //判断URB的状态
	case 0:			//URB被成功接收
		break;
	case -ECONNRESET:	//断开连接错误,urb未终止就返回给了回调函数
	case -ENOENT: //urb被kill了,生命周期彻底被终止
	case -ESHUTDOWN:  //USB主控制器驱动程序发生了严重的错误,或者提交完的一瞬间设备被拔出
		return;
	
	default:		//其它错误,均可以重新提交urb
		goto resubmit;
	}
	printk("irq1\n");
	for (i = 0; i < 8; i++)
		input_report_key(kbd->dev, usb_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);/*usb_kbd_keycode[224]-usb_kbd_keycode[231],8次的值依次是:29-42-56-125-97-54-100-126,判断这8个键的状态*/
	printk("irq2\n");
	//若同时只按下2个按键则另一个键在第[2]个字节,若同时有两个按键则第二个在第[3]字节，类推最多可有6个按键同时按下
	for (i = 2; i < 8; i++) {

		if (kbd->old[i] > 3 && memscan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8) {  //判断那些键的状态改变了,即由按下变为了松开
			if (usb_kbd_keycode[kbd->old[i]])  //是键盘所用的按键,就报告按键离开
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->old[i]], 0);
			else                   //不是键盘所用的按键
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->old[i]);
		}

		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) {  //判断那些键的状态改变了,即由松开变为了按下
			if (usb_kbd_keycode[kbd->new[i]])  //是键盘所用的按键,就报告按键被按下
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->new[i]], 1);
			else        //不是键盘所用的按键
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->new[i]);
		}
	}

	input_sync(kbd->dev);  //同步设备,告知事件的接收者驱动已经发出了一个完整的input子系统的报告
	
        printk("irq2.1\n");
	memcpy(kbd->old, kbd->new, 8);  //将本次的按键状态拷贝到kbd->old,用作下次urb处理时判断按键状态的改变
        printk("irq3\n");
resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC); //重新发送urb请求块
	if (i)  //发送urb请求块失败
	{
		printk("irq4\n");
		err_hid ("can't resubmit intr, %s-%s/input0, status %d",
				kbd->usbdev->bus->bus_name,
				kbd->usbdev->devpath, i);
		printk("irq5\n");
	}
	printk("irq6\n");
}

static int usb_kbd_event(struct input_dev *dev, unsigned int type,
			 unsigned int code, int value)
{
	printk("Starting event.\n");
	struct usb_kbd *kbd = input_get_drvdata(dev);

	if (type != EV_LED)
		return -1;

	kbd->newleds = (!!test_bit(LED_KANA,    dev->led) << 3) | (!!test_bit(LED_COMPOSE, dev->led) << 3) |
		       (!!test_bit(LED_SCROLLL, dev->led) << 2) | (!!test_bit(LED_CAPSL,   dev->led) << 1) |
		       (!!test_bit(LED_NUML,    dev->led));

	if (kbd->led->status == -EINPROGRESS)
		return 0;

	if (*(kbd->leds) == kbd->newleds)
		return 0;

	*(kbd->leds) = kbd->newleds;
	kbd->led->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->led, GFP_ATOMIC))
		err_hid("usb_submit_urb(leds) failed");

	return 0;
}

static void usb_kbd_led(struct urb *urb)
{
	printk("Starting led\n");
	struct usb_kbd *kbd = urb->context;

	if (urb->status)
		dev_warn(&urb->dev->dev, "led urb status %d received\n",
			 urb->status);

	if (*(kbd->leds) == kbd->newleds)
		return;

	*(kbd->leds) = kbd->newleds;
	kbd->led->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->led, GFP_ATOMIC))
		err_hid("usb_submit_urb(leds) failed");
}


 //编写USB设备打开函数:打开键盘设备时，开始提交在 probe 函数中构建的 urb，进入 urb 周期。 
static int usb_kbd_open(struct input_dev *dev)
{
	printk("Opening\n");
	struct usb_kbd *kbd = input_get_drvdata(dev);
	printk("open1\n\n");
	kbd->irq->dev = kbd->usbdev;
	if (usb_submit_urb(kbd->irq, GFP_KERNEL))
		return -EIO;
	printk("open2\n");
	return 0;
}

// 编写USB设备关闭函数：关闭键盘设备时，结束 urb 生命周期。 
static void usb_kbd_close(struct input_dev *dev)
{
	struct usb_kbd *kbd = input_get_drvdata(dev);

	usb_kill_urb(kbd->irq);
}


//创建URB：分配URB内存空间即创建URB
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


// 销毁URB：释放URB内存空间即销毁URB
static void usb_kbd_free_mem(struct usb_device *dev, struct usb_kbd *kbd)
{
	usb_free_urb(kbd->irq);
	usb_free_urb(kbd->led);
	usb_buffer_free(dev, 8, kbd->new, kbd->new_dma);
	usb_buffer_free(dev, sizeof(struct usb_ctrlrequest), kbd->cr, kbd->cr_dma);
	usb_buffer_free(dev, 1, kbd->leds, kbd->leds_dma);
}

static int usb_kbd_probe(struct usb_interface *iface,
			 const struct usb_device_id *id)  //usb_interface *iface:由内核自动获取的接口,一个接口对应一种功能, struct usb_device_id *id:设备的标识符
{
	printk("Starting probe\n");
	struct usb_device *dev = interface_to_usbdev(iface);  //获取usb接口结构体中的usb设备结构体,每个USB设备对应一个struct usb_device的变量，由usb core负责申请和赋值
	struct usb_host_interface *interface;  //连接到的接口的描述
	struct usb_endpoint_descriptor *endpoint;    //传输数据管道的端点
	struct usb_kbd *kbd;  //usb设备在用户空间的描述
	struct input_dev *input_dev;  //表示输入设备
	int i, pipe, maxp; 
	int error = -ENOMEM; 
	
	
	interface = iface->cur_altsetting; //将连接到的接口的描述设置为当前的setting
	printk("1\n");
	if (interface->desc.bNumEndpoints != 1) //判断中断IN端点的个数,键盘只有一个端点,如果不为1,则出错,desc是设备描述符
		return -ENODEV;

	endpoint = &interface->endpoint[0].desc; //取得键盘中断IN端点的描述符,endpoint[0]表示中断端点
	printk("2\n");
	if (!usb_endpoint_is_int_in(endpoint)) //查看所获得的端点是否为中断IN端点
		return -ENODEV;
  printk("3\n");
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress); //得到驱动程序的中断OUT端点号,创建管道，用于连接驱动程序缓冲区和设备端口
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe)); //得到最大可以传输的数据包(字节)

	kbd = kzalloc(sizeof(struct usb_kbd), GFP_KERNEL); //为kbd结构体分配内存,GFP_KERNEL是内核内存分配时最常用的标志位，无内存可用时可引起休眠
	input_dev = input_allocate_device();   //为输入设备的结构体分配内存,并初始化它
	printk("inputdev-1:%s\n",input_dev->name);
	printk("4\n");
	if (!kbd || !input_dev) //给kbd或input_dev分配内存失败
		goto fail1;
        printk("5\n");
	if (usb_kbd_alloc_mem(dev, kbd)) //分配urb内存空间失败,即创建urb失败
		goto fail2;
	printk("6\n");
	kbd->usbdev = dev;    //给kbd的usb设备结构体usbdev赋值
	kbd->dev = input_dev; //给kbd的输入设备结构体dev赋值,将所有内容统一用kbd封装,input子系统只能处理input_dev类型的对象
	printk("7\n");
	if (dev->manufacturer) //将厂商名,产品名赋值给kbd的name成员	
	{
		printk("7.1\n");
		strlcpy(kbd->name, dev->manufacturer, sizeof(kbd->name));
	}
	printk("8\n");
	
	if (dev->product) {
		printk("7.2\n");
		if (dev->manufacturer) //有厂商名,就在产品名之前加入空格
			strlcat(kbd->name, " ", sizeof(kbd->name));
		strlcat(kbd->name, dev->product, sizeof(kbd->name));
	}

	printk("9\n");

  printk("10\n");
	usb_make_path(dev, kbd->phys, sizeof(kbd->phys)); //分配设备的物理路径的地址，设备链接地址，不随设备的拔出而改变
	printk("10.1\n");
	strlcpy(kbd->phys, "/input0", sizeof(kbd->phys));
	printk("10.2\n");
	input_dev->name = kbd->name; //给input_dev的name赋值
	printk("inputdevname:%s\n",input_dev->name);
	printk("kbddevname:%s\n",kbd->dev->name);
	input_dev->phys = kbd->phys;  //设备链接地址
	usb_to_input_id(dev, &input_dev->id); //给输入设备结构体input->的标识符结构赋值,主要设置bustype、vendo、product等
	printk("10.3\n");
	//input_dev->dev.parent = &iface->dev;

	//input_set_drvdata(input_dev, kbd);
	printk("10.4\n");
	input_dev->evbit[0] = BIT_MASK(EV_KEY) /*键码事件*/| BIT_MASK(EV_LED) | /*LED事件*/
		BIT_MASK(EV_REP)/*自动重覆数值*/;  //支持的事件类型
	printk("10.5\n");
	input_dev->ledbit[0] = BIT_MASK(LED_NUML) /*数字灯*/| BIT_MASK(LED_CAPSL) |/*大小写灯*/
		BIT_MASK(LED_SCROLLL)/*滚动灯*/ ;   //EV_LED事件支持的事件码
	printk("10.6\n");

	for (i = 0; i < 255; i++)
		set_bit(usb_kbd_keycode[i], input_dev->keybit); // 初始化,每个键盘扫描码都可以出发键盘事件
	printk("10.7\n");
	clear_bit(0, input_dev->keybit); //为0的键盘扫描码不能触发键盘事件
	printk("10.8\n");
	input_dev->event = usb_kbd_event; //设置input设备的打开、关闭、写入数据时的处理方法
	input_dev->open = usb_kbd_open;
	input_dev->close = usb_kbd_close;
  //初始化中断URB
	usb_fill_int_urb(kbd->irq/*初始化kbd->irq这个urb*/, dev/*这个urb要发送到dev这个设备*/, pipe/*这个urb要发送到pipe这个端点*/,
			 kbd->new/*指向缓冲的指针*/, (maxp > 8 ? 8 : maxp)/*缓冲区长度(不超过8)*/,
			 usb_kbd_irq/*这个urb完成时调用的处理函数*/, kbd/*指向数据块的指针，被添加到这个urb结构可被完成处理函数获取*/, endpoint->bInterval/*urb应当被调度的间隔*/);
	printk("10.9\n");
	kbd->irq->transfer_dma = kbd->new_dma;  //指定urb需要传输的DMA缓冲区
	kbd->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;  //本urb有一个DMA缓冲区需要传输，用DMA传输要设的标志

	kbd->cr->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;  //操作的是USB类接口对象
	kbd->cr->bRequest = 0x09;  //中断请求编号
	kbd->cr->wValue = cpu_to_le16(0x200); //大端、小端模式转换
	kbd->cr->wIndex = cpu_to_le16(interface->desc.bInterfaceNumber); //接口号
	kbd->cr->wLength = cpu_to_le16(1);  //一次数据传输要传的字节数
  //初始化控制URB
	printk("10.10\n");
	usb_fill_control_urb(kbd->led/*初始化kbd->led这个urb*/, dev/*这个urb要由dev这个设备发出*/, usb_sndctrlpipe(dev, 0)/*urb发送到的端点*/,
			     (void *) kbd->cr/*发送的setup packet*/, kbd->leds/*待发送数据的缓冲区*/, 1/*发送数据长度*/,
			     usb_kbd_led/*这个urb完成时调用的处理函数*/, kbd/*指向数据块的指针，被添加到这个urb结构可被完成处理函数获取*/);
	kbd->led->setup_dma = kbd->cr_dma;  //指定urb需要传输的DMA缓冲区
	kbd->led->transfer_dma = kbd->leds_dma;  //本urb有一个DMA缓冲区需要传输,用DMA传输要设的标志
	kbd->led->transfer_flags |= (URB_NO_TRANSFER_DMA_MAP | URB_NO_SETUP_DMA_MAP/*如果使用DMA传输则urb中setup_dma指针所指向的缓冲区是DMA缓冲区而不是setup_packet所指向的缓冲区*/);   
	
	printk("10.11\n");
	printk("kbddevname1:%s\n",kbd->dev->name);
	error = input_register_device(kbd->dev);  //注册输入设备
	printk("11\n");
	if (error)  //注册失败
		goto fail2;

	usb_set_intfdata(iface, kbd);  //设置接口私有数据,向内核注册一个data，这个data的结构可以是任意的，这段程序向内核注册了一个usb_kbd结构,这个data可以在以后用usb_get_intfdata来得到
	printk("12\n");
	return 0;

fail2:	
	usb_kbd_free_mem(dev, kbd);  //释放URB内存空间,销毁URB
fail1:	
	input_free_device(input_dev); //释放input_dev和kbd的空间
	kfree(kbd);
	return error;
}

static void usb_kbd_disconnect(struct usb_interface *intf)
{
	struct usb_kbd *kbd = usb_get_intfdata (intf);

	usb_set_intfdata(intf, NULL);
	if (kbd) {
		usb_kill_urb(kbd->irq);
                printk("disconnect1\n");
		input_unregister_device(kbd->dev);
		printk("disconnect2\n");
		usb_kbd_free_mem(interface_to_usbdev(intf), kbd);
		kfree(kbd);
	}
}

static struct usb_device_id usb_kbd_id_table [] = {  //根据该表中的信息匹配设备，找到设备后，调用probe（）
	{ USB_DEVICE(USB_KEYBOARD_VENDOR_ID, USB_KEYBOARD_PRODUCT_ID) },
	{ } 
};

MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);

static struct usb_driver usb_kbd_driver = {  //声明一个usb_driver类型的对象,并给它的各个域赋值
	.name =		"usbkbd",
	.probe =	usb_kbd_probe,
	.disconnect =	usb_kbd_disconnect,
	.id_table =	usb_kbd_id_table,
};

static int __init usb_kbd_init(void)
{
	printk("Registering...\n");
	int result = usb_register(&usb_kbd_driver);  //注册设备
	if (result == 0)  //注册成功
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
				DRIVER_DESC "\n");
	printk("Registered!\n");
	return result;
}

static void __exit usb_kbd_exit(void)
{
	printk("Deregistering...\n");
	usb_deregister(&usb_kbd_driver);  //注销设备
	printk("Deregistered!\n");
}

module_init(usb_kbd_init);
module_exit(usb_kbd_exit);
