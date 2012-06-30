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

struct usb_kbd {   //  ����USB���̽ṹ�壺
	struct input_dev *dev;   //һ�������豸
	struct usb_device *usbdev;  //����һ��usb�豸
	unsigned char old[8];  //ǰһ�δ���urb����ʱ����״̬�Ļ�����
	struct urb *irq, *led;  //�����Ϳ��Ƶ��ж������urb
	unsigned char newleds; //����ָ����״̬
	char name[128];  //�������ֺͲ�Ʒ����
	char phys[64]; //�豸�ڵ�

	unsigned char *new;  //��������ʱ����urb����ʱ��ǰ����״̬�Ļ�����
	struct usb_ctrlrequest *cr;  //��������ṹ
	unsigned char *leds;  //��ǰָʾ��״̬
	dma_addr_t cr_dma;  //��������URB��DMA�����ַ
	dma_addr_t new_dma;  //�ж�urb��ʹ�ø�DMA������
	dma_addr_t leds_dma; //ָʾ��DMA�����ַ
};

//�����ж��¼�������

static void usb_kbd_irq(struct urb *urb)
{
	printk("Starting irq\n");
	struct usb_kbd *kbd = urb->context;
	int i;

	switch (urb->status) {  //�ж�URB��״̬
	case 0:			//URB���ɹ�����
		break;
	case -ECONNRESET:	//�Ͽ����Ӵ���,urbδ��ֹ�ͷ��ظ��˻ص�����
	case -ENOENT: //urb��kill��,�������ڳ��ױ���ֹ
	case -ESHUTDOWN:  //USB�����������������������صĴ���,�����ύ���һ˲���豸���γ�
		return;
	
	default:		//��������,�����������ύurb
		goto resubmit;
	}
	printk("irq1\n");
	for (i = 0; i < 8; i++)
		input_report_key(kbd->dev, usb_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);/*usb_kbd_keycode[224]-usb_kbd_keycode[231],8�ε�ֵ������:29-42-56-125-97-54-100-126,�ж���8������״̬*/
	printk("irq2\n");
	//��ͬʱֻ����2����������һ�����ڵ�[2]���ֽ�,��ͬʱ������������ڶ����ڵ�[3]�ֽڣ�����������6������ͬʱ����
	for (i = 2; i < 8; i++) {

		if (kbd->old[i] > 3 && memscan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8) {  //�ж���Щ����״̬�ı���,���ɰ��±�Ϊ���ɿ�
			if (usb_kbd_keycode[kbd->old[i]])  //�Ǽ������õİ���,�ͱ��水���뿪
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->old[i]], 0);
			else                   //���Ǽ������õİ���
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->old[i]);
		}

		if (kbd->new[i] > 3 && memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) {  //�ж���Щ����״̬�ı���,�����ɿ���Ϊ�˰���
			if (usb_kbd_keycode[kbd->new[i]])  //�Ǽ������õİ���,�ͱ��水��������
				input_report_key(kbd->dev, usb_kbd_keycode[kbd->new[i]], 1);
			else        //���Ǽ������õİ���
				dev_info(&urb->dev->dev,
						"Unknown key (scancode %#x) released.\n", kbd->new[i]);
		}
	}

	input_sync(kbd->dev);  //ͬ���豸,��֪�¼��Ľ����������Ѿ�������һ��������input��ϵͳ�ı���
	
        printk("irq2.1\n");
	memcpy(kbd->old, kbd->new, 8);  //�����εİ���״̬������kbd->old,�����´�urb����ʱ�жϰ���״̬�ĸı�
        printk("irq3\n");
resubmit:
	i = usb_submit_urb (urb, GFP_ATOMIC); //���·���urb�����
	if (i)  //����urb�����ʧ��
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


 //��дUSB�豸�򿪺���:�򿪼����豸ʱ����ʼ�ύ�� probe �����й����� urb������ urb ���ڡ� 
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

// ��дUSB�豸�رպ������رռ����豸ʱ������ urb �������ڡ� 
static void usb_kbd_close(struct input_dev *dev)
{
	struct usb_kbd *kbd = input_get_drvdata(dev);

	usb_kill_urb(kbd->irq);
}


//����URB������URB�ڴ�ռ伴����URB
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


// ����URB���ͷ�URB�ڴ�ռ伴����URB
static void usb_kbd_free_mem(struct usb_device *dev, struct usb_kbd *kbd)
{
	usb_free_urb(kbd->irq);
	usb_free_urb(kbd->led);
	usb_buffer_free(dev, 8, kbd->new, kbd->new_dma);
	usb_buffer_free(dev, sizeof(struct usb_ctrlrequest), kbd->cr, kbd->cr_dma);
	usb_buffer_free(dev, 1, kbd->leds, kbd->leds_dma);
}

static int usb_kbd_probe(struct usb_interface *iface,
			 const struct usb_device_id *id)  //usb_interface *iface:���ں��Զ���ȡ�Ľӿ�,һ���ӿڶ�Ӧһ�ֹ���, struct usb_device_id *id:�豸�ı�ʶ��
{
	printk("Starting probe\n");
	struct usb_device *dev = interface_to_usbdev(iface);  //��ȡusb�ӿڽṹ���е�usb�豸�ṹ��,ÿ��USB�豸��Ӧһ��struct usb_device�ı�������usb core��������͸�ֵ
	struct usb_host_interface *interface;  //���ӵ��Ľӿڵ�����
	struct usb_endpoint_descriptor *endpoint;    //�������ݹܵ��Ķ˵�
	struct usb_kbd *kbd;  //usb�豸���û��ռ������
	struct input_dev *input_dev;  //��ʾ�����豸
	int i, pipe, maxp; 
	int error = -ENOMEM; 
	
	
	interface = iface->cur_altsetting; //�����ӵ��Ľӿڵ���������Ϊ��ǰ��setting
	printk("1\n");
	if (interface->desc.bNumEndpoints != 1) //�ж��ж�IN�˵�ĸ���,����ֻ��һ���˵�,�����Ϊ1,�����,desc���豸������
		return -ENODEV;

	endpoint = &interface->endpoint[0].desc; //ȡ�ü����ж�IN�˵��������,endpoint[0]��ʾ�ж϶˵�
	printk("2\n");
	if (!usb_endpoint_is_int_in(endpoint)) //�鿴����õĶ˵��Ƿ�Ϊ�ж�IN�˵�
		return -ENODEV;
  printk("3\n");
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress); //�õ�����������ж�OUT�˵��,�����ܵ������������������򻺳������豸�˿�
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe)); //�õ������Դ�������ݰ�(�ֽ�)

	kbd = kzalloc(sizeof(struct usb_kbd), GFP_KERNEL); //Ϊkbd�ṹ������ڴ�,GFP_KERNEL���ں��ڴ����ʱ��õı�־λ�����ڴ����ʱ����������
	input_dev = input_allocate_device();   //Ϊ�����豸�Ľṹ������ڴ�,����ʼ����
	printk("inputdev-1:%s\n",input_dev->name);
	printk("4\n");
	if (!kbd || !input_dev) //��kbd��input_dev�����ڴ�ʧ��
		goto fail1;
        printk("5\n");
	if (usb_kbd_alloc_mem(dev, kbd)) //����urb�ڴ�ռ�ʧ��,������urbʧ��
		goto fail2;
	printk("6\n");
	kbd->usbdev = dev;    //��kbd��usb�豸�ṹ��usbdev��ֵ
	kbd->dev = input_dev; //��kbd�������豸�ṹ��dev��ֵ,����������ͳһ��kbd��װ,input��ϵͳֻ�ܴ���input_dev���͵Ķ���
	printk("7\n");
	if (dev->manufacturer) //��������,��Ʒ����ֵ��kbd��name��Ա	
	{
		printk("7.1\n");
		strlcpy(kbd->name, dev->manufacturer, sizeof(kbd->name));
	}
	printk("8\n");
	
	if (dev->product) {
		printk("7.2\n");
		if (dev->manufacturer) //�г�����,���ڲ�Ʒ��֮ǰ����ո�
			strlcat(kbd->name, " ", sizeof(kbd->name));
		strlcat(kbd->name, dev->product, sizeof(kbd->name));
	}

	printk("9\n");

  printk("10\n");
	usb_make_path(dev, kbd->phys, sizeof(kbd->phys)); //�����豸������·���ĵ�ַ���豸���ӵ�ַ�������豸�İγ����ı�
	printk("10.1\n");
	strlcpy(kbd->phys, "/input0", sizeof(kbd->phys));
	printk("10.2\n");
	input_dev->name = kbd->name; //��input_dev��name��ֵ
	printk("inputdevname:%s\n",input_dev->name);
	printk("kbddevname:%s\n",kbd->dev->name);
	input_dev->phys = kbd->phys;  //�豸���ӵ�ַ
	usb_to_input_id(dev, &input_dev->id); //�������豸�ṹ��input->�ı�ʶ���ṹ��ֵ,��Ҫ����bustype��vendo��product��
	printk("10.3\n");
	//input_dev->dev.parent = &iface->dev;

	//input_set_drvdata(input_dev, kbd);
	printk("10.4\n");
	input_dev->evbit[0] = BIT_MASK(EV_KEY) /*�����¼�*/| BIT_MASK(EV_LED) | /*LED�¼�*/
		BIT_MASK(EV_REP)/*�Զ��ظ���ֵ*/;  //֧�ֵ��¼�����
	printk("10.5\n");
	input_dev->ledbit[0] = BIT_MASK(LED_NUML) /*���ֵ�*/| BIT_MASK(LED_CAPSL) |/*��Сд��*/
		BIT_MASK(LED_SCROLLL)/*������*/ ;   //EV_LED�¼�֧�ֵ��¼���
	printk("10.6\n");

	for (i = 0; i < 255; i++)
		set_bit(usb_kbd_keycode[i], input_dev->keybit); // ��ʼ��,ÿ������ɨ���붼���Գ��������¼�
	printk("10.7\n");
	clear_bit(0, input_dev->keybit); //Ϊ0�ļ���ɨ���벻�ܴ��������¼�
	printk("10.8\n");
	input_dev->event = usb_kbd_event; //����input�豸�Ĵ򿪡��رա�д������ʱ�Ĵ�����
	input_dev->open = usb_kbd_open;
	input_dev->close = usb_kbd_close;
  //��ʼ���ж�URB
	usb_fill_int_urb(kbd->irq/*��ʼ��kbd->irq���urb*/, dev/*���urbҪ���͵�dev����豸*/, pipe/*���urbҪ���͵�pipe����˵�*/,
			 kbd->new/*ָ�򻺳��ָ��*/, (maxp > 8 ? 8 : maxp)/*����������(������8)*/,
			 usb_kbd_irq/*���urb���ʱ���õĴ�����*/, kbd/*ָ�����ݿ��ָ�룬����ӵ����urb�ṹ�ɱ���ɴ�������ȡ*/, endpoint->bInterval/*urbӦ�������ȵļ��*/);
	printk("10.9\n");
	kbd->irq->transfer_dma = kbd->new_dma;  //ָ��urb��Ҫ�����DMA������
	kbd->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;  //��urb��һ��DMA��������Ҫ���䣬��DMA����Ҫ��ı�־

	kbd->cr->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;  //��������USB��ӿڶ���
	kbd->cr->bRequest = 0x09;  //�ж�������
	kbd->cr->wValue = cpu_to_le16(0x200); //��ˡ�С��ģʽת��
	kbd->cr->wIndex = cpu_to_le16(interface->desc.bInterfaceNumber); //�ӿں�
	kbd->cr->wLength = cpu_to_le16(1);  //һ�����ݴ���Ҫ�����ֽ���
  //��ʼ������URB
	printk("10.10\n");
	usb_fill_control_urb(kbd->led/*��ʼ��kbd->led���urb*/, dev/*���urbҪ��dev����豸����*/, usb_sndctrlpipe(dev, 0)/*urb���͵��Ķ˵�*/,
			     (void *) kbd->cr/*���͵�setup packet*/, kbd->leds/*���������ݵĻ�����*/, 1/*�������ݳ���*/,
			     usb_kbd_led/*���urb���ʱ���õĴ�����*/, kbd/*ָ�����ݿ��ָ�룬����ӵ����urb�ṹ�ɱ���ɴ�������ȡ*/);
	kbd->led->setup_dma = kbd->cr_dma;  //ָ��urb��Ҫ�����DMA������
	kbd->led->transfer_dma = kbd->leds_dma;  //��urb��һ��DMA��������Ҫ����,��DMA����Ҫ��ı�־
	kbd->led->transfer_flags |= (URB_NO_TRANSFER_DMA_MAP | URB_NO_SETUP_DMA_MAP/*���ʹ��DMA������urb��setup_dmaָ����ָ��Ļ�������DMA������������setup_packet��ָ��Ļ�����*/);   
	
	printk("10.11\n");
	printk("kbddevname1:%s\n",kbd->dev->name);
	error = input_register_device(kbd->dev);  //ע�������豸
	printk("11\n");
	if (error)  //ע��ʧ��
		goto fail2;

	usb_set_intfdata(iface, kbd);  //���ýӿ�˽������,���ں�ע��һ��data�����data�Ľṹ����������ģ���γ������ں�ע����һ��usb_kbd�ṹ,���data�������Ժ���usb_get_intfdata���õ�
	printk("12\n");
	return 0;

fail2:	
	usb_kbd_free_mem(dev, kbd);  //�ͷ�URB�ڴ�ռ�,����URB
fail1:	
	input_free_device(input_dev); //�ͷ�input_dev��kbd�Ŀռ�
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

static struct usb_device_id usb_kbd_id_table [] = {  //���ݸñ��е���Ϣƥ���豸���ҵ��豸�󣬵���probe����
	{ USB_DEVICE(USB_KEYBOARD_VENDOR_ID, USB_KEYBOARD_PRODUCT_ID) },
	{ } 
};

MODULE_DEVICE_TABLE (usb, usb_kbd_id_table);

static struct usb_driver usb_kbd_driver = {  //����һ��usb_driver���͵Ķ���,�������ĸ�����ֵ
	.name =		"usbkbd",
	.probe =	usb_kbd_probe,
	.disconnect =	usb_kbd_disconnect,
	.id_table =	usb_kbd_id_table,
};

static int __init usb_kbd_init(void)
{
	printk("Registering...\n");
	int result = usb_register(&usb_kbd_driver);  //ע���豸
	if (result == 0)  //ע��ɹ�
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
				DRIVER_DESC "\n");
	printk("Registered!\n");
	return result;
}

static void __exit usb_kbd_exit(void)
{
	printk("Deregistering...\n");
	usb_deregister(&usb_kbd_driver);  //ע���豸
	printk("Deregistered!\n");
}

module_init(usb_kbd_init);
module_exit(usb_kbd_exit);
