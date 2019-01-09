/*
 *  Ubiquiti Networks XM (rev 1.0) board support
 *
 *  Copyright (C) 2011 René Bolldorf <xsecute@googlemail.com>
 *
 *  Derived from: mach-pb44.c
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ath9k_platform.h>
#include <linux/etherdevice.h>

#include <asm/mach-ath79/irq.h>

#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-usb.h"
#include "machtypes.h"

#define UBNT_XM_GPIO_LED_L1		0
#define UBNT_XM_GPIO_LED_L2		1
#define UBNT_XM_GPIO_LED_L3		11
#define UBNT_XM_GPIO_LED_L4		7

#define UBNT_XM_GPIO_BTN_RESET		12

#define UBNT_XM_KEYS_POLL_INTERVAL	20
#define UBNT_XM_KEYS_DEBOUNCE_INTERVAL	(3 * UBNT_XM_KEYS_POLL_INTERVAL)

#define UBNT_XM_EEPROM_ADDR		0x1fff1000

static struct gpio_led ubnt_xm_leds_gpio[] __initdata = {
	{
		.name		= "ubnt:red:link1",
		.gpio		= UBNT_XM_GPIO_LED_L1,
		.active_low	= 0,
	}, {
		.name		= "ubnt:orange:link2",
		.gpio		= UBNT_XM_GPIO_LED_L2,
		.active_low	= 0,
	}, {
		.name		= "ubnt:green:link3",
		.gpio		= UBNT_XM_GPIO_LED_L3,
		.active_low	= 0,
	}, {
		.name		= "ubnt:green:link4",
		.gpio		= UBNT_XM_GPIO_LED_L4,
		.active_low	= 0,
	},
};

static struct gpio_keys_button ubnt_xm_gpio_keys[] __initdata = {
	{
		.desc			= "reset",
		.type			= EV_KEY,
		.code			= KEY_RESTART,
		.debounce_interval	= UBNT_XM_KEYS_DEBOUNCE_INTERVAL,
		.gpio			= UBNT_XM_GPIO_BTN_RESET,
		.active_low		= 1,
	}
};

#define UBNT_M_WAN_PHYMASK	BIT(4)

static void __init ubnt_xm_init(void)
{
	u8 *eeprom = (u8 *) KSEG1ADDR(UBNT_XM_EEPROM_ADDR);
	u8 *mac1 = (u8 *) KSEG1ADDR(0x1fff0000);
	u8 *mac2 = (u8 *) KSEG1ADDR(0x1fff0000 + ETH_ALEN);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(ubnt_xm_leds_gpio),
				 ubnt_xm_leds_gpio);

	ath79_register_gpio_keys_polled(-1, UBNT_XM_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(ubnt_xm_gpio_keys),
					ubnt_xm_gpio_keys);

	ath79_register_m25p80(NULL);
	ap91_pci_init(eeprom, NULL);

	ath79_register_mdio(0, ~UBNT_M_WAN_PHYMASK);
	ath79_init_mac(ath79_eth0_data.mac_addr, mac1, 0);
	ath79_init_mac(ath79_eth1_data.mac_addr, mac2, 0);
	ath79_register_eth(0);
}

MIPS_MACHINE(ATH79_MACH_UBNT_XM,
	     "UBNT-XM",
	     "Ubiquiti Networks XM (rev 1.0) board",
	     ubnt_xm_init);

MIPS_MACHINE(ATH79_MACH_UBNT_BULLET_M, "UBNT-BM", "Ubiquiti Bullet M",
	     ubnt_xm_init);

static void __init ubnt_rocket_m_setup(void)
{
	ubnt_xm_init();
	ath79_register_usb();
}

MIPS_MACHINE(ATH79_MACH_UBNT_ROCKET_M, "UBNT-RM", "Ubiquiti Rocket M",
	     ubnt_rocket_m_setup);

static void __init ubnt_nano_m_setup(void)
{
	ubnt_xm_init();
	ath79_register_eth(1);
}

MIPS_MACHINE(ATH79_MACH_UBNT_NANO_M, "UBNT-NM", "Ubiquiti Nanostation M",
	     ubnt_nano_m_setup);

static struct gpio_led ubnt_airrouter_leds_gpio[] __initdata = {
	{
		.name		= "ubnt:green:globe",
		.gpio		= 0,
		.active_low	= 1,
	}, {
	        .name		= "ubnt:green:power",
		.gpio		= 11,
		.active_low	= 1,
		.default_state  = LEDS_GPIO_DEFSTATE_ON,
	}
};

static void __init ubnt_airrouter_setup(void)
{
	u8 *mac1 = (u8 *) KSEG1ADDR(0x1fff0000);
	u8 *ee = (u8 *) KSEG1ADDR(0x1fff1000);

	ath79_register_m25p80(NULL);
	ath79_register_mdio(0, ~UBNT_M_WAN_PHYMASK);

	ath79_init_mac(ath79_eth0_data.mac_addr, mac1, 0);
	ath79_init_local_mac(ath79_eth1_data.mac_addr, mac1);

	ath79_register_eth(1);
	ath79_register_eth(0);
	ath79_register_usb();

	ap91_pci_init(ee, NULL);
	ath79_register_leds_gpio(-1, ARRAY_SIZE(ubnt_airrouter_leds_gpio),
				 ubnt_airrouter_leds_gpio);

	ath79_register_gpio_keys_polled(-1, UBNT_XM_KEYS_POLL_INTERVAL,
                                        ARRAY_SIZE(ubnt_xm_gpio_keys),
                                        ubnt_xm_gpio_keys);
}

MIPS_MACHINE(ATH79_MACH_UBNT_AIRROUTER, "UBNT-AR", "Ubiquiti AirRouter",
	     ubnt_airrouter_setup);

static struct gpio_led ubnt_unifi_leds_gpio[] __initdata = {
	{
		.name		= "ubnt:orange:dome",
		.gpio		= 1,
		.active_low	= 0,
	}, {
		.name		= "ubnt:green:dome",
		.gpio		= 0,
		.active_low	= 0,
	}
};

static struct gpio_led ubnt_unifi_outdoor_leds_gpio[] __initdata = {
	{
		.name		= "ubnt:orange:front",
		.gpio		= 1,
		.active_low	= 0,
	}, {
		.name		= "ubnt:green:front",
		.gpio		= 0,
		.active_low	= 0,
	}
};


static void __init ubnt_unifi_setup(void)
{
	u8 *mac = (u8 *) KSEG1ADDR(0x1fff0000);
	u8 *ee = (u8 *) KSEG1ADDR(0x1fff1000);

	ath79_register_m25p80(NULL);

	ath79_register_mdio(0, ~UBNT_M_WAN_PHYMASK);

	ath79_init_mac(ath79_eth0_data.mac_addr, mac, 0);
	ath79_register_eth(0);

	ap91_pci_init(ee, NULL);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(ubnt_unifi_leds_gpio),
				 ubnt_unifi_leds_gpio);

	ath79_register_gpio_keys_polled(-1, UBNT_XM_KEYS_POLL_INTERVAL,
                                        ARRAY_SIZE(ubnt_xm_gpio_keys),
                                        ubnt_xm_gpio_keys);
}

MIPS_MACHINE(ATH79_MACH_UBNT_UNIFI, "UBNT-UF", "Ubiquiti UniFi",
	     ubnt_unifi_setup);


#define UBNT_UNIFIOD_PRI_PHYMASK	BIT(4)
#define UBNT_UNIFIOD_2ND_PHYMASK	(BIT(0) | BIT(1) | BIT(2) | BIT(3))

static void __init ubnt_unifi_outdoor_setup(void)
{
	u8 *mac1 = (u8 *) KSEG1ADDR(0x1fff0000);
	u8 *mac2 = (u8 *) KSEG1ADDR(0x1fff0000 + ETH_ALEN);
	u8 *ee = (u8 *) KSEG1ADDR(0x1fff1000);

	ath79_register_m25p80(NULL);

	ath79_register_mdio(0, ~(UBNT_UNIFIOD_PRI_PHYMASK |
				 UBNT_UNIFIOD_2ND_PHYMASK));

	ath79_init_mac(ath79_eth0_data.mac_addr, mac1, 0);
	ath79_init_mac(ath79_eth1_data.mac_addr, mac2, 0);
	ath79_register_eth(0);
	ath79_register_eth(1);

	ap91_pci_init(ee, NULL);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(ubnt_unifi_outdoor_leds_gpio),
				 ubnt_unifi_outdoor_leds_gpio);
}

MIPS_MACHINE(ATH79_MACH_UBNT_UNIFI_OUTDOOR, "UBNT-U20",
	     "Ubiquiti UniFiAP Outdoor",
	     ubnt_unifi_outdoor_setup);
