include $(SRCBASE)/.config

ifeq ($(MESH),y)
export LINUXDIR := $(SRCBASE)/linux/linux-3.3.x.mesh
else
export LINUXDIR := $(SRCBASE)/linux/linux-3.3.x
endif

ifeq ($(EXTRACFLAGS),)
export EXTRACFLAGS := -DBCMWPA2 -fno-delete-null-pointer-checks -mips32 -mtune=mips32
endif

export BUILD := $(shell (gcc -dumpmachine))
export KERNEL_BINARY=$(LINUXDIR)/vmlinux
export PLATFORM := mips-uclibc
export PLATFORM_ARCH := mips-uclibc
export CROSS_COMPILE := mips-openwrt-linux-uclibc-
export CROSS_COMPILER := $(CROSS_COMPILE)
export READELF := mips-openwrt-linux-uclibc-readelf
export CONFIGURE := ./configure --host=mips-linux --build=$(BUILD)
export HOSTCONFIG := linux-mips
export ARCH := mips
export HOST := mips-linux
export KERNELCC := $(CROSS_COMPILE)gcc
export KERNELLD := $(CROSS_COMPILE)ld
export TOOLS := /opt/openwrt-gcc463.mips
export RTVER := 0.9.33.2

# Kernel load address and entry address
export LOADADDR := 80060000
export ENTRYADDR := $(LOADADDR)

# OpenWRT's toolchain needs STAGING_DIR environment variable that points to top directory of toolchain.
export STAGING_DIR=$(shell which mips-openwrt-linux-gcc|sed -e "s,/bin/mips-openwrt-linux-gcc,,")

EXTRA_CFLAGS := -DLINUX26 -DCONFIG_QCA -pipe -DDEBUG_NOISY -DDEBUG_RCTEST

export CONFIG_LINUX26=y
export CONFIG_QCA=y

# related path to platform specific software package
export PLATFORM_ROUTER := $(if $(MESH),qca95xx.mesh)

EXTRA_CFLAGS += -DLINUX30
export CONFIG_LINUX30=y

define platformRouterOptions
	@( \
	if [ "$(QCA)" = "y" ]; then \
		sed -i "/RTCONFIG_QCA\>/d" $(1); \
		echo "RTCONFIG_QCA=y" >>$(1); \
		sed -i "/RTCONFIG_QCA_ARM/d" $(1); \
		echo "# RTCONFIG_QCA_ARM is not set" >>$(1); \
		sed -i "/RTCONFIG_FITFDT/d" $(1); \
		echo "# RTCONFIG_FITFDT is not set" >>$(1); \
		if [ "$(QCA953X)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA953X/d" $(1); \
			echo "RTCONFIG_QCA953X=y" >>$(1); \
		fi; \
		if [ "$(QCA955X)" = "y" ]; then \
			sed -i "/RTCONFIG_SOC_QCA9557/d" $(1); \
			echo "RTCONFIG_SOC_QCA9557=y" >>$(1); \
			sed -i "/RTCONFIG_WIFI_QCA9557_QCA9882/d" $(1); \
			echo "RTCONFIG_WIFI_QCA9557_QCA9882=y" >>$(1); \
		fi; \
		if [ "$(QCA956X)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA956X/d" $(1); \
			echo "RTCONFIG_QCA956X=y" >>$(1); \
			if [ "$(PLAC56)" = "y" ]; then \
				sed -i "/RTCONFIG_PCIE_QCA9882/d" $(1); \
				echo "RTCONFIG_PCIE_QCA9882=y" >>$(1); \
			elif [ "$(PLAC66U)" = "y" ] || [ "$(RPAC66)" = "y" ] || [ "$(MAPAC1750)" = "y" ]; then \
				sed -i "/RTCONFIG_PCIE_QCA9880/d" $(1); \
				echo "RTCONFIG_PCIE_QCA9880=y" >>$(1); \
			fi; \
		fi; \
		if [ "$(PLC_UTILS)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA_PLC_UTILS/d" $(1); \
			echo "RTCONFIG_QCA_PLC_UTILS=y" >>$(1); \
		fi; \
		if [ "$(AR7420)" = "y" ]; then \
			sed -i "/RTCONFIG_AR7420/d" $(1); \
			echo "RTCONFIG_AR7420=y" >>$(1); \
		fi; \
		if [ "$(QCA7500)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA7500/d" $(1); \
			echo "RTCONFIG_QCA7500=y" >>$(1); \
		fi; \
		if [ "$(QCA8033)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA8033/d" $(1); \
			echo "RTCONFIG_QCA8033=y" >>$(1); \
		fi; \
		if [ "$(ART2)" = "y" ]; then \
			sed -i "/RTCONFIG_ART2_BUILDIN/d" $(1); \
			echo "RTCONFIG_ART2_BUILDIN=y" >>$(1); \
		fi; \
		if [ "$(QCA_VAP_LOCALMAC)" = "y" ]; then \
			sed -i "/RTCONFIG_QCA_VAP_LOCALMAC/d" $(1); \
			echo "RTCONFIG_QCA_VAP_LOCALMAC=y" >>$(1); \
		fi; \
	fi; \
	)
endef

define platformBusyboxOptions
	@( \
	if [ "$(QCA)" = "y" ]; then \
		sed -i "/CONFIG_MDMM/d" $(1); \
		echo "CONFIG_MDMM=y" >>$(1); \
		sed -i "/CONFIG_ETHREG/d" $(1); \
		echo "CONFIG_ETHREG=y" >>$(1); \
		sed -i "/CONFIG_TFTP /d" $(1); \
		echo "CONFIG_TFTP=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TFTP_GET/d" $(1); \
		echo "CONFIG_FEATURE_TFTP_GET=y" >>$(1); \
		sed -i "/CONFIG_FEATURE_TFTP_PUT/d" $(1); \
		echo "CONFIG_FEATURE_TFTP_PUT=y" >>$(1); \
		if [ "$(MAPAC1750)" = "y" ]; then \
			sed -i "/CONFIG_TELNET\b/d" $(1); \
			echo "CONFIG_TELNET=y" >>$(1); \
		fi; \
	fi; \
	)
endef

BOOT_FLASH_TYPE_POOL =	\
	"NOR"		\
	"SPI"		\
	"NAND"


define platformKernelConfig
	@( \
	if [ "$(QCA)" = "y" ]; then \
		if [ "$(PLN12)" = "y" ]; then \
			sed -i "/CONFIG_ATH79_MACH_AP143/d" $(1); \
			echo "CONFIG_ATH79_MACH_AP143=y" >>$(1); \
			sed -i "/CONFIG_PLN12/d" $(1); \
			echo "CONFIG_PLN12=y" >>$(1); \
		fi; \
		if [ "$(RTAC55U)" = "y" ] || [ "$(RTAC55UHP)" = "y" ] || [ "$(RT4GAC55U)" = "y" ]; then \
			sed -i "/CONFIG_ATH79_MACH_AP135/d" $(1); \
			echo "CONFIG_ATH79_MACH_AP135=y" >>$(1); \
			echo "# CONFIG_ATH79_MACH_AP135_DUAL is not set" >>$(1); \
			sed -i "/CONFIG_RTAC55U/d" $(1); \
			echo "CONFIG_RTAC55U=y" >>$(1); \
		fi; \
		if [ "$(PLAC56)" = "y" ] || [ "$(PLAC66U)" = "y" ] || [ "$(RPAC66)" = "y" ] || [ "$(MAPAC1750)" = "y" ]; then \
			sed -i "/CONFIG_ATH79_MACH_AP152/d" $(1); \
			echo "CONFIG_ATH79_MACH_AP152=y" >>$(1); \
			if [ "$(PLAC56)" = "y" ]; then \
				sed -i "/CONFIG_PLAC56/d" $(1); \
				echo "CONFIG_PLAC56=y" >>$(1); \
			fi; \
			if [ "$(PLAC66U)" = "y" ]; then \
				sed -i "/CONFIG_PLAC66U/d" $(1); \
				echo "CONFIG_PLAC66U=y" >>$(1); \
			fi; \
			if [ "$(RPAC66)" = "y" ]; then \
				sed -i "/CONFIG_RPAC66/d" $(1); \
				echo "CONFIG_RPAC66=y" >>$(1); \
			fi; \
			if [ "$(MAPAC1750)" = "y" ]; then \
				sed -i "/CONFIG_MAPAC1750/d" $(1); \
				echo "CONFIG_MAPAC1750=y" >>$(1); \
			fi; \
		fi; \
		if [ "$(CONFIG_LINUX30)" = "y" ]; then \
			for bftype in $(BOOT_FLASH_TYPE_POOL) ; do \
				sed -i "/CONFIG_MTD_$${bftype}_RALINK\>/d" $(1); \
				if [ "$(BOOT_FLASH_TYPE)" = "$${bftype}" ] ; then \
					echo "CONFIG_MTD_$${bftype}_RALINK=y" >> $(1); \
				else \
					echo "# CONFIG_MTD_$${bftype}_RALINK is not set" >> $(1); \
				fi; \
			done; \
			sed -i "/CONFIG_MTD_ANY_RALINK/d" $(1); \
			echo "# CONFIG_MTD_ANY_RALINK is not set" >>$(1); \
			sed -i "/CONFIG_BRIDGE_EBT_ARPNAT/d" $(1); \
			if [ "$(MAKE_HIVEDOT)" = "y" ] || [ "$(MAKE_HIVESPOT)" = "y" ]; then \
				echo "# CONFIG_BRIDGE_EBT_ARPNAT=m" >>$(1); \
			else \
				echo "# CONFIG_BRIDGE_EBT_ARPNAT is not set" >>$(1); \
			fi; \
			sed -i "/CONFIG_NF_CONNTRACK_EVENTS/d" $(1); \
			echo "CONFIG_NF_CONNTRACK_EVENTS=y" >>$(1); \
			sed -i "/CONFIG_NF_CONNTRACK_CHAIN_EVENTS/d" $(1); \
			echo "# CONFIG_NF_CONNTRACK_CHAIN_EVENTS is not set" >>$(1); \
		fi; \
	fi; \
	if [ "$(JFFS2)" = "y" ]; then \
		if [ "$(CONFIG_LINUX26)" = "y" ]; then \
			sed -i "/CONFIG_JFFS2_FS/d" $(1); \
			echo "CONFIG_JFFS2_FS=m" >>$(1); \
			sed -i "/CONFIG_JFFS2_FS_DEBUG/d" $(1); \
			echo "CONFIG_JFFS2_FS_DEBUG=0" >>$(1); \
			sed -i "/CONFIG_JFFS2_FS_WRITEBUFFER/d" $(1); \
			echo "CONFIG_JFFS2_FS_WRITEBUFFER=y" >>$(1); \
			sed -i "/CONFIG_JFFS2_SUMMARY/d" $(1); \
			echo "# CONFIG_JFFS2_SUMMARY is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_FS_XATTR/d" $(1); \
			echo "# CONFIG_JFFS2_FS_XATTR is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_COMPRESSION_OPTIONS/d" $(1); \
			echo "CONFIG_JFFS2_COMPRESSION_OPTIONS=y" >>$(1); \
			sed -i "/CONFIG_JFFS2_ZLIB/d" $(1); \
			echo "CONFIG_JFFS2_ZLIB=y" >>$(1); \
			sed -i "/CONFIG_JFFS2_LZO/d" $(1); \
			echo "# CONFIG_JFFS2_LZO is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_LZMA/d" $(1); \
			echo "# CONFIG_JFFS2_LZMA is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_RTIME/d" $(1); \
			echo "# CONFIG_JFFS2_RTIME is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_RUBIN/d" $(1); \
			echo "# CONFIG_JFFS2_RUBIN is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_CMODE_NONE/d" $(1); \
			echo "# CONFIG_JFFS2_CMODE_NONE is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_CMODE_PRIORITY/d" $(1); \
			echo "CONFIG_JFFS2_CMODE_PRIORITY=y" >>$(1); \
			sed -i "/CONFIG_JFFS2_CMODE_SIZE/d" $(1); \
			echo "# CONFIG_JFFS2_CMODE_SIZE is not set" >>$(1); \
		fi; \
		if [ "$(CONFIG_LINUX30)" = "y" ]; then \
			sed -i "/CONFIG_JFFS2_FS_WBUF_VERIFY/d" $(1); \
			echo "# CONFIG_JFFS2_FS_WBUF_VERIFY is not set" >>$(1); \
			sed -i "/CONFIG_JFFS2_CMODE_FAVOURLZO/d" $(1); \
			echo "# CONFIG_JFFS2_CMODE_FAVOURLZO is not set" >>$(1); \
		fi; \
	else \
		sed -i "/CONFIG_JFFS2_FS/d" $(1); \
		echo "# CONFIG_JFFS2_FS is not set" >>$(1); \
	fi; \
	if [ "$(UBI)" = "y" ]; then \
		sed -i "/CONFIG_MTD_UBI\>/d" $(1); \
		echo "CONFIG_MTD_UBI=y" >>$(1); \
		sed -i "/CONFIG_MTD_UBI_WL_THRESHOLD/d" $(1); \
		echo "CONFIG_MTD_UBI_WL_THRESHOLD=4096" >>$(1); \
		sed -i "/CONFIG_MTD_UBI_BEB_RESERVE/d" $(1); \
		echo "CONFIG_MTD_UBI_BEB_RESERVE=1" >>$(1); \
		sed -i "/CONFIG_MTD_UBI_GLUEBI/d" $(1); \
		echo "CONFIG_MTD_UBI_GLUEBI=y" >>$(1); \
		sed -i "/CONFIG_FACTORY_CHECKSUM/d" $(1); \
		echo "CONFIG_FACTORY_CHECKSUM=y" >>$(1); \
		if [ "$(UBI_DEBUG)" = "y" ]; then \
			sed -i "/CONFIG_MTD_UBI_DEBUG/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG=y" >>$(1); \
			sed -i "/CONFIG_GCOV_KERNEL/d" $(1); \
			echo "# CONFIG_GCOV_KERNEL is not set" >>$(1); \
			sed -i "/CONFIG_L2TP_DEBUGFS/d" $(1); \
			echo "# CONFIG_L2TP_DEBUGFS is not set" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_MSG/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_MSG=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_PARANOID/d" $(1); \
			echo "# CONFIG_MTD_UBI_DEBUG_PARANOID is not set" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_DISABLE_BGT/d" $(1); \
			echo "# CONFIG_MTD_UBI_DEBUG_DISABLE_BGT is not set" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_EMULATE_BITFLIPS/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_EMULATE_BITFLIPS=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_EMULATE_WRITE_FAILURES/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_EMULATE_WRITE_FAILURES=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_EMULATE_ERASE_FAILURES/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_EMULATE_ERASE_FAILURES=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_MSG_BLD/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_MSG_BLD=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_MSG_EBA/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_MSG_EBA=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_MSG_WL/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_MSG_WL=y" >>$(1); \
			sed -i "/CONFIG_MTD_UBI_DEBUG_MSG_IO/d" $(1); \
			echo "CONFIG_MTD_UBI_DEBUG_MSG_IO=y" >>$(1); \
			sed -i "/CONFIG_JBD_DEBUG/d" $(1); \
			echo "# CONFIG_JBD_DEBUG is not set" >>$(1); \
			sed -i "/CONFIG_LKDTM/d" $(1); \
			echo "# CONFIG_LKDTM is not set" >>$(1); \
			sed -i "/CONFIG_DYNAMIC_DEBUG/d" $(1); \
			echo "CONFIG_DYNAMIC_DEBUG=y" >>$(1); \
			sed -i "/CONFIG_SPINLOCK_TEST/d" $(1); \
			echo "# CONFIG_SPINLOCK_TEST is not set" >>$(1); \
		else \
			sed -i "/CONFIG_MTD_UBI_DEBUG/d" $(1); \
			echo "# CONFIG_MTD_UBI_DEBUG is not set" >>$(1); \
		fi; \
		if [ "$(UBIFS)" = "y" ]; then \
			sed -i "/CONFIG_UBIFS_FS/d" $(1); \
			echo "CONFIG_UBIFS_FS=y" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_XATTR/d" $(1); \
			echo "# CONFIG_UBIFS_FS_XATTR is not set" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_ADVANCED_COMPR/d" $(1); \
			echo "CONFIG_UBIFS_FS_ADVANCED_COMPR=y" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_LZO/d" $(1); \
			echo "CONFIG_UBIFS_FS_LZO=y" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_ZLIB/d" $(1); \
			echo "CONFIG_UBIFS_FS_ZLIB=y" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_XZ/d" $(1); \
			echo "CONFIG_UBIFS_FS_XZ=y" >>$(1); \
			sed -i "/CONFIG_UBIFS_FS_DEBUG/d" $(1); \
			echo "# CONFIG_UBIFS_FS_DEBUG is not set" >>$(1); \
		else \
			sed -i "/CONFIG_UBIFS_FS/d" $(1); \
			echo "# CONFIG_UBIFS_FS is not set" >>$(1); \
		fi; \
	else \
		sed -i "/CONFIG_ATH79_DEV_SPI/d" $(1); \
		echo "CONFIG_ATH79_DEV_SPI=y" >>$(1); \
		sed -i "/CONFIG_ATH79_DEV_NAND/d" $(1); \
		echo "# CONFIG_ATH79_DEV_NAND is not set" >>$(1); \
	fi; \
	if [ "$(DUMP_OOPS_MSG)" = "y" ]; then \
		echo "CONFIG_DUMP_PREV_OOPS_MSG=y" >>$(1); \
		echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_ADDR=0x45300000" >>$(1); \
		echo "CONFIG_DUMP_PREV_OOPS_MSG_BUF_LEN=0x2000" >>$(1); \
	fi; \
	if [ "$(BT_CONN)" = "y" ] ; then \
		sed -i "/CONFIG_BT/d" $(1); \
		echo "CONFIG_BT=y" >>$(1); \
		sed -i "/CONFIG_BT_HCIBTUSB/d" $(1); \
		echo "CONFIG_BT_HCIBTUSB=m" >>$(1); \
		sed -i "/CONFIG_BT_RFCOMM/d" $(1); \
		echo "CONFIG_BT_RFCOMM=y" >>$(1); \
		sed -i "/CONFIG_BT_RFCOMM_TTY/d" $(1); \
		echo "CONFIG_BT_RFCOMM_TTY=y" >>$(1); \
		sed -i "/CONFIG_BT_BNEP/d" $(1); \
		echo "CONFIG_BT_BNEP=y" >>$(1); \
		sed -i "/CONFIG_BT_BNEP_MC_FILTER/d" $(1); \
		echo "CONFIG_BT_BNEP_MC_FILTER=y" >>$(1); \
		sed -i "/CONFIG_BT_BNEP_PROTO_FILTER/d" $(1); \
		echo "CONFIG_BT_BNEP_PROTO_FILTER=y" >>$(1); \
		sed -i "/CONFIG_BT_HIDP/d" $(1); \
		echo "# CONFIG_BT_HIDP is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIUART/d" $(1); \
		echo "# CONFIG_BT_HCIUART is not set" >>$(1); \
		echo "# CONFIG_BT_HCIUART_H4 is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIUART_BCSP/d" $(1); \
		echo "# CONFIG_BT_HCIUART_BCSP is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIBCM203X/d" $(1); \
		echo "# CONFIG_BT_HCIBCM203X is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIBPA10X/d" $(1); \
		echo "# CONFIG_BT_HCIBPA10X is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIBFUSB/d" $(1); \
		echo "# CONFIG_BT_HCIBFUSB is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIUART_ATH3K/d" $(1); \
		echo "# CONFIG_BT_HCIUART_ATH3K is not set" >>$(1); \
		sed -i "/CONFIG_BT_ATH3K/d" $(1); \
		echo "CONFIG_BT_ATH3K=m" >>$(1); \
		echo "# CONFIG_BT_HCIUART_LL is not set" >>$(1); \
		echo "# CONFIG_BT_HCIUART_3WIRE is not set" >>$(1); \
		sed -i "/CONFIG_BT_HCIVHCI/d" $(1); \
		echo "CONFIG_BT_HCIVHCI=y" >>$(1); \
		echo "# CONFIG_BT_MRVL is not set" >>$(1); \
		echo "# CONFIG_BTRFS_FS is not set" >>$(1); \
	fi; \
	if [ "$(NO_USBSTORAGE)" = "y" ] ; then \
		sed -i "/CONFIG_SCSI/d" $(1); \
		echo "# CONFIG_SCSI is not set" >>$(1); \
		sed -i "/CONFIG_EXT2_FS/d" $(1); \
		echo "# CONFIG_EXT2_FS is not set" >>$(1); \
		sed -i "/CONFIG_EXT3_FS/d" $(1); \
		echo "# CONFIG_EXT3_FS is not set" >>$(1); \
		sed -i "/CONFIG_REISERFS_FS/d" $(1); \
		echo "# CONFIG_REISERFS_FS is not set" >>$(1); \
		sed -i "/CONFIG_XFS_FS/d" $(1); \
		echo "# CONFIG_XFS_FS is not set" >>$(1); \
		sed -i "/CONFIG_FUSE_FS/d" $(1); \
		echo "# CONFIG_FUSE_FS is not set" >>$(1); \
		sed -i "/CONFIG_FAT_FS/d" $(1); \
		echo "# CONFIG_FAT_FS is not set" >>$(1); \
		sed -i "/CONFIG_VFAT_FS/d" $(1); \
		echo "# CONFIG_VFAT_FS is not set" >>$(1); \
		sed -i "/CONFIG_CONFIGFS_FS/d" $(1); \
		echo "# CONFIG_CONFIGFS_FS is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_437/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_437 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_850/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_850 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_852/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_852 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_866/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_866 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_936/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_936 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_950/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_950 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_932/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_932 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_CODEPAGE_949/d" $(1); \
		echo "# CONFIG_NLS_CODEPAGE_949 is not set" >>$(1); \
		sed -i "/CONFIG_NLS_ISO8859_1/d" $(1); \
		echo "# CONFIG_NLS_ISO8859_1 is not set" >>$(1); \
		echo "# CONFIG_NLS_ISO8859_13 is not set" >>$(1); \
		echo "# CONFIG_NLS_ISO8859_14 is not set" >>$(1); \
		echo "# CONFIG_NLS_ISO8859_15 is not set" >>$(1); \
	fi; \
	)
endef
