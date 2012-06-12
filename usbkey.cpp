static void *usb_kbd_probe(struct usb_device *dev, unsigned int ifnum,
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
 
 printk(KERN_INFO "input%d: %s on usb%d:%d.%d\\n",
                 kbd->dev.number, kbd->name, dev->bus->busnum, dev->devnum, ifnum);
        return kbd;
}
