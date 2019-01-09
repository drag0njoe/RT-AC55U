/* 
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 */

#ifndef __BOARD_955X_H
#define __BOARD_955X_H

#include <config.h>

#undef MTDPARTS_DEFAULT

#undef CFG_HZ

//#define RTAC55U_SR1

#include <atheros.h>

/*-----------------------------------------------------------------------
 * Model name, bootloader version
 */
#define ASUS_BLVER			"1012"

#ifndef __ASSEMBLY__
/* board955x.c */
extern const char *model;
extern const char *blver;
extern const char *bl_stage;

/* board.c */
extern int modifies;
#endif

/*-----------------------------------------------------------------------
 * Factory
 */
#define RAMAC0_OFFSET	0x1002	// eth0 is same as 2.4G
#define RAMAC1_OFFSET	0x5006	// eth1 is same as 5G

#define RFCAL_FLAG_OFFSET	0xD00A	/* size: 4 bytes */
#define RFCAL_READY		"LSDK"	/* RF caldata is ready and can be copied to factory. */
#define RFCAL_COPIED		"RFCA"	/* If RF caldata have been copied to factory successful, RFCAL_FLAG_OFFSET of factory must be updated to this magic number. */

/*-----------------------------------------------------------------------
 * Bootloader size and Config size definitions
 */
#if defined(CONFIG_ATH_NAND_BR)
#if defined(UBI_SUPPORT)
#define CFG_BOOTLOADER_SIZE		0xE0000			/* 2 x 448KiB bootloader  = 896k */
#define CFG_MAX_BOOTLOADER_BINARY_SIZE	0x70000			/* bootloader size is limited under 448KiB */
#define CFG_CONFIG_SIZE			0x20000
#else
#define CFG_BOOTLOADER_SIZE		0x40000			/* 256KiB bootloader */
#define CFG_MAX_BOOTLOADER_BINARY_SIZE	0x40000			/* bootloader size is limited under 256KiB */
#define CFG_CONFIG_SIZE			0x10000
#endif
#else
#define CFG_BOOTLOADER_SIZE		0x40000
#define CFG_MAX_BOOTLOADER_BINARY_SIZE	CFG_BOOTLOADER_SIZE
#define CFG_CONFIG_SIZE			0x10000
#endif	/* CONFIG_ATH_NAND_BR */

#if defined(UBI_SUPPORT)
#define MTDIDS			"nand0=nand0"
/* Keep Bootloader size and environment size equal to CFG_BOOTLOADER_SIZE and CFG_CONFIG_SIZE respectively. */
#define MTDPARTS		"mtdparts=nand0:896k(Bootloader),128k(environment),126m(UBI_DEV),1m(caldata)"

/*
 * UBI volume size definitions
 * Don't define size for tailed reserved space due to it's size varies.
 */
#define PEB_SIZE			(128 * 1024)
#define LEB_SIZE			(PEB_SIZE - (2 * 2 * 1024))
#define CFG_UBI_NVRAM_NR_LEB		1
#define CFG_UBI_FACTORY_NR_LEB		2
#define CFG_UBI_FIRMWARE_NR_LEB		397	/* 124KB x 397 = 48.074MB */
#define CFG_UBI_FIRMWARE2_NR_LEB	397

#if defined(LSDK_NART_SUPPORT)
#define CFG_UBI_LSDKRFCAL_NR_LEB	42
#endif
#define CFG_UBI_APP_NR_LEB		1	/* relay on auto-size feature */

#define CFG_UBI_NVRAM_SIZE		(LEB_SIZE * CFG_UBI_NVRAM_NR_LEB)
#define CFG_UBI_FACTORY_SIZE		(LEB_SIZE * CFG_UBI_FACTORY_NR_LEB)
#define CFG_UBI_FACTORY2_SIZE		(LEB_SIZE * CFG_UBI_FACTORY_NR_LEB)
#define CFG_UBI_FIRMWARE_SIZE		(LEB_SIZE * CFG_UBI_FIRMWARE_NR_LEB)
#define CFG_UBI_FIRMWARE2_SIZE		(LEB_SIZE * CFG_UBI_FIRMWARE2_NR_LEB)
#define CFG_UBI_LSDKRFCAL_SIZE		(LEB_SIZE * CFG_UBI_LSDKRFCAL_NR_LEB)
#define CFG_UBI_APP_SIZE		(LEB_SIZE * CFG_UBI_APP_NR_LEB)

#define CFG_NVRAM_SIZE			CFG_UBI_NVRAM_SIZE

#define CFG_FACTORY_SIZE		(CFG_UBI_FACTORY_SIZE + CFG_UBI_FACTORY2_SIZE)
#else	/* !UBI_SUPPORT */
#define CFG_FACTORY_SIZE		0x10000
#endif	/* UBI_SUPPORT */

#define CFG_UBI_DEV_OFFSET	(CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE)

/* Environment address, factory address, and firmware address definitions */
#if defined(CONFIG_ATH_NAND_BR)
/* Basically, CFG_FACTORY_ADDR and CFG_KERN_ADDR are used to compatible to original code infrastructure.
 * Real nvram area would be moved into the nvram volume of UBI device.
 * Real Factory area would be moved into the Factory volume of UBI device.
 * Real firmware area would be moved into the linux and linux2 volume of UBI device.
 */
#if defined(UBI_SUPPORT)
#define CFG_ENV_ADDR		(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE)
#define CFG_FACTORY_ADDR	(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_NVRAM_SIZE)
#define CFG_KERN_ADDR		(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_NVRAM_SIZE + CFG_FACTORY_SIZE))
#define CFG_KERN2_ADDR		(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_NVRAM_SIZE + CFG_FACTORY_SIZE + CFG_UBI_FIRMWARE_SIZE))

#if defined(LSDK_NART_SUPPORT)
#define CFG_LSDKNART_ADDR	(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_NVRAM_SIZE + CFG_FACTORY_SIZE + CFG_UBI_FIRMWARE_SIZE + CFG_UBI_FIRMWARE2_SIZE))
#define RFCALDATA_REGION_SIZE	0x100000
#endif

#else /* Pure NAND Flash */
#define CFG_ENV_ADDR		(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE)
#define CFG_FACTORY_ADDR	(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE)
#define CFG_KERN_ADDR		(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_FACTORY_SIZE))
#endif
#else /* SPI NOR Flash */
#define CFG_ENV_ADDR		(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE)
#define CFG_FACTORY_ADDR	(CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE)
#define CFG_KERN_ADDR		(CFG_FLASH_BASE + (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_FACTORY_SIZE))
#endif
/*-----------------------------------------------------------------------*/


#ifndef FLASH_SIZE
#define FLASH_SIZE 8
#endif

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */
#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks */
#if (FLASH_SIZE == 16)
#define CFG_MAX_FLASH_SECT	256	/* max number of sectors on one chip */
#elif (FLASH_SIZE == 8)
#define CFG_MAX_FLASH_SECT	128	/* max number of sectors on one chip */
#else
#define CFG_MAX_FLASH_SECT	64	/* max number of sectors on one chip */
#endif

#define CFG_FLASH_SECTOR_SIZE	(64*1024)
#if (FLASH_SIZE == 16)
#define CFG_FLASH_SIZE		0x01000000	/* Total flash size */
#elif (FLASH_SIZE == 8)
#define CFG_FLASH_SIZE		0x00800000	/* max number of sectors on one chip */
#else
#define CFG_FLASH_SIZE		0x00400000	/* Total flash size */
#endif

#define SHRINK_UBOOT		/* Similar to COMPRESSED_UBOOT */

#ifndef COMPRESSED_UBOOT
#define ENABLE_DYNAMIC_CONF	1
#endif

#undef CFG_ATHRS26_PHY

#if (CFG_MAX_FLASH_SECT * CFG_FLASH_SECTOR_SIZE) != CFG_FLASH_SIZE
#	error "Invalid flash configuration"
#endif

#define CFG_FLASH_WORD_SIZE	unsigned short

#if defined(CONFIG_ATH_NAND_BR) && defined(COMPRESSED_UBOOT)
#define CFG_FLASH_BASE			0xa0100000
#else
/* NOR Flash start address */
#define CFG_FLASH_BASE			0x9f000000
#endif

#ifdef COMPRESSED_UBOOT
#define BOOTSTRAP_TEXT_BASE		CFG_FLASH_BASE
#define BOOTSTRAP_CFG_MONITOR_BASE	BOOTSTRAP_TEXT_BASE
#endif

#define CONFIG_PCI_CONFIG_DATA_IN_OTP

/*
 * Defines to change flash size on reboot
 */
#ifdef ENABLE_DYNAMIC_CONF
#define UBOOT_FLASH_SIZE		(256 * 1024)
#define UBOOT_ENV_SEC_START		(CFG_FLASH_BASE + UBOOT_FLASH_SIZE)

#define CFG_FLASH_MAGIC			0xaabacada
#define CFG_FLASH_MAGIC_F		(UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x20)
#define CFG_FLASH_SECTOR_SIZE_F		*(volatile int *)(CFG_FLASH_MAGIC_F + 0x4)
#define CFG_FLASH_SIZE_F		*(volatile int *)(CFG_FLASH_MAGIC_F + 0x8) /* Total flash size */
#define CFG_MAX_FLASH_SECT_F		(CFG_FLASH_SIZE / CFG_FLASH_SECTOR_SIZE) /* max number of sectors on one chip */
#else
#define CFG_FLASH_SIZE_F		CFG_FLASH_SIZE
#define CFG_FLASH_SECTOR_SIZE_F		CFG_FLASH_SECTOR_SIZE
#endif


#define CFG_DDR_REFRESH_VAL		0x4138

/*
 * The following #defines are needed to get flash environment right
 */
#define	CFG_MONITOR_BASE	TEXT_BASE
#define	CFG_MONITOR_LEN		(192 << 10)

#undef CONFIG_BOOTARGS

#define CONFIG_EXTRA_ENV_SETTINGS	\
	"preferred_nic=eth1\0"	\
	""

/*
 * timeout values are in ticks
 */
#define CFG_FLASH_ERASE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(2 * CFG_HZ) /* Timeout for Flash Write */

/*
 * Cache lock for stack
 */
#define CFG_INIT_SP_OFFSET	0x1000
#define CFG_INIT_SRAM_SP_OFFSET	0xbd007000

#ifdef CONFIG_ATH_NAND_SUPPORT
#if defined(UBI_SUPPORT)
#	define CONFIG_BOOTCOMMAND	"tftp"
#else
#	define CONFIG_BOOTCOMMAND	"tftp"
#endif
#else
#	define CONFIG_BOOTCOMMAND	"tftp"
#endif



#ifdef ENABLE_DYNAMIC_CONF
#define CFG_DDR_MAGIC		0xaabacada
#define CFG_DDR_MAGIC_F		(UBOOT_ENV_SEC_START + CFG_FLASH_SECTOR_SIZE - 0x30)
#define CFG_DDR_CONFIG_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 4)
#define CFG_DDR_CONFIG2_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 8)
#define CFG_DDR_EXT_MODE_VAL_F	*(volatile int *)(CFG_DDR_MAGIC_F + 12)
#endif

#define CONFIG_NET_MULTI
#define CONFIG_MEMSIZE_IN_BYTES
#define CONFIG_PCI 1

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#ifndef COMPRESSED_UBOOT
#define ATH_CFG_COMMANDS	((			\
				CONFIG_CMD_DFL	|	\
				CFG_CMD_DHCP	|	\
				CFG_CMD_MII	|	\
				CFG_CMD_PING	|	\
				CFG_CMD_NET	|	\
				CFG_CMD_ENV	|	\
				CFG_CMD_RUN		\
				) & ~(			\
				CFG_CMD_AUTOSCRIPT|	\
				CFG_CMD_BDI	|	\
				CFG_CMD_CONSOLE	|	\
				CFG_CMD_DDR	|	\
				CFG_CMD_ELF	|	\
				CFG_CMD_ETHREG	|	\
				CFG_CMD_FLASH	|	\
				CFG_CMD_IMLS	|	\
				CFG_CMD_IMI	|	\
				CFG_CMD_ITEST	|	\
				CFG_CMD_MISC	|	\
				CFG_CMD_NFS	|	\
				CFG_CMD_PLL	|	\
				CFG_CMD_RUN		\
				))
#else
#	ifdef CONFIG_ATH_NAND_BR
#		define ATH_CFG_COMMANDS		((			\
						CONFIG_CMD_DFL	|	\
						CFG_CMD_PING	|	\
						CFG_CMD_NET) & ~(	\
						CFG_CMD_FLASH		\
						))
#	else
#		define ATH_CFG_COMMANDS		(CONFIG_CMD_DFL	|	\
				CFG_CMD_PING	|	\
				CFG_CMD_NET)
#	endif
#endif /* #ifndef COMPRESSED_UBOOT */

#ifdef CONFIG_ATH_NAND_SUPPORT
#	ifdef CONFIG_ATH_NAND_BR
#		define CFG_ENV_IS_IN_NAND	1
#		define CFG_ENV_OFFSET		CFG_BOOTLOADER_SIZE
#		define CFG_ENV_SIZE		CFG_CONFIG_SIZE
#		define ATH_EXTRA_CMD		CFG_CMD_NAND
#	else
#		define CFG_ENV_IS_IN_FLASH	1
#		define CFG_ENV_SIZE		CFG_FLASH_SECTOR_SIZE
#		define ATH_EXTRA_CMD		(CFG_CMD_NAND | CFG_CMD_FLASH)
#	endif
#	define NAND_MAX_CHIPS			1
#	define CFG_MAX_NAND_DEVICE		1
#else
#	define ATH_EXTRA_CMD			CFG_CMD_FLASH
#	define CFG_ENV_IS_IN_FLASH		1
#	define CFG_ENV_SIZE			CFG_FLASH_SECTOR_SIZE
#endif

#ifdef COMPRESSED_UBOOT
#undef  CFG_ENV_IS_IN_FLASH
#undef  CFG_ENV_IS_IN_NAND
#define CFG_ENV_IS_NOWHERE		1
#endif

#define CONFIG_COMMANDS			((ATH_CFG_COMMANDS | ATH_EXTRA_CMD | CFG_CMD_LOADB) & ~(CFG_CMD_LOADS))

#define CONFIG_IPADDR			192.168.1.1
#define CONFIG_SERVERIP			192.168.1.10
#define CONFIG_ETHADDR			0x00:0xaa:0xbb:0xcc:0xdd:0xee
#define CFG_FAULT_ECHO_LINK_DOWN	1

#define CFG_PHY_ADDR			0
#define CFG_GMII			0
#define CFG_MII0_RMII			1
#define CFG_AG7100_GE0_RMII		1

#define CFG_BOOTM_LEN			(32 << 20) /* 32 MB */

/*
** Parameters defining the location of the calibration/initialization
** information for the two Merlin devices.
** NOTE: **This will change with different flash configurations**
*/

#define WLANCAL				0x00001000	/* Factory offset address */
#define BOARDCAL			0x00000000	/* Factory offset address */
#define ATHEROS_PRODUCT_ID		137
#define CAL_SECTOR			(CFG_MAX_FLASH_SECT - 1)

/* For Merlin, both PCI, PCI-E interfaces are valid */
#define ATH_ART_PCICFG_OFFSET		12

#include <cmd_confdefs.h>

#endif	/* __BOARD_955X_H */
