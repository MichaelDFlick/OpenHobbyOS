#ifndef OHOS_USB_H
#define OHOS_USB_H

#include "types.h"

#define USB_MAX_DEVICES       16
#define USB_MAX_CONFIG_BYTES  256
#define USB_MAX_CLASS_DRIVERS 8

#define USB_STATE_DEFAULT     0
#define USB_STATE_ADDRESS     1
#define USB_STATE_CONFIGURED  2

#define USB_DIR_IN            (1 << 7)
#define USB_DIR_OUT           0
#define USB_TYPE_STANDARD     (0 << 5)
#define USB_TYPE_CLASS        (1 << 5)
#define USB_RECIP_DEVICE      0
#define USB_RECIP_INTERFACE   1
#define USB_RECIP_ENDPOINT    2
#define USB_RECIP_OTHER       3

#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03

#define USB_DESC_DEVICE      1
#define USB_DESC_CONFIG      2
#define USB_DESC_STRING      3
#define USB_DESC_INTERFACE   4
#define USB_DESC_ENDPOINT    5
#define USB_DESC_HID         0x21

#define USB_CLASS_HUB             0x09
#define USB_CLASS_HID             0x03
#define USB_CLASS_MASS_STORAGE    0x08

#define USB_ENDPOINT_OUT     0x00
#define USB_ENDPOINT_IN      0x80

typedef struct PACKED {
    u8  bmRequestType;
    u8  bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} usb_setup_packet_t;

typedef struct PACKED {
    u8  bLength;
    u8  bDescriptorType;
    u16 bcdUSB;
    u8  bDeviceClass;
    u8  bDeviceSubClass;
    u8  bDeviceProtocol;
    u8  bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8  iManufacturer;
    u8  iProduct;
    u8  iSerialNumber;
    u8  bNumConfigurations;
} usb_device_descriptor_t;

typedef struct PACKED {
    u8  bLength;
    u8  bDescriptorType;
    u16 wTotalLength;
    u8  bNumInterfaces;
    u8  bConfigurationValue;
    u8  iConfiguration;
    u8  bmAttributes;
    u8  bMaxPower;
} usb_configuration_descriptor_t;

typedef struct PACKED {
    u8  bLength;
    u8  bDescriptorType;
    u8  bInterfaceNumber;
    u8  bAlternateSetting;
    u8  bNumEndpoints;
    u8  bInterfaceClass;
    u8  bInterfaceSubClass;
    u8  bInterfaceProtocol;
    u8  iInterface;
} usb_interface_descriptor_t;

typedef struct PACKED {
    u8  bLength;
    u8  bDescriptorType;
    u8  bEndpointAddress;
    u8  bmAttributes;
    u16 wMaxPacketSize;
    u8  bInterval;
} usb_endpoint_descriptor_t;

typedef struct PACKED {
    u8  bLength;
    u8  bDescriptorType;
    u16 bcdHID;
    u8  bCountryCode;
    u8  bNumDescriptors;
    u8  bReportDescriptorType;
    u16 wReportDescriptorLength;
} usb_hid_descriptor_t;

typedef struct usb_device {
    u8  address;
    u8  port;
    u8  speed;
    struct usb_device *hub;
    usb_device_descriptor_t desc;
    u8  config_raw[USB_MAX_CONFIG_BYTES];
    u8  config_length;
    u8  configuration;
    bool configured;
    void *hc_private;
} usb_device_t;

typedef struct {
    int (*control_transfer)(usb_device_t *dev, usb_setup_packet_t *req, void *data);
    int (*interrupt_transfer)(usb_device_t *dev, u8 endpoint, void *data, u16 length);
    int (*bulk_transfer)(usb_device_t *dev, u8 endpoint, void *data, u16 length);
    int (*reset_port)(usb_device_t *hub, u8 port);
} usb_hcd_ops_t;

typedef struct {
    u8 class;
    u8 subclass;
    u8 protocol;
    void (*probe)(usb_device_t *dev);
} usb_class_driver_t;

int  usb_init(void);
int  usb_class_driver_register(const usb_class_driver_t *driver);
void usb_probe_device(usb_device_t *dev);

int  usb_device_add(usb_device_t *dev);
void usb_device_remove(u8 address);
usb_device_t *usb_device_get(u8 address);

int  usb_control_transfer(usb_device_t *dev, u8 bmReqType, u8 bRequest,
                          u16 wValue, u16 wIndex, u16 wLength, void *data);
int  usb_get_descriptor(usb_device_t *dev, u8 type, u8 index, void *buf, u16 length);
int  usb_set_address(usb_device_t *dev, u8 address);
int  usb_set_configuration(usb_device_t *dev, u8 config);
int  usb_clear_halt(usb_device_t *dev, u8 endpoint);
int  usb_reset_port(usb_device_t *hub, u8 port);

void usb_hub_init(void);
void usb_keyboard_init(void);
void usb_mouse_init(void);
void usb_storage_init(void);

#endif
