/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <devices.h>
#include <version.h>
#include <net.h>
#include <environment.h>

#if defined(ASUS_PRODUCT)
#include <gpio.h>
#include <replace.h>
#include <flash_wrapper.h>
#include <cmd_tftpServer.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef crc32
#undef crc32
#endif

#if ( ((CFG_ENV_ADDR+CFG_ENV_SIZE) < CFG_MONITOR_BASE) || \
      (CFG_ENV_ADDR >= (CFG_MONITOR_BASE + CFG_MONITOR_LEN)) ) || \
    defined(CFG_ENV_IS_IN_NVRAM)
#define	TOTAL_MALLOC_LEN	(CFG_MALLOC_LEN + CFG_ENV_SIZE)
#else
#define	TOTAL_MALLOC_LEN	CFG_MALLOC_LEN
#endif

#undef DEBUG

#if defined(ASUS_PRODUCT)
extern int do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_load_serial_bin(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_tftpb(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_tftpd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

#if defined(CFG_ENV_IS_IN_NAND)
#if defined(UBOOT_STAGE1)
#define BOOTFILENAME	"uboot_stage1.img"
#elif defined(UBOOT_STAGE2)
#define BOOTFILENAME	"uboot_stage2.img"
#else
#define BOOTFILENAME	"u-boot_nand.bin"
#endif
#else
#define BOOTFILENAME	"u-boot.bin"
#endif

#define SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL	'0'
#define SEL_LOAD_LINUX_SDRAM			'1'
#define SEL_LOAD_LINUX_WRITE_FLASH		'2'
#define SEL_BOOT_FLASH				'3'
#define SEL_ENTER_CLI				'4'
#define SEL_LOAD_BOOT_SDRAM_VIA_SERIAL		'5'
#define SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL	'7'
#define SEL_LOAD_BOOT_SDRAM			'8'
#define SEL_LOAD_BOOT_WRITE_FLASH		'9'
#define SEL_LOAD_LSDK_WRITE_FLASH		'L'

#if defined(UBOOT_STAGE1)
extern struct stage2_loc g_s2_loc;

#define BOOT_IMAGE_NAME	"Bootloader stage1 code"
#define SYS_IMAGE_NAME	"Bootloader stage2 code"
#elif defined(UBOOT_STAGE2)
#define BOOT_IMAGE_NAME "Bootloader stage1/2 code"
#define SYS_IMAGE_NAME  "System code"
#else
#define BOOT_IMAGE_NAME "Boot Loader code"
#define SYS_IMAGE_NAME  "System code"
#endif

int modifies = 0;

#define ARGV_LEN	128
static char file_name_space[ARGV_LEN];
#endif	/* ASUS_PRODUCT */

extern int timer_init(void);

extern int incaip_set_cpuclk(void);

#if defined(CONFIG_WASP_SUPPORT) || defined(CONFIG_MACH_QCA955x) || defined(CONFIG_MACH_QCA953x) || defined(CONFIG_MACH_QCA956x)
void ath_set_tuning_caps(void);
#else
#define ath_set_tuning_caps()	/* nothing */
#endif


extern ulong uboot_end_data;
extern ulong uboot_end;

ulong monitor_flash_len;

const char version_string[] =
	U_BOOT_VERSION" (" __DATE__ " - " __TIME__ ")";

static char *failed = "*** failed ***\n";

/*
 * Begin and End of memory area for malloc(), and current "brk"
 */
static ulong mem_malloc_start;
static ulong mem_malloc_end;
static ulong mem_malloc_brk;


/*
 * The Malloc area is immediately below the monitor copy in DRAM
 */
static void mem_malloc_init (void)
{
	ulong dest_addr = CFG_MONITOR_BASE + gd->reloc_off;

	mem_malloc_end = dest_addr;
	mem_malloc_start = dest_addr - TOTAL_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start,
		0,
		mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	/*
	 * if we are giving memory back make sure we clear it out since
	 * we set MORECORE_CLEARS to 1
	 */
	if (increment < 0)
		memset((void *)new, 0, -increment);

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (void*)MORECORE_FAILURE;
	}
	mem_malloc_brk = new;
	return ((void *) old);
}


static int init_func_ram (void)
{
#ifdef	CONFIG_BOARD_TYPES
	int board_type = gd->board_type;
#else
	int board_type = 0;	/* use dummy arg */
#endif
	puts ("DRAM:  ");

	if ((gd->ram_size = initdram (board_type)) > 0) {
		print_size (gd->ram_size, "\n");
		return (0);
	}
	puts (failed);
	return (1);
}

static int display_banner(void)
{

	printf ("\n\n%s\n\n", version_string);

	return (0);
}

#ifndef CONFIG_ATH_NAND_BR
static void display_flash_config(ulong size)
{
	puts ("Flash: ");
	print_size (size, "\n");
}
#endif


static int init_baudrate (void)
{
	char tmp[64];	/* long enough for environment variables */
	int i = getenv_r ("baudrate", tmp, sizeof (tmp));

	gd->baudrate = (i > 0)
			? (int) simple_strtoul (tmp, NULL, 10)
			: CONFIG_BAUDRATE;

	return (0);
}


/*
 * Breath some life into the board...
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependend #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
#ifndef COMPRESSED_UBOOT
	timer_init,
#endif
	env_init,		/* initialize environment */
#ifdef CONFIG_INCA_IP
	incaip_set_cpuclk,	/* set cpu clock according to environment variable */
#endif
	init_baudrate,		/* initialze baudrate settings */
#ifndef COMPRESSED_UBOOT
	serial_init,		/* serial communications setup */
#endif
	console_init_f,
	display_banner,		/* say that we are here */
#ifndef COMPRESSED_UBOOT
	checkboard,
        init_func_ram,
#endif
	NULL,
};


void board_init_f(ulong bootflag)
{
	gd_t gd_data, *id;
	bd_t *bd;
	init_fnc_t **init_fnc_ptr;
	ulong addr, addr_sp, len = (ulong)&uboot_end - CFG_MONITOR_BASE;
	ulong *s;
#ifdef COMPRESSED_UBOOT
        char board_string[50];
#endif
#ifdef CONFIG_PURPLE
	void copy_code (ulong);
#endif


	/* Pointer is writable since we allocated a register for it.
	 */
	gd = &gd_data;
	/* compiler optimization barrier needed for GCC >= 3.4 */
	__asm__ __volatile__("": : :"memory");

	memset ((void *)gd, 0, sizeof (gd_t));

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0) {
			hang ();
		}
	}

#ifdef COMPRESSED_UBOOT
        checkboard(board_string);
        printf("%s\n\n",board_string);
        gd->ram_size = bootflag;
	puts ("DRAM:  ");
	print_size (gd->ram_size, "\n");
#endif

#ifdef ASUS_PRODUCT
	led_init();	// turn_on_led. If stage1 have turn on all led, this function causes small blinking
#if defined(RPAC66)
	int i = 0;	
	for (i=0;i<3;i++)
	 {
	  leds_on();
	  udelay(100000);
	  leds_off();
	  udelay(100000);
	 }
	 leds_on();
#endif
	gpio_init();
#endif
	/*
	 * Now that we have DRAM mapped and working, we can
	 * relocate the code and continue running from DRAM.
	 */
	addr = CFG_SDRAM_BASE + gd->ram_size;

	/* We can reserve some RAM "on top" here.
	 */

	/* round down to next 4 kB limit.
	 */
	addr &= ~(4096 - 1);
	debug ("Top of RAM usable for U-Boot at: %08lx\n", addr);

	/* Reserve memory for U-Boot code, data & bss
	 * round down to next 16 kB limit
	 */
	addr -= len;
	addr &= ~(16 * 1024 - 1);

	debug ("Reserving %ldk for U-Boot at: %08lx\n", len >> 10, addr);

	 /* Reserve memory for malloc() arena.
	 */
	addr_sp = addr - TOTAL_MALLOC_LEN;
	debug ("Reserving %dk for malloc() at: %08lx\n",
			TOTAL_MALLOC_LEN >> 10, addr_sp);

	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr_sp -= sizeof(bd_t);
	bd = (bd_t *)addr_sp;
	gd->bd = bd;
	debug ("Reserving %d Bytes for Board Info at: %08lx\n",
			sizeof(bd_t), addr_sp);

	addr_sp -= sizeof(gd_t);
	id = (gd_t *)addr_sp;
	debug ("Reserving %d Bytes for Global Data at: %08lx\n",
			sizeof (gd_t), addr_sp);

 	/* Reserve memory for boot params.
	 */
	addr_sp -= CFG_BOOTPARAMS_LEN;
	bd->bi_boot_params = addr_sp;
	debug ("Reserving %dk for boot params() at: %08lx\n",
			CFG_BOOTPARAMS_LEN >> 10, addr_sp);

	/*
	 * Finally, we set up a new (bigger) stack.
	 *
	 * Leave some safety gap for SP, force alignment on 16 byte boundary
	 * Clear initial stack frame
	 */
	addr_sp -= 16;
	addr_sp &= ~0xF;
	s = (ulong *)addr_sp;
	*s-- = 0;
	*s-- = 0;
	addr_sp = (ulong)s;
	debug ("Stack Pointer at: %08lx\n", addr_sp);

	/*
	 * Save local variables to board info struct
	 */
	bd->bi_memstart	= CFG_SDRAM_BASE;	/* start of  DRAM memory */
	bd->bi_memsize	= gd->ram_size;		/* size  of  DRAM memory in bytes */
	bd->bi_baudrate	= gd->baudrate;		/* Console Baudrate */

	memcpy (id, (void *)gd, sizeof (gd_t));

	/* On the purple board we copy the code in a special way
	 * in order to solve flash problems
	 */
#ifdef CONFIG_PURPLE
	copy_code(addr);
#endif

	relocate_code (addr_sp, id, addr);

	/* NOTREACHED - relocate_code() does not return */
}

#if defined(ASUS_PRODUCT)
#if !defined(UBOOT_STAGE1)
void set_ver(void)
{
	int rc;

	rc = replace(0xd18a, (unsigned char*)blver, 4);
	if (rc)
		printf("\n### [set boot ver] flash write fail\n");
}

static void __call_replace(unsigned long addr, unsigned char *ptr, int len, char *msg)
{
	int rc;
	char *status = "ok";

	if (!ptr || len <= 0)
		return;

	if (!msg)
		msg = "";

	rc = replace(addr, ptr, len);
	if (rc)
		status = "fail";

	printf("\n### [%s] flash writs %s\n", msg, status);
}

void init_mac(void)
{
	unsigned char mac[6] = { 0x00, 0x11, 0x11, 0x11, 0x11, 0x11 };
#if defined(DUAL_BAND)
	unsigned char mac2[6] = { 0x00, 0x22, 0x22, 0x22, 0x22, 0x22 };
#endif

	printf("\ninit mac\n");
	__call_replace(0x0, mac, sizeof(mac), "init mac");

#if defined(DUAL_BAND)
	__call_replace(0x7, mac2, sizeof(mac2), "init mac2");
#endif

	__call_replace(0xd188, (unsigned char*) "DB", 2, "init countrycode");
	__call_replace(0xd180, (unsigned char*) "1234567890", 8, "init pincode");
}

/* Restore to default. */
int reset_to_default(void)
{
	ulong addr, size;

#if defined(UBI_SUPPORT)
	unsigned char *p;

	addr = CFG_FLASH_BASE + CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE;
	size = CFG_NVRAM_SIZE;
	p = malloc(CFG_NVRAM_SIZE);
	if (!p)
		p = (unsigned char*) CFG_LOAD_ADDR;

	memset(p, 0xFF, CFG_NVRAM_SIZE);
	ra_flash_erase_write(p, addr, size, 0);

	if (p != (unsigned char*) CFG_LOAD_ADDR)
		free(p);
#endif
	/* erase U-Boot environment whether it shared same block with nvram or not. */
	addr = CFG_ENV_ADDR;
	size = CFG_CONFIG_SIZE;
	printf("Erase 0x%08x size 0x%x\n", addr, size);
	ranand_set_sbb_max_addr(addr + size);
	ra_flash_erase(addr, size);
	ranand_set_sbb_max_addr(0);

#ifdef PLC_PARTS
	addr = CFG_PLC_ADDR;
	size = CFG_PLC_SIZE;
	printf("Erase 0x%08x size 0x%x\n", addr, size);
	ranand_set_sbb_max_addr(addr + size);
	ra_flash_erase(addr, size);
	ranand_set_sbb_max_addr(0);
#endif

	return 0;
}
#endif	/* !UBOOT_STAGE1 */

static void input_value(char *str)
{
	if (str)
		strcpy(console_buffer, str);
	else
		console_buffer[0] = '\0';

	while(1) {
		if (__readline ("==:", 1) > 0) {
			strcpy (str, console_buffer);
			break;
		}
		else
			break;
	}
}

int tftp_config(int type, char *argv[])
{
	char *s;
	char default_file[ARGV_LEN], file[ARGV_LEN], devip[ARGV_LEN], srvip[ARGV_LEN], default_ip[ARGV_LEN];
	static char buf_addr[] = "0x80060000XXX";

	printf(" Please Input new ones /or Ctrl-C to discard\n");

	memset(default_file, 0, ARGV_LEN);
	memset(file, 0, ARGV_LEN);
	memset(devip, 0, ARGV_LEN);
	memset(srvip, 0, ARGV_LEN);
	memset(default_ip, 0, ARGV_LEN);

	printf("\tInput device IP ");
	s = getenv("ipaddr");
	memcpy(devip, s, strlen(s));
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", devip);
	input_value(devip);
	setenv("ipaddr", devip);
	if (strcmp(default_ip, devip) != 0)
		modifies++;

	printf("\tInput server IP ");
	s = getenv("serverip");
	memcpy(srvip, s, strlen(s));
	memset(default_ip, 0, ARGV_LEN);
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", srvip);
	input_value(srvip);
	setenv("serverip", srvip);
	if (strcmp(default_ip, srvip) != 0)
		modifies++;

	sprintf(buf_addr, "0x%x", CFG_LOAD_ADDR);
	argv[1] = buf_addr;

	switch (type) {
	case SEL_LOAD_BOOT_SDRAM:	/* fall through */
	case SEL_LOAD_BOOT_WRITE_FLASH:	/* fall through */
	case SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL:
		printf("\tInput Uboot filename ");
		strncpy(argv[2], BOOTFILENAME, ARGV_LEN);
		break;
	case SEL_LOAD_LINUX_WRITE_FLASH:/* fall through */
	case SEL_LOAD_LINUX_SDRAM:
		printf("\tInput Linux Kernel filename ");
		strncpy(argv[2], "uImage", ARGV_LEN);
		break;
#if defined(LSDK_NART_SUPPORT)
	case SEL_LOAD_LSDK_WRITE_FLASH:
		printf("\tInput LSDK NART firmware filename ");
		break;
#endif
	default:
		printf("%s: Unknown type %d\n", __func__, type);
	}

	s = getenv("bootfile");
	if (s != NULL) {
		memcpy(file, s, strlen(s));
		memcpy(default_file, s, strlen(s));
	}
	printf("(%s) ", file);
	input_value(file);
	if (file == NULL)
		return 1;
	copy_filename(argv[2], file, sizeof(file));
	setenv("bootfile", file);
	if (strcmp(default_file, file) != 0)
		modifies++;

	return 0;
}

#if !defined(UBOOT_STAGE1)
/* System Load %s to SDRAM via TFTP */
static void handle_boottype_1(void)
{
	int argc= 3;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));

	tftp_config(SEL_LOAD_LINUX_SDRAM, argv);
	argc= 3;
	setenv("autostart", "yes");
	do_tftpb(cmdtp, 0, argc, argv);
}
#endif

/* System Load %s then write to Flash via TFTP */
static void handle_boottype_2(void)
{
	int argc= 3, confirm = 0;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;
	char addr_str[11];

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));

	printf(" Warning!! Erase Linux in Flash then burn new one. Are you sure?(Y/N)\n");
	confirm = getc();
	if (confirm != 'y' && confirm != 'Y') {
		printf(" Operation terminated\n");
		return;
	}
#if defined(UBOOT_STAGE1)
	setenv("bootfile", "uboot_stage2.img");
#endif
	tftp_config(SEL_LOAD_LINUX_WRITE_FLASH, argv);
	argc= 3;
	setenv("autostart", "no");
	do_tftpb(cmdtp, 0, argc, argv);

	{
		unsigned int load_address = simple_strtoul(argv[1], NULL, 16);
#if defined(UBOOT_STAGE1)
		struct stage2_loc *s2 = &g_s2_loc;

		ranand_write_stage2(load_address, NetBootFileXferSize);
		ranand_locate_stage2(s2);
		sprintf(addr_str, "0x%X", s2->good->code);
#else
#if defined(MXIC_EN4B_SUPPORT)
		set_4byte(1);
#endif
		ra_flash_erase_write((uchar*)load_address, CFG_KERN_ADDR, NetBootFileXferSize, 0);
#if defined(MXIC_EN4B_SUPPORT)
		set_4byte(0);
#endif
#endif
	}

	argc= 2;
#if !defined(UBOOT_STAGE1)
	sprintf(addr_str, "0x%X", CFG_KERN_ADDR);
#endif
	argv[1] = &addr_str[0];
	do_bootm(cmdtp, 0, argc, argv);
}

#if defined(LSDK_NART_SUPPORT)
/**
 * Choose ASUS firmware / LSDK NART firmware
 * @return:
 * 	0:	LSDK NART firmware
 *  otherwise:	ASUS firmware
 */
static int choose_firmware(void)
{
	int r;
	unsigned char flag[4] __attribute__ ((aligned(4)));

	if (!(r = ra_factory_read(flag, RFCAL_FLAG_OFFSET, 4)) && !memcmp(flag, RFCAL_COPIED, 4))
		return 1;	/* RF caldata have been copied to factory. */

	if (!r && find_rfcaldata() > 0)
		return 0;	/* found valid RF caldata. */

	/* SHOULD NOT HAPPEN. CHOOSE ASUS FIRMWARE. */
	return 1;
}

/**
 * Check and copy RF caldata to factory.
 * @return:
 * 	0:	Factory OK, nothing happen
 * 	1:	RF caldata have been copied to factory.
 *     -1:	RF caldata not found.
 *     -2:	copy RF caldata fail.
 */
static int check_and_copy_rfcaldata(void)
{
	int r, first;
	loff_t offset;
	unsigned char flag[4] __attribute__ ((aligned(4)));

	for (first = 1; first > 0; first--) {
		if (!ra_factory_read(flag, RFCAL_FLAG_OFFSET, 4) && !memcmp(flag, RFCAL_COPIED, 4))
			break;		/* RF caldata have been copied to factory. */

		if ((offset = find_rfcaldata()) <= 0) {
			printf("RF caldata not found!\n");
			return -1;	/* valid RF caldata not found. */
		}

		if ((r = copy_rfcaldata(offset)) < 0)
			return -2;	/* copy RF caldata fail. */
	}

	return 0;
}
#else
static inline check_and_copy_rfcaldata(void) { return 0; }
#endif

/* System Boot Linux via Flash */
static void handle_boottype_3(void)
{
	int r;
	char *argv[2] = {"", ""};
	char addr_str[11];
	cmd_tbl_t c, *cmdtp = &c;

#if !defined(UBOOT_STAGE1)
	r = check_and_copy_rfcaldata();

	if (!chkVer())
	       set_ver();

	if ((chkMAC()) < 0)
	       init_mac();

	argv[1] = &addr_str[0];
	sprintf(addr_str, "0x%X", CFG_KERN_ADDR);

#if defined(LSDK_NART_SUPPORT)
	if (r < 0 || !choose_firmware())
		sprintf(addr_str, "0x%X", CFG_LSDKNART_ADDR);
#endif
#endif /* ! UBOOT_STAGE1 */

	// eth_initialize(gd->bd); // Vic: it will cause eth1 receive no packet in kernel
	do_tftpd(cmdtp, 0, 2, argv);
}

/* System Enter Boot Command Line Interface */
static void handle_boottype_4(void)
{
	printf ("\n%s\n", version_string);
	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
		main_loop ();
	}
}

#if 0 //!defined(UBOOT_STAGE1)
/* System Load %s to SDRAM via Serial (*.bin) */
static void handle_boottype_5(void)
{
	int my_tmp, argc= 3;
	char *argv[4];
	char tftp_load_addr[] = "0x81000000XXX";
	cmd_tbl_t c, *cmdtp = &c;

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));
	sprintf(tftp_load_addr, "0x%x", CFG_LOAD_ADDR);

	printf("**************************************************************\n");
	printf("*** NOTICE: You MUST use the 'RAM Version' of uboot **********\n");
	printf("**************************************************************\n");
	argc= 4;
	argv[1] = tftp_load_addr;
	setenv("autostart", "yes");
	my_tmp = do_load_serial_bin(cmdtp, 0, argc, argv);
	NetBootFileXferSize=simple_strtoul(getenv("filesize"), NULL, 16);

	if (NetBootFileXferSize > CFG_BOOTLOADER_SIZE || my_tmp == 1)
		printf("Abort: Bootloader is too big or download aborted!\n");
}
#endif

/* System Load %s then write to Flash via Serial */
static void handle_boottype_7(void)
{
	int my_tmp, argc= 3;
	cmd_tbl_t c, *cmdtp = &c;
	char tftp_load_addr[] = "0x81000000XXX";
	char *argv[4] = { "loadb", tftp_load_addr, NULL, NULL };
	unsigned int addr = CFG_LOAD_ADDR;

	sprintf(tftp_load_addr, "0x%x", addr);
	argv[1] = tftp_load_addr;
	argc=2;
	setenv("autostart", "no");
	my_tmp = do_load_serial_bin(cmdtp, 0, argc, argv);
	if (my_tmp == 1) {
		printf("Abort: Load bootloader from serial fail!\n");
		return;
	}

	program_bootloader(addr, simple_strtoul(getenv("filesize"), NULL, 16));

	//reset
	do_reset(cmdtp, 0, argc, argv);
}

#if 0 //!defined(UBOOT_STAGE1)
/* System Load %s to SDRAM via TFTP.(*.bin) */
static void handle_boottype_8(void)
{
	int argc= 3;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));

	printf("**************************************************************\n");
	printf("*** NOTICE: You MUST use the 'RAM Version' of uboot **********\n");
	printf("**************************************************************\n");
	tftp_config(SEL_LOAD_BOOT_SDRAM, argv);
	argc= 5;
	setenv("autostart", "yes");
	do_tftpb(cmdtp, 0, argc, argv);
}
#endif

/* System Load %s then write to Flash via TFTP. (.bin) */
static void handle_boottype_9(void)
{
	int argc= 3, confirm = 0;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));

	printf(" Warning!! Erase %s in Flash then burn new one. Are you sure?(Y/N)\n", BOOT_IMAGE_NAME);
	confirm = getc();
	if (confirm != 'y' && confirm != 'Y') {
		printf(" Operation terminated\n");
		return;
	}
	setenv("bootfile", BOOTFILENAME);
	tftp_config(SEL_LOAD_BOOT_WRITE_FLASH, argv);
	argc= 3;
	setenv("autostart", "no");
	do_tftpb(cmdtp, 0, argc, argv);
	program_bootloader(CFG_LOAD_ADDR, NetBootFileXferSize);

	//reset
	do_reset(cmdtp, 0, argc, argv);
}

#if defined(LSDK_NART_SUPPORT)
/* Load LSDK NART firmware (%s), write to Flash via TFTP and reboot. */
static void handle_boottype_L(void)
{
	int argc= 3, confirm = 0;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;
	unsigned int load_address;

	argv[2] = &file_name_space[0];
	memset(file_name_space, 0, sizeof(file_name_space));

	printf(" Warning!! Erase LSDK NART firmware in Flash then burn new one. Are you sure?(Y/N)\n");
	confirm = getc();
	if (confirm != 'y' && confirm != 'Y') {
		printf(" Operation terminated\n");
		return;
	}
	tftp_config(SEL_LOAD_LSDK_WRITE_FLASH, argv);
	argc= 3;
	setenv("autostart", "no");
	do_tftpb(cmdtp, 0, argc, argv);

	load_address = simple_strtoul(argv[1], NULL, 16);
	ra_flash_erase_write((uchar*)load_address, CFG_LSDKNART_ADDR, NetBootFileXferSize, 0);

	//reset
	do_reset(cmdtp, 0, argc, argv);
}
#endif	/* LSDK_NART_SUPPORT */

#if 0 //defined (CFG_ENV_IS_IN_NAND)
/* System Load %s then write to Flash via Serial */
static void handle_boottype_0(void)
{
	int argc= 3;
	char *argv[4];
	cmd_tbl_t c, *cmdtp = &c;

	argc= 1;
	setenv("autostart", "no");
	do_load_serial_bin(cmdtp, 0, argc, argv);
	NetBootFileXferSize=simple_strtoul(getenv("filesize"), NULL, 16);
#if defined(UBOOT_STAGE1)
	ranand_write_stage2(CFG_LOAD_ADDR, NetBootFileXferSize);
#else
	ra_flash_erase_write((uchar*) CFG_LOAD_ADDR, CFG_KERN_ADDR, NetBootFileXferSize, 0);
#endif

	//reset
	do_reset(cmdtp, 0, argc, argv);
}
#endif

static struct boot_menu_s {
	char type;
	void (*func)(void);
	char *msg;
	const char *param1;
} boot_menu[] = {
#if 0 //defined(CFG_ENV_IS_IN_NAND)
	{ SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL,	handle_boottype_0, "Load %s then write to Flash via Serial.", SYS_IMAGE_NAME },
#endif
#if !defined(UBOOT_STAGE1)
	{ SEL_LOAD_LINUX_SDRAM,			handle_boottype_1, "Load %s to SDRAM via TFTP.", SYS_IMAGE_NAME },
#endif
	{ SEL_LOAD_LINUX_WRITE_FLASH,		handle_boottype_2, "Load %s then write to Flash via TFTP.", SYS_IMAGE_NAME },
	{ SEL_BOOT_FLASH,			handle_boottype_3, "Boot %s via Flash (default).", SYS_IMAGE_NAME },
	{ SEL_ENTER_CLI,			handle_boottype_4, "Entr boot command line interface.", NULL },
#if 0 //!defined(UBOOT_STAGE1)
	{ SEL_LOAD_BOOT_SDRAM_VIA_SERIAL,	handle_boottype_5, "Load %s to SDRAM via Serial.", BOOT_IMAGE_NAME },
#endif
	{ SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL,	handle_boottype_7, "Load %s then write to Flash via Serial.", BOOT_IMAGE_NAME },
#if 0 //!defined(UBOOT_STAGE1)
	{ SEL_LOAD_BOOT_SDRAM,			handle_boottype_8, "Load %s to SDRAM via TFTP.", BOOT_IMAGE_NAME },
#endif
	{ SEL_LOAD_BOOT_WRITE_FLASH,		handle_boottype_9, "Load %s then write to Flash via TFTP.", BOOT_IMAGE_NAME },

#if defined(UBI_SUPPORT)
	{ SEL_LOAD_LSDK_WRITE_FLASH,		handle_boottype_L, "Load LSDK NART firmware, write to Flash via TFTP and reboot.", NULL },
#endif

	{ 0, NULL, NULL, NULL },
};

static int OperationSelect(void)
{
	char valid_boot_type[16];
	char msg[256];
	struct boot_menu_s *p = &boot_menu[0];
	char *s = getenv ("bootdelay"), *q = &valid_boot_type[0];
	int my_tmp, BootType = '3', timer1 = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;

	memset(valid_boot_type, 0, sizeof(valid_boot_type));
	printf("\nPlease choose the operation: \n");
	while (p->func) {
		*q++ = p->type;
		sprintf(msg, "   %c: %s\n", p->type, p->msg);
		if (p->param1)
			printf(msg, p->param1);
		else
			printf(msg);

		p++;
	}
	*q = '\0';

	if (timer1 > 5)
		timer1 = 5;
#if defined(UBOOT_STAGE1) || defined(UBOOT_STAGE2)
	if (timer1 <= CONFIG_BOOTDELAY)
		timer1 = 0;
#endif

	timer1 *= 100;
	if (!timer1)
		timer1 = 20;
	while (timer1 > 0) {
		--timer1;
		/* delay 10ms */
		if ((my_tmp = tstc()) != 0) {	/* we got a key press	*/
			timer1 = 0;	/* no more delay	*/
			BootType = getc();
			if (!strchr(valid_boot_type, BootType))
				BootType = '3';
			printf("\n\rYou choosed %c\n\n", BootType);
			break;
		}
		if (DETECT() || DETECT_WPS()) {
			BootType = '3';
			break;
		}
		udelay (10000);
		if ((timer1 / 100 * 100) == timer1)
			printf ("\b\b\b%2d ", timer1 / 100);
	}
	putc ('\n');

	return BootType;
}

#endif /* ASUS_PRODUCT */

/************************************************************************
 *
 * This is the next part if the initialization sequence: we are now
 * running from RAM and have a "normal" C environment, i. e. global
 * data can be written, BSS has been cleared, the stack size in not
 * that critical any more, etc.
 *
 ************************************************************************
 */

void board_init_r (gd_t *id, ulong dest_addr)
{
	cmd_tbl_t *cmdtp;
	ulong size = 0;
	extern void malloc_bin_reloc (void);
#ifndef CFG_ENV_IS_NOWHERE
	extern char * env_name_spec;
#endif
#ifdef CONFIG_ATH_NAND_SUPPORT
#ifdef ATH_SPI_NAND
	extern ulong ath_spi_nand_init(void);
#else
	extern ulong ath_nand_init(void);
#endif
#endif
	char *s, *e;
	bd_t *bd;
	int i;
#if defined(ASUS_PRODUCT)
	char *argv[4], msg[256];
	int argc = 3, BootType = '3';
	struct boot_menu_s *p = &boot_menu[0];

	argv[2] = &file_name_space[0];
	file_name_space[0] = '\0';
#endif

	gd = id;
	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */

	debug ("Now running in RAM - U-Boot at: %08lx\n", dest_addr);

	gd->reloc_off = dest_addr - CFG_MONITOR_BASE;

	monitor_flash_len = (ulong)&uboot_end_data - dest_addr;

	/*
	 * We have to relocate the command table manually
	 */
 	for (cmdtp = &__u_boot_cmd_start; cmdtp !=  &__u_boot_cmd_end; cmdtp++) {
		ulong addr;

		addr = (ulong) (cmdtp->cmd) + gd->reloc_off;
#if 0
		printf ("Command \"%s\": 0x%08lx => 0x%08lx\n",
				cmdtp->name, (ulong) (cmdtp->cmd), addr);
#endif
		cmdtp->cmd =
			(int (*)(struct cmd_tbl_s *, int, int, char *[]))addr;

		addr = (ulong)(cmdtp->name) + gd->reloc_off;
		cmdtp->name = (char *)addr;

		if (cmdtp->usage) {
			addr = (ulong)(cmdtp->usage) + gd->reloc_off;
			cmdtp->usage = (char *)addr;
		}
#ifdef	CFG_LONGHELP
		if (cmdtp->help) {
			addr = (ulong)(cmdtp->help) + gd->reloc_off;
			cmdtp->help = (char *)addr;
		}
#endif
	}
	/* there are some other pointer constants we must deal with */
#ifndef CFG_ENV_IS_NOWHERE
	env_name_spec += gd->reloc_off;
#endif
#if defined(ASUS_PRODUCT)
	/* relocate command table */
	for (p = &boot_menu[0]; p->func; ++p ) {
		ulong addr;

		addr = (ulong) (p->func) + gd->reloc_off;
		p->func = (void (*)(void)) addr;
		addr = (ulong) (p->msg) + gd->reloc_off;
		p->msg = (char*) addr;
		if (p->param1) {
			addr = (ulong) (p->param1) + gd->reloc_off;
			p->param1 = (char*) addr;
		}
	}
#endif


#ifndef CONFIG_ATH_NAND_BR
	/* configure available FLASH banks */
	size = flash_init();
	display_flash_config (size);
#endif
#if defined(RPAC66)
	  leds_off();
#endif
	bd = gd->bd;
	bd->bi_flashstart = CFG_FLASH_BASE;
	bd->bi_flashsize = size;
#if CFG_MONITOR_BASE == CFG_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for U-Boot */
#else
	bd->bi_flashoffset = 0;
#endif

	/* initialize malloc() area */
	mem_malloc_init();
	malloc_bin_reloc();

	/* FIXME: Check boot reason and print it. */

#ifdef CONFIG_ATH_NAND_BR
	bd->bi_flashsize = ath_nand_init();
#endif

#if defined(ASUS_PRODUCT)
#if defined(MAPAC1750)
	blue_led_on();
#else
	disable_all_leds();	/* Inhibit ALL LED, except PWR LED. */
	leds_off();
	power_led_on();
#endif
#endif

#if defined(ASUS_PRODUCT)
	ra_flash_init_layout();
#endif

#if defined(CFG_MALLOC_LEN)
	printf("Maximum malloc length: %d KBytes\n", CFG_MALLOC_LEN >> 10);
	printf("mem_malloc_start/brk/end: 0x%x/%x/%x\n",
		mem_malloc_start, mem_malloc_brk, mem_malloc_end);
#endif

	/* relocate environment function pointers etc. */
	env_relocate();

	/* board MAC address */
	s = getenv ("ethaddr");
	for (i = 0; i < 6; ++i) {
		bd->bi_enetaddr[i] = s ? simple_strtoul (s, &e, 16) : 0;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	/* IP Address */
	bd->bi_ip_addr = getenv_IPaddr("ipaddr");

#if defined(CONFIG_PCI)
	/*
	 * Do pci configuration
	 */
	pci_init();
#endif

/** leave this here (after malloc(), environment and PCI are working) **/
	/* Initialize devices */
	devices_init ();

	jumptable_init ();

	/* Initialize the console (after the relocation and devices init) */
	console_init_r ();
/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

	/* Initialize from environment */
	if ((s = getenv ("loadaddr")) != NULL) {
		load_addr = simple_strtoul (s, NULL, 16);
	}
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if ((s = getenv ("bootfile")) != NULL) {
		copy_filename (BootFile, s, sizeof (BootFile));
	}
#endif	/* CFG_CMD_NET */

#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	misc_init_r ();
#endif

#if defined(CONFIG_ATH_NAND_BR)
	ranand_check_and_fix_bootloader();
#endif

#if (CONFIG_COMMANDS & CFG_CMD_NET)
#if defined(CONFIG_NET_MULTI)
	puts ("Net:   ");
#endif

	eth_initialize(gd->bd);
#endif

#if defined(CONFIG_ATH_NAND_SUPPORT) && !defined(CONFIG_ATH_NAND_BR)
#ifdef ATH_SPI_NAND
	ath_spi_nand_init();
#else
 	ath_nand_init();
#endif
#endif

        ath_set_tuning_caps(); /* Needed here not to mess with Ethernet clocks */

#if defined(ASUS_PRODUCT)
	/* Boot Loader Menu */
#if defined(RPAC66)
	  leds_on();
#elif defined(MAPAC1750)
	leds_off();
#endif
	//LANWANPartition();	/* FIXME */
	BootType = OperationSelect();
	for (p = &boot_menu[0]; p->func; ++p ) {
		if (p->type != BootType) {
			continue;
		}

		sprintf(msg, "   %c: %s\n", p->type, p->msg);
		if (p->param1)
			printf(msg, p->param1);
		else
			printf(msg);

		p->func();
		break;
	}

	if (!p->func) {
		printf("   \nSystem Boot Linux via Flash.\n");
		do_bootm(cmdtp, 0, 1, argv);
	}

	do_reset(cmdtp, 0, argc, argv);
#else	/* !ASUS_PRODUCT */
	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
		main_loop ();
	}
#endif	/* ASUS_PRODUCT */

	/* NOTREACHED - no way out of command loop except booting */
}

void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}
