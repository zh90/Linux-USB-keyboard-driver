static void *usb_kbd_probe(struct usb_device *dev, unsigned int ifnum,        //first edition   USB键盘驱动探测函数，初始化设备并指定一些处理函数的地址
                           const struct usb_device_id *id)
{
 struct usb_interface *iface;
        struct usb_interface_descriptor *interface;
 struct usb_endpoint_descriptor *endpoint;
        struct usb_kbd *kbd;
        int  pipe, maxp;
 iface = &dev->actconfig->interface[ifnum];
        interface = &iface->altsetting[iface->act_altsetting];
 if ((dev->descriptor.idVendor != USB_HOTKEY_VENDOR_ID) ||
  (dev->descriptor.idProduct != USB_HOTKEY_PRODUCT_ID) ||
  (ifnum != 1))
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
        printk(KERN_INFO "GUO: Vid = %.4x, Pid = %.4x, Device = %.2x, ifnum = %.2x, bufCount = %.8x\\n",
        dev->descriptor.idVendor,dev->descriptor.idProduct,dev->descriptor.bcdDevice, ifnum, maxp);
        if (!(kbd = kmalloc(sizeof(struct usb_kbd), GFP_KERNEL))) return NULL;
        memset(kbd, 0, sizeof(struct usb_kbd));
        kbd->usbdev = dev;
        FILL_INT_URB(&kbd->irq, dev, pipe, kbd->new, maxp > 8 ? 8 : maxp,
        usb_kbd_irq, kbd, endpoint->bInterval);
        kbd->irq.dev = kbd->usbdev;
 if (dev->descriptor.iManufacturer)
                usb_string(dev, dev->descriptor.iManufacturer, kbd->name, 63);
 if (usb_submit_urb(&kbd->irq)) {
                kfree(kbd);
                return NULL;
        }
 
 p      rintk(KERN_INFO "input%d: %s on usb%d:%d.%d\\n",
                 kbd->dev.number, kbd->name, dev->bus->busnum, dev->devnum, ifnum);
        return kbd;
}


static int usb_kbd_probe(struct usb_interface *iface,                         //second edition   USB键盘驱动探测函数，初始化设备并指定一些处理函数的地址
			 const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(iface);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_kbd *kbd;
	struct input_dev *input_dev;
	int i, pipe, maxp;

	printk("SUNWILL-USBKBD:begin_kbd_probe begin...\n");

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

	/*获得设备节点*/
	usb_make_path(dev, kbd->phys, sizeof(kbd->phys));
	strlcpy(kbd->phys, "/input0", sizeof(kbd->phys));

	input_dev->name = kbd->name;
	input_dev->phys = kbd->phys;
	/* 
     * input_dev 中的 input_id 结构体，用来存储厂商、设备类型和设备的编号，这个函数是将设备描述符 
     * 中的编号赋给内嵌的输入子系统结构体 
     */
	usb_to_input_id(dev, &input_dev->id);
	  /* cdev 是设备所属类别（class device） */ 
	input_dev->cdev.dev = &iface->dev;
	/* input_dev 的 private 数据项用于表示当前输入设备的种类，这里将键盘结构体对象赋给它 */ 
	input_dev->private = kbd;

	input_dev->evbit[0] = BIT(EV_KEY)/*键码事件*/ | BIT(EV_LED)/*LED事件*/ | BIT(EV_REP)/*自动重覆数值*/;
	input_dev->ledbit[0] = BIT(LED_NUML)/*数字灯*/ | BIT(LED_CAPSL)/*大小写灯*/ | BIT(LED_SCROLLL)/*滚动灯*/ | BIT(LED_COMPOSE) | BIT(LED_KANA);

	for (i = 0; i < 255; i++)
		set_bit(usb_kbd_keycode[i], input_dev->keybit);
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
	/*设置接口私有数据*/
	usb_set_intfdata(iface, kbd);

	return 0;

fail2:	usb_kbd_free_mem(dev, kbd);
fail1:	input_free_device(input_dev);
	kfree(kbd);
	return -ENOMEM;
}
