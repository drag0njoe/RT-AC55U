/*
 * Atheros CUS227 board support
 *
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#include <linux/ar8216_platform.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mtd/mtd.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>

#include <linux/of_platform.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-audio.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-nand.h"
#include "dev-spi.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "dev-i2c.h"
#include "machtypes.h"
#include "nand-caldata-fixup.h"

#define CUS227_GPIO_S1			12
#define CUS227_GPIO_S2			13
#define CUS227_GPIO_S3			14
#define CUS227_GPIO_S4			15
#define CUS227_GPIO_I2C_SDA		16
#define CUS227_GPIO_I2C_SCL		17

#define CUS227_KEYS_POLL_INTERVAL	20	/* msecs */
#define CUS227_KEYS_DEBOUNCE_INTERVAL	(3 * CUS227_KEYS_POLL_INTERVAL)

#define CUS227_GPIO_SPI_CLK		6
#define CUS227_GPIO_SPI_MOSI		7
#define CUS227_GPIO_SPI_MISO		8
#define CUS227_GPIO_I2S_MCLK		22
#define CUS227_GPIO_I2S_SD		18
#define CUS227_GPIO_I2S_WS		20
#define CUS227_GPIO_I2S_CLK		21
#define CUS227_GPIO_I2S_MIC_SD		19
#define CUS227_GPIO_SPDIF_OUT		4

#define CUS227_GPIO_SPI_CS1		11

#define CUS227_MAC0_OFFSET		0
#define CUS227_WMAC_CALDATA_OFFSET	0x1000

static struct ath79_caldata_fixup cus227_caldata = {
	.name = "art",
	.wmac_caldata_addr = CUS227_WMAC_CALDATA_OFFSET,
	.pcie_caldata_addr = { FIXUP_UNDEFINED, FIXUP_UNDEFINED },
	.mac_addr = { FIXUP_UNDEFINED, CUS227_MAC0_OFFSET },
};

static struct of_device_id __initdata cus227_common_ids[] = {
	{ .type = "soc", },
	{ .compatible = "gpio-keys", },
	{ .compatible = "gpio-leds", },
	{},
};

/* There is no H/W I2C controller in ATH79 SoCs, so use GPIO based S/W I2C master */
static struct i2c_gpio_platform_data ath79_i2c_gpio_data = {
	.sda_pin	= CUS227_GPIO_I2C_SDA,
	.scl_pin	= CUS227_GPIO_I2C_SCL,
};


/* Because CUS227 doesn't have a NOR flash on the SPI bus, we cannot
 * reuse the routines from dev-m25p80.c to instanciate it.
 * That's also the reason why the first device on the bus is 1 and not 0 */
static struct ath79_spi_controller_data ath79_spi1_cdata =
{
	.cs_type = ATH79_SPI_CS_TYPE_INTERNAL,
	.is_flash = false,
	.cs_line = 1,
};

static struct spi_board_info cus227_spi_info[] = {
	{
		.bus_num	= 0,
		.chip_select	= 1,
		.max_speed_hz   = 25000000,
		.modalias	= "wm8988",
		.controller_data = &ath79_spi1_cdata,
	}
};


static void __init cus227_audio_setup(void)
{
	u32 t;

	/* Reset I2S internal controller */
	t = ath79_reset_rr(AR71XX_RESET_REG_RESET_MODULE);
	ath79_reset_wr(AR71XX_RESET_REG_RESET_MODULE, t | AR934X_RESET_I2S );
	udelay(1);

	/* GPIO configuration
	   Please note that the value in direction_output doesn't really matter
	   here as GPIOs are configured to relay internal data signal
	*/
	gpio_request(CUS227_GPIO_I2S_CLK, "I2S CLK");
	ath79_gpio_output_select(CUS227_GPIO_I2S_CLK, AR934X_GPIO_OUT_MUX_I2S_CLK);
	gpio_direction_output(CUS227_GPIO_I2S_CLK, 0);

	gpio_request(CUS227_GPIO_I2S_WS, "I2S WS");
	ath79_gpio_output_select(CUS227_GPIO_I2S_WS, AR934X_GPIO_OUT_MUX_I2S_WS);
	gpio_direction_output(CUS227_GPIO_I2S_WS, 0);

	gpio_request(CUS227_GPIO_I2S_SD, "I2S SD");
	ath79_gpio_output_select(CUS227_GPIO_I2S_SD, AR934X_GPIO_OUT_MUX_I2S_SD);
	gpio_direction_output(CUS227_GPIO_I2S_SD, 0);

	gpio_request(CUS227_GPIO_I2S_MCLK, "I2S MCLK");
	ath79_gpio_output_select(CUS227_GPIO_I2S_MCLK, AR934X_GPIO_OUT_MUX_I2S_MCK);
	gpio_direction_output(CUS227_GPIO_I2S_MCLK, 0);

	gpio_request(CUS227_GPIO_SPDIF_OUT, "SPDIF OUT");
	ath79_gpio_output_select(CUS227_GPIO_SPDIF_OUT, AR934X_GPIO_OUT_MUX_SPDIF_OUT);
	gpio_direction_output(CUS227_GPIO_SPDIF_OUT, 0);

	gpio_request(CUS227_GPIO_I2S_MIC_SD, "I2S MIC_SD");
	ath79_gpio_input_select(CUS227_GPIO_I2S_MIC_SD, AR934X_GPIO_IN_MUX_I2S_MIC_SD);
	gpio_direction_input(CUS227_GPIO_I2S_MIC_SD);

	/* Init stereo block registers in default configuration */
	ath79_audio_setup();
}

static void __init cus227_register_i2c_devices(
			struct i2c_board_info const *info)
{
	ath79_register_i2c(&ath79_i2c_gpio_data, info, info?1:0);
}

static void __init cus227_register_spi_devices(
			struct spi_board_info const *info)
{
	gpio_request(CUS227_GPIO_SPI_CLK, "SPI CLK");
	ath79_gpio_output_select(CUS227_GPIO_SPI_CLK, AR934X_GPIO_OUT_MUX_SPI_CLK);
	gpio_direction_output(CUS227_GPIO_SPI_CLK, 0);

	gpio_request(CUS227_GPIO_SPI_MOSI, "SPI MOSI");
	ath79_gpio_output_select(CUS227_GPIO_SPI_MOSI, AR934X_GPIO_OUT_MUX_SPI_MOSI);
	gpio_direction_output(CUS227_GPIO_SPI_MOSI, 0);

	gpio_request(CUS227_GPIO_SPI_CS1, "SPI CS1");
	ath79_gpio_output_select(CUS227_GPIO_SPI_CS1, AR934X_GPIO_OUT_MUX_SPI_CS1);
	gpio_direction_output(CUS227_GPIO_SPI_CS1, 0);

	/* a dedicated GPIO pin is used as SPI MISO since SPI controller doesn't support modes other than mode-0  */
	gpio_request(CUS227_GPIO_SPI_MISO, "SPI MISO");
	ath79_gpio_input_select(CUS227_GPIO_SPI_MISO, AR934X_GPIO_IN_MUX_SPI_MISO);
	gpio_direction_input(CUS227_GPIO_SPI_MISO);

	ath79_spi_data.bus_num = 0;
	ath79_spi_data.num_chipselect = 2;
	ath79_spi_data.miso_line = CUS227_GPIO_SPI_MISO;
	ath79_register_spi(&ath79_spi_data, info, 1);
}


static void __init cus227_setup(void)
{
	of_platform_bus_probe(NULL, cus227_common_ids, NULL);

	ath79_register_usb();

	ath79_register_nand();
	ath79_mtd_caldata_fixup(&cus227_caldata);

	/* just define GPIO based S/W I2C master device now */
	/* later I2C slave devices can be added per platform defined */
	cus227_register_i2c_devices(NULL);

	cus227_register_spi_devices(cus227_spi_info);

	ath79_register_wmac(NULL, NULL);

	/* GMAC1 is connected to the internal switch */
	ath79_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;
	ath79_eth1_data.speed = SPEED_1000;
	ath79_eth1_data.duplex = DUPLEX_FULL;
	ath79_register_mdio(1, 0x0);
	ath79_register_eth(1);

	/* Audio initialization: PCM/I2S and CODEC */
	cus227_audio_setup();
	ath79_audio_device_register();

}
MIPS_MACHINE(ATH79_MACH_CUS227, "CUS227", "Qualcomm Atheros CUS227",
	     cus227_setup);
