/*
 *  Atheros AR71XX/AR724X/AR913X USB Host Controller support
 *
 *  Copyright (C) 2008-2010 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#ifndef _ATH79_DEV_USB_H
#define _ATH79_DEV_USB_H

#include <linux/usb/ehci_pdriver.h>

extern struct usb_ehci_pdata ath79_ehci0_pdata_v2;
extern struct usb_ehci_pdata ath79_ehci1_pdata_v2;

void ath79_register_usb(void);
void ath79_init_usb_pdata(void);

#endif /* _ATH79_DEV_USB_H */
