/*	$NetBSD: usb.h,v 1.3 1998/07/25 15:22:11 augustss Exp $	*/
/*	FreeBSD $Id$ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _USB_H_
#define _USB_H_

#include <sys/types.h>
#if defined(__NetBSD__)
#include <sys/ioctl.h>
#endif

#if defined(__FreeBSD__)
#include <sys/malloc.h>

#if defined(KERNEL)
MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
#endif
#endif

#define USB_MAX_DEVICES 128
#define USB_START_ADDR 0

#define USB_CONTROL_ENDPOINT 0
#define USB_MAX_ENDPOINTS 16

/*
 * The USB records contain some unaligned little-endian word
 * components.  The U[SG]ETW macros take care of both the alignment
 * and endian problem and should always be used to access 16 bit
 * values.
 */
typedef u_int8_t uByte;
typedef u_int8_t uWord[2];
#define UGETW(w) ((w)[0] | ((w)[1] << 8))
#define USETW(w,v) ((w)[0] = (u_int8_t)(v), (w)[1] = (u_int8_t)((v) >> 8))
#define USETW2(w,h,l) ((w)[0] = (u_int8_t)(l), (w)[1] = (u_int8_t)(h))
/* 
 * On little-endian machines that can handle unanliged accesses
 * (e.g. i386) these macros can be replaced by the following.
 */
#if 0
#define UGETW(w) (*(u_int16_t *)(w))
#define USETW(w,v) (*(u_int16_t *)(w) = (v))
#endif

typedef struct {
	uByte		bmRequestType;
	uByte		bRequest;
	uWord		wValue;
	uWord		wIndex;
	uWord		wLength;
} usb_device_request_t;

#define UT_WRITE		0x00
#define UT_READ			0x80
#define UT_STANDARD		0x00
#define UT_CLASS		0x20
#define UT_VENDOR		0x40
#define UT_DEVICE		0x00
#define UT_INTERFACE		0x01
#define UT_ENDPOINT		0x02
#define UT_OTHER		0x03

#define UT_READ_DEVICE		(UT_READ  | UT_STANDARD | UT_DEVICE)
#define UT_READ_INTERFACE	(UT_READ  | UT_STANDARD | UT_INTERFACE)
#define UT_READ_ENDPOINT	(UT_READ  | UT_STANDARD | UT_ENDPOINT)
#define UT_WRITE_DEVICE		(UT_WRITE | UT_STANDARD | UT_DEVICE)
#define UT_WRITE_INTERFACE	(UT_WRITE | UT_STANDARD | UT_INTERFACE)
#define UT_WRITE_ENDPOINT	(UT_WRITE | UT_STANDARD | UT_ENDPOINT)
#define UT_READ_CLASS_DEVICE	(UT_READ  | UT_CLASS | UT_DEVICE)
#define UT_READ_CLASS_INTERFACE	(UT_READ  | UT_CLASS | UT_INTERFACE)
#define UT_READ_CLASS_OTHER	(UT_READ  | UT_CLASS | UT_OTHER)
#define UT_WRITE_CLASS_DEVICE	(UT_WRITE | UT_CLASS | UT_DEVICE)
#define UT_WRITE_CLASS_INTERFACE (UT_WRITE | UT_CLASS | UT_INTERFACE)
#define UT_WRITE_CLASS_OTHER	(UT_WRITE | UT_CLASS | UT_OTHER)

/* Requests */
#define UR_GET_STATUS		0x00
#define UR_CLEAR_FEATURE	0x01
#define UR_SET_FEATURE		0x03
#define UR_SET_ADDRESS		0x05
#define UR_GET_DESCRIPTOR	0x06
#define  UDESC_DEVICE		1
#define  UDESC_CONFIG		2
#define  UDESC_STRING		3
#define  UDESC_INTERFACE	4
#define  UDESC_ENDPOINT		5
#define UR_SET_DESCRIPTOR	0x07
#define UR_GET_CONFIG		0x08
#define UR_SET_CONFIG		0x09
#define UR_GET_INTERFACE	0x0a
#define UR_SET_INTERFACE	0x0b
#define UR_SYNCH_FRAME		0x0c

/* Feature numbers */
#define UF_ENDPOINT_STALL	0
#define UF_DEVICE_REMOTE_WAKEUP	1

#define USB_MAX_IPACKET		8 /* maximum size of the initial packet */

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
} usb_descriptor_t;

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bcdUSB;
	uByte		bDeviceClass;
	uByte		bDeviceSubClass;
	uByte		bDeviceProtocol;
	uByte		bMaxPacketSize;
	/* The fields below are not part of the initial descriptor. */
	uWord		idVendor;
	uWord		idProduct;
	uWord		bcdDevice;
	uByte		iManufacturer;
	uByte		iProduct;
	uByte		iSerialNumber;
	uByte		bNumConfigurations;
} usb_device_descriptor_t;
#define USB_DEVICE_DESCRIPTOR_SIZE 18

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		wTotalLength;
	uByte		bNumInterface;
	uByte		bConfigurationValue;
	uByte		iConfiguration;
	uByte		bmAttributes;
#define UC_BUS_POWERED		0x80
#define UC_SELF_POWERED		0x40
#define UC_REMOTE_WAKEUP	0x20
	uByte		bMaxPower; /* max current in 2 mA units */
#define UC_POWER_FACTOR 2
} usb_config_descriptor_t;
#define USB_CONFIG_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bInterfaceNumber;
	uByte		bAlternateSetting;
	uByte		bNumEndpoints;
	uByte		bInterfaceClass;
	uByte		bInterfaceSubClass;
	uByte		bInterfaceProtocol;
	uByte		iInterface;
} usb_interface_descriptor_t;
#define USB_INTERFACE_DESCRIPTOR_SIZE 9

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bEndpointAddress;
#define UE_IN		0x80
#define UE_OUT		0x00
#define UE_ADDR		0x0f
#define UE_GET_IN(a)	(((a) >> 7) & 1)
	uByte		bmAttributes;
#define UE_CONTROL	0x00
#define UE_ISOCHRONOUS	0x01
#define UE_BULK		0x02
#define UE_INTERRUPT	0x03
#define UE_XFERTYPE	0x03
	uWord		wMaxPacketSize;
	uByte		bInterval;
} usb_endpoint_descriptor_t;
#define USB_ENDPOINT_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uWord		bString[127];
} usb_string_descriptor_t;
#define USB_MAX_STRING_LEN 128

/* Hub specific request */
#define UR_GET_BUS_STATE	0x02

/* Hub features */
#define UHF_C_HUB_LOCAL_POWER	0
#define UHF_C_HUB_OVER_CURRENT	1
#define UHF_PORT_CONNECTION	0
#define UHF_PORT_ENABLE		1
#define UHF_PORT_SUSPEND	2
#define UHF_PORT_OVER_CURRENT	3
#define UHF_PORT_RESET		4
#define UHF_PORT_POWER		8
#define UHF_PORT_LOW_SPEED	9
#define UHF_C_PORT_CONNECTION	16
#define UHF_C_PORT_ENABLE	17
#define UHF_C_PORT_SUSPEND	18
#define UHF_C_PORT_OVER_CURRENT	19
#define UHF_C_PORT_RESET	20

typedef struct {
	uByte		bDescLength;
	uByte		bDescriptorType;
	uByte		bNbrPorts;
	uWord		bHubCharacteristics;
#define UHD_PWR			0x03
#define UHD_PWR_GANGED		0x00
#define UHD_PWR_INDIVIDUAL	0x01
#define UHD_PWR_NO_SWITCH	0x02
#define UHD_COMPOUND		0x04
#define UHD_OC			0x18
#define UHD_OC_GLOBAL		0x00
#define UHD_OC_INDIVIDUAL	0x08
#define UHD_OC_NONE		0x10
	uByte		bPwrOn2PwrGood;	/* delay in 2 ms units */
#define UHD_PWRON_FACTOR 2
	uByte		bHubContrCurrent;
	uByte		DeviceRemovable[1];
	/* this is only correct with 1-7 ports on the hub */
	uByte		PortPowerCtrlMask[3];
} usb_hub_descriptor_t;
#define USB_HUB_DESCRIPTOR_SIZE 9

typedef struct {
	uWord		wStatus;
/* Device status flags */
#define UDS_SELF_POWERED		0x0001
#define UDS_REMOTE_WAKEUP		0x0002
} usb_status_t;

typedef struct {
	uWord		wHubStatus;
#define UHS_LOCAL_POWER			0x0001
#define UHS_OVER_CURRENT		0x0002
	uWord		wHubChange;
} usb_hub_status_t;

typedef struct {
	uWord		wPortStatus;
#define UPS_CURRENT_CONNECT_STATUS	0x0001
#define UPS_PORT_ENABLED		0x0002
#define UPS_SUSPEND			0x0004
#define UPS_OVERCURRENT_INDICATOR	0x0008
#define UPS_RESET			0x0010
#define UPS_PORT_POWER			0x0100
#define UPS_LOW_SPEED			0x0200
	uWord		wPortChange;
#define UPS_C_CONNECT_STATUS		0x0001
#define UPS_C_PORT_ENABLED		0x0002
#define UPS_C_SUSPEND			0x0004
#define UPS_C_OVERCURRENT_INDICATOR	0x0008
#define UPS_C_PORT_RESET		0x0010
} usb_port_status_t;

#define UDESC_CS_DEVICE		0x21
#define UDESC_CS_CONFIG		0x22
#define UDESC_CS_STRING		0x23
#define UDESC_CS_INTERFACE	0x24
#define UDESC_CS_ENDPOINT	0x25

#define UDESC_HUB		0x29

#define UCLASS_UNSPEC		0
#define UCLASS_AUDIO		1
#define  USUBCLASS_AUDIOCONTROL	1
#define  USUBCLASS_AUDIOSTREAM	2
#define UCLASS_HID		3
#define  USUBCLASS_BOOT	 	1
#define UCLASS_PRINTER		7
#define  USUBCLASS_PRINTER	1
#define  UPROTO_PRINTER_UNI	1
#define  UPROTO_PRINTER_BI	2
#define UCLASS_HUB		9
#define  USUBCLASS_HUB		1

#define USB_HUB_MAX_DEPTH 5

#define USB_PORT_RESET_DELAY	10  /* ms */
#define USB_PORT_POWERUP_DELAY	100 /* ms */
#define USB_POWER_SETTLE	100 /* ms */

#define USB_MIN_POWER		100 /* mA */
#define USB_MAX_POWER		500 /* mA */


#define USB_RESET_DELAY		100 /* ms XXX?*/
#define USB_RESUME_DELAY	10 /* ms XXX?*/

/*** ioctl() related stuff ***/

struct usb_ctl_request {
	int	addr;
	usb_device_request_t request;
	void	*data;
};

struct usb_all_desc {
	u_char	data[1024];	/* filled data size will vary */
};

struct usb_ctl_report_desc {
	int	size;
	u_char	data[1024];	/* filled data size will vary */
};

struct usb_device_info {
	uByte	addr;		/* device address */
	char	product[USB_MAX_STRING_LEN];
	char	vendor[USB_MAX_STRING_LEN];
	char	revision[8];
	uByte	class;
	uByte	config;
	uByte	lowspeed;
	int	power;		/* power consumption in mA, 0 if selfpowered */
	int	nports;
	uByte	ports[16];	/* hub only: addresses of devices on ports */
#define USB_PORT_ENABLED 0xff
#define USB_PORT_SUSPENDED 0xfe
#define USB_PORT_POWERED 0xfd
#define USB_PORT_DISABLED 0xfc
};

struct usb_ctl_report {
	int report;
	u_char	data[1024];	/* filled data size will vary */
};

struct usb_device_stats {
	u_long	requests[4];	/* indexed by transfer type UE_* */
};

/* USB controller */
#define USB_REQUEST		_IOWR('U', 1, struct usb_ctl_request)
#define USB_SETDEBUG		_IOW ('U', 2, int)
#define USB_DISCOVER		_IO  ('U', 3)
#define USB_DEVICEINFO		_IOWR('U', 4, struct usb_device_info)
#define USB_DEVICESTATS		_IOR ('U', 5, struct usb_device_stats)

/* Generic HID device */
#define USB_GET_REPORT_DESC	_IOR ('U', 21, struct usb_ctl_report_desc)
#define USB_SET_IMMED		_IOW ('U', 22, int)
#define USB_GET_REPORT		_IOWR('U', 23, struct usb_ctl_report)

/* Generic USB device */
#define USB_SET_CONFIG		_IOW ('U', 100, int)
#define USB_SET_INTERFACE	_IOW ('U', 101, int)
#define USB_GET_DEVICE_DESC	_IOR ('U', 102, usb_device_descriptor_t)
#define USB_GET_CONFIG_DESC	_IOR ('U', 103, usb_config_descriptor_t)
#define USB_GET_INTERFACE_DESC	_IOR ('U', 104, usb_interface_descriptor_t)
#define USB_GET_ALL_DESC	_IOR ('U', 105, struct usb_all_desc)


#endif /* _USB_H_ */
