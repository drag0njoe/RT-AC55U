/*
 * Driver for NAND support, Rick Bronson
 * borrowed heavily from:
 * (c) 1999 Machine Vision Holdings, Inc.
 * (c) 1999, 2000 David Woodhouse <dwmw2@infradead.org>
 *
 * Added 16-bit nand support
 * (C) 2004 Texas Instruments
 */

#include <common.h>


#ifndef CFG_NAND_LEGACY
/*
 *
 * New NAND support
 *
 */
#include <common.h>

#if (CONFIG_COMMANDS & CFG_CMD_NAND)

#include <command.h>
#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>

#ifdef CONFIG_SHOW_BOOT_PROGRESS
# include <status_led.h>
# define SHOW_BOOT_PROGRESS(arg)	show_boot_progress(arg)
#else
# define SHOW_BOOT_PROGRESS(arg)
#endif

#include <jffs2/jffs2.h>
#include <nand.h>

/* ath_nand.c */
#define ATH_NAND_BLK_DONT_KNOW	0x0
#define ATH_NAND_BLK_GOOD	0x1
#define ATH_NAND_BLK_BAD	0x2
#define ATH_NAND_BLK_ERASED	0x3
extern void ath_nand_set_blk_state_wrap(struct mtd_info *mtd, loff_t b, unsigned state);

extern nand_info_t nand_info[];       /* info for NAND chips */

static void hex_dump(ulong addr, u_char *buf, ulong len)
{
	int i, j;
	char c, s;
	u_char *p, *q;

	if (!buf)
		return;

	for (i = len, p = q = buf; i > 0; i -= 16, addr += 16, p += 16) {
		q = p;
		printf("%08lx: ", addr);
		for (j = 0; j < 16; ++j) {
			 s = (j == 7)? '-':' ';
			printf("%02X%c", *(p + j), s);
		}
		printf("    ");
		for (j = 0; j < 16; ++j) {
			c = *(q + j);
			if (c < 0x20 || c > 0x7f)
				c = '.';
			printf("%c", c);
		}
		printf("\n");
	}
}

static int nand_raw_dump(nand_info_t *nand, ulong off, int page)
{
	int i;
	u_char *buf;
	ulong addr = off;

	buf = malloc(nand->writesize + nand->oobsize);
	if (!buf) {
		puts("No memory for page buffer\n");
		return 1;
	}
	off &= ~(nand->writesize - 1);
	i = nand_read_raw(nand, buf, off, nand->writesize, nand->oobsize);
	if (i < 0) {
		printf("Error (%d) reading page %08x\n", i, off);
		free(buf);
		return 1;
	}
	addr = off;
	printf("Address %08lx (page %lx) dump:\n", off, off >> nand->writesize_shift);
	if (page > 0)
		hex_dump(addr, buf, nand->writesize);
	puts("OOB:\n");
	hex_dump(addr + nand->writesize, buf + nand->writesize, nand->oobsize);
	free(buf);

	return 0;
}

static int nand_dump_oob(nand_info_t *nand, ulong off)
{
	return nand_raw_dump(nand, off, 0);
}

static int nand_dump(nand_info_t *nand, ulong off)
{
	return nand_raw_dump(nand, off, 1);
}

/* ------------------------------------------------------------------------- */

static void
arg_off_size(int argc, char *argv[], ulong *off, ulong *size, ulong totsize)
{
	*off = 0;
	*size = 0;

#if defined(CONFIG_JFFS2_NAND) && defined(CFG_JFFS_CUSTOM_PART)
	if (argc >= 1 && strcmp(argv[0], "partition") == 0) {
		int part_num;
		struct part_info *part;
		const char *partstr;

		if (argc >= 2)
			partstr = argv[1];
		else
			partstr = getenv("partition");

		if (partstr)
			part_num = (int)simple_strtoul(partstr, NULL, 10);
		else
			part_num = 0;

		part = jffs2_part_info(part_num);
		if (part == NULL) {
			printf("\nInvalid partition %d\n", part_num);
			return;
		}
		*size = part->size;
		*off = (ulong)part->offset;
	} else
#endif
	{
		if (argc >= 1)
			*off = (ulong)simple_strtoul(argv[0], NULL, 16);
		else
			*off = 0;

		if (argc >= 2)
			*size = (ulong)simple_strtoul(argv[1], NULL, 16);
		else
			*size = 0;

	}

}

int do_nand(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int i, dev, ret;
	ulong addr, off, size;
	char *cmd, *s;
	nand_info_t *nand;

	/* at least two arguments please */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	if (strcmp(cmd, "info") == 0) {

		putc('\n');
		for (i = 0; i < CFG_MAX_NAND_DEVICE; i++) {
			if (nand_info[i].name)
				printf("Device %d: %s, sector size %lu KiB\n",
					i, nand_info[i].name,
					nand_info[i].erasesize >> 10);
		}
		return 0;
	}

	if (strcmp(cmd, "device") == 0) {

		if (argc < 3) {
			if ((nand_curr_device < 0) ||
			    (nand_curr_device >= CFG_MAX_NAND_DEVICE))
				puts("\nno devices available\n");
			else
				printf("\nDevice %d: %s\n", nand_curr_device,
					nand_info[nand_curr_device].name);
			return 0;
		}
		dev = (int)simple_strtoul(argv[2], NULL, 10);
		if (dev < 0 || dev >= CFG_MAX_NAND_DEVICE || !nand_info[dev].name) {
			puts("No such device\n");
			return 1;
		}
		printf("Device %d: %s", dev, nand_info[dev].name);
		puts("... is now current device\n");
		nand_curr_device = dev;
		return 0;
	}

	/* the following commands operate on the current device */
	if (nand_curr_device < 0 || nand_curr_device >= CFG_MAX_NAND_DEVICE ||
	    !nand_info[nand_curr_device].name) {
		puts("\nno devices available\n");
		return 1;
	}
	nand = &nand_info[nand_curr_device];

	if (strcmp(cmd, "bad") == 0) {
		printf("\nDevice %d bad blocks:\n", nand_curr_device);
		for (off = 0; off < nand->size; off += nand->erasesize)
			if (nand_block_isbad(nand, off))
				printf("  %08x\n", off);
		return 0;
	}

	if (strcmp(cmd, "erase") == 0) {
		arg_off_size(argc - 2, argv + 2, &off, &size, nand->size);
		if (off == 0 && size == 0)
			return 0;

		if (!size)
			size = nand->erasesize;
		printf("\nNAND erase: device %d offset 0x%x, size 0x%x ",
		       nand_curr_device, off, size);
		ret = nand_erase(nand, off, size);
		printf("\n%s\n", ret ? "ERROR" : "OK");

		return ret == 0 ? 0 : 1;
	}

	if (strncmp(cmd, "dump", 4) == 0) {
		if (argc < 3)
			goto usage;

		s = strchr(cmd, '.');
		off = (int)simple_strtoul(argv[2], NULL, 16);

		if (s != NULL && strcmp(s, ".oob") == 0)
			ret = nand_dump_oob(nand, off);
		else
			ret = nand_dump(nand, off);

		return ret == 0 ? 1 : 0;

	}

	/* read write */
	if (strncmp(cmd, "read", 4) == 0 || strncmp(cmd, "write", 5) == 0) {
		if (argc < 4)
			goto usage;
/*
		s = strchr(cmd, '.');
		clean = CLEAN_NONE;
		if (s != NULL) {
			if (strcmp(s, ".jffs2") == 0 || strcmp(s, ".e") == 0
			    || strcmp(s, ".i"))
				clean = CLEAN_JFFS2;
		}
*/
		addr = (ulong)simple_strtoul(argv[2], NULL, 16);
		if (addr < CFG_SDRAM_BASE) {
			printf("Invalid RAM address %08lx\n", addr);
			return 1;
		}

		arg_off_size(argc - 3, argv + 3, &off, &size, nand->size);
		if (off == 0 && size == 0)
			return 0;

		if (!size)
			size = nand->erasesize;
		i = strncmp(cmd, "read", 4) == 0;	/* 1 = read, 0 = write */
		printf("\nNAND %s: device %d offset 0x%x, size %u ... ",
		       i ? "read" : "write", nand_curr_device, off, size);

		if (i)
			ret = nand_read(nand, (loff_t)off, (size_t*) &size, (u_char *)addr);
		else
			ret = nand_write(nand, (loff_t)off, (size_t*) &size, (u_char *)addr);

		printf(" %d bytes %s: %s\n", size,
		       i ? "read" : "written", ret ? "ERROR" : "OK");

		return ret == 0 ? 0 : 1;
	}

#if defined(DEBUG_BAD_BLOCK)
	if (!strncmp(cmd, "markbad", 7)) {
		u_char buf[4], buf1[4];

		if (!nand->block_markbad) {
			printf("mtd->block_markbad is not defined!\n");
			return 0;
		}
		off = (int)simple_strtoul(argv[2], NULL, 16);
		off &= ~nand->erasesize_mask;	/* aligned to block address */
		if ((ret = nand_read_raw(nand, buf, off, 0, sizeof(buf))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off, ret);
			return 0;
		}
		if ((ret = nand_read_raw(nand, buf1, off + nand->writesize, 0, sizeof(buf1))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off + nand->writesize, ret);
			return 0;
		}
		if (buf[0] != 0xFF || buf1[0] != 0xFF) {
			printf("offset %lx is bad-block. (mark %02x,%02x, bbt state %d)\n",
				off, buf[0], buf1[0], nand_block_isbad(nand, off));
			return 0;
		}

		if ((ret = nand->block_markbad(nand, off)) != 0) {
			printf("mark bad block at offset %lx fail. (ret %d)\n", off, ret);
			return 0;
		}

		return 0;
	}
	if (!strncmp(cmd, "erasebad", 9)) {
		u_char buf[4], buf1[4];

		off = (int)simple_strtoul(argv[2], NULL, 16);
		off &= ~nand->erasesize_mask;	/* aligned to block address */
		if ((ret = nand_read_raw(nand, buf, off, 0, sizeof(buf))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off, ret);
			return 0;
		}
		if ((ret = nand_read_raw(nand, buf1, off + nand->writesize, 0, sizeof(buf1))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off + nand->writesize, ret);
			return 0;
		}
		if (buf[0] == 0xFF && buf1[0] == 0xFF) {
			printf("offset %lx is not bad-block\n", off);
			return 0;
		} else if ((buf[0] != SW_BAD_BLOCK_INDICATION && buf[0] != 0xFF) ||
			   (buf1[0] != SW_BAD_BLOCK_INDICATION && buf1[0] != 0xFF))
		{
			printf("skip unknown bad-block indication byte. (mark %02x,%02x)\n", buf[0], buf1[0]);
			return 0;
		}
		ath_nand_set_blk_state_wrap(nand, off, ATH_NAND_BLK_GOOD);	/* cheat mtd->erase */
		if ((ret = nand_erase(nand, off, nand->erasesize)) != 0) {
			printf("erase offset %lx fail. (ret %d)\n", off, ret);
			return 0;
		}

		return 0;
	}
#endif

#if defined(DEBUG_ECC_CORRECTION)
	if (!strncmp(cmd, "flipbits", 8)) {
		/* nand flipbits <page_number> byte_addr:bit_addr[,bit_addr][,bit_addr...]
		 * [byte_addr:bit_addr[,bit_addr][,bit_addr...]
		 * [byte_addr:bit_addr[,bit_addr][,bit_addr...]
		 * [byte_addr:bit_addr[,bit_addr][,bit_addr...]
		 * Up to 4 bytes can be alerted.
		 */
		const int pages_per_block = nand->erasesize / nand->writesize;
		struct mtd_oob_ops ops;
		int i, mod_cnt = 0, ret, cnt;
		ulong off;
		struct mod_s {
			unsigned int byte_addr;
			unsigned int bit_mask;
		} mod_ary[4], *mod = &mod_ary[0];
		unsigned int block, page, start_page, byte_addr, bit;
		char *q;
		unsigned char c, *p, blk_buf[nand->erasesize + pages_per_block * nand->oobsize];
		struct erase_info ei;

		if (argc < 4)
			return 1;
		if (!nand->erase || !nand->write_oob) {
			printf("Invalid nand->erase %p or nand->write_oob %p\n", nand->erase, nand->write_oob);
			return 1;
		}

		page = simple_strtoul(argv[2], NULL, 16);
		if (page * nand->writesize >= nand->size) {
			printf("invalid page 0x%x\n", page);
			return 1;
		}
		start_page = page & ~(pages_per_block - 1);
		printf("erasesize_shift %d writesize_shift %d\n", nand->erasesize_shift, nand->writesize_shift);
		block = start_page >> (nand->erasesize_shift - nand->writesize_shift);
		printf("page 0x%x start_page 0x%x block 0x%x\n", page, start_page, block);

		/* parsing byte address, bit address */
		for (i = 3; i < argc; ++i) {
			if ((q = strchr(argv[i], ':')) == NULL) {
				printf("colon symbol not found.\n");
				return 1;
			}

			*q = '\0';
			byte_addr = simple_strtoul(argv[i], NULL, 16);
			if (byte_addr >= (2048 + 64)) {
				printf("invalid byte address 0x%x\n", byte_addr);
				return 1;
			}
			mod->byte_addr = byte_addr;
			mod->bit_mask = 0;

			q++;
			while (q && *q != '\0') {
				if (*q < '0' || *q > '9') {
					printf("invalid character. (%c %x)\n", *q, *q);
					return 1;
				}
				bit = simple_strtoul(q, NULL, 16);
				if (bit >= 8) {
					printf("invalid bit address %d\n", bit);
					return 1;
				}
				mod->bit_mask |= (1 << bit);
				q = strchr(q, ',');
				if (q)
					q++;
			}
			mod_cnt++;
			mod++;
		}

		if (!mod_cnt) {
			printf("byte address/bit address pair is not specified.\n");
			return 1;
		}

		/* read a block from block-aligned address with valid OOB information */
		for (i = 0, cnt = 0, p = &blk_buf[0], off = start_page << nand->writesize_shift;
		     i < pages_per_block;
		     ++i, p += nand->writesize + nand->oobsize, off += nand->writesize)
		{
			if ((ret = nand_read_raw(nand, p, off, nand->writesize, nand->oobsize)) < 0)
				printf("read page 0x%x fail. (ret %d)\n", start_page + i, ret);
			else
				cnt++;
		}

		if (cnt != pages_per_block)
			return 1;

		/* erase block */
		memset(&ei, 0, sizeof(ei));
		ei.mtd = nand;
		ei.addr = block << nand->erasesize_shift;
		ei.len = nand->erasesize;
		if ((ret = nand->erase(nand, &ei)) != 0) {
			printf("Erase addr %x len %x fail. (ret %d)\n", ei.addr, ei.len, ret);
			return 1;
		}

		/* flip bits */
		for (i = 0, mod = &mod_ary[0], p = &blk_buf[0] + ((page - start_page) << nand->writesize_shift) + ((page - start_page) * nand->oobsize);
			i < mod_cnt;
			++i, ++mod)
		{
			c = *(p + mod->byte_addr);
			*(p + mod->byte_addr) ^= mod->bit_mask;
			printf("flip page 0x%x byte 0x%x bitmask 0x%x: orig val %02x -> %02x\n",
				page, mod->byte_addr, mod->bit_mask, c, *(p + mod->byte_addr));
		}

		/* use raw write to write back page and oob information */
		for (i = 0, p = &blk_buf[0]; i < pages_per_block; ++i) {
			memset(&ops, 0, sizeof(ops));
			ops.datbuf = p;
			ops.len = nand->writesize;
			ops.oobbuf = p + nand->writesize;
			ops.ooblen = nand->oobsize;
			ops.mode =  MTD_OOB_RAW;
			if ((ret = nand->write_oob(nand, (start_page + i) << nand->writesize_shift, &ops)) != 0)
				printf("write page 0x%x fail. (ret %d)\n", start_page + i, ret);

			p += nand->writesize + nand->oobsize;
		}

		return 0;
	}
#endif

usage:
	printf("Usage:\n%s\n", cmdtp->usage);
	return 1;
}

U_BOOT_CMD(nand, 5, 1, do_nand,
	"nand    - NAND sub-system\n",
	"info                  - show available NAND devices\n"
	"nand device [dev]     - show or set current device\n"
	"nand read[.jffs2]     - addr off size\n"
	"nand write[.jffs2]    - addr off size - read/write `size' bytes starting\n"
	"    at offset `off' to/from memory address `addr'\n"
	"nand erase [clean] [off size] - erase `size' bytes from\n"
	"    offset `off' (entire device if not specified)\n"
	"nand bad - show bad blocks\n"
	"nand dump[.oob] off - dump page\n"
#if defined(DEBUG_BAD_BLOCK)
	"nand markbad off - mark bad block at offset (UNSAFE)\n"
	"nand erasebad off - erase bad block at offset (UNSAFE)\n"
#endif
#if defined(DEBUG_ECC_CORRECTION)
	"nand flipbits <page_number> "	\
		"byte_addr:bit_addr[,bit_addr][,bit_addr...] "	\
		"[byte_addr:bit_addr[,bit_addr][,bit_addr...] "	\
		"[byte_addr:bit_addr[,bit_addr][,bit_addr...] "	\
		"[byte_addr:bit_addr[,bit_addr][,bit_addr...]\n"
#endif
);

#if !defined(SHRINK_UBOOT)
int do_nandboot(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	char *boot_device = NULL;
	char *ep;
	int dev;
	int r;
	ulong addr, cnt, offset = 0;
	image_header_t *hdr;
	nand_info_t *nand;

	switch (argc) {
	case 1:
		addr = CFG_LOAD_ADDR;
		boot_device = getenv("bootdevice");
		break;
	case 2:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = getenv("bootdevice");
		break;
	case 3:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = argv[2];
		break;
	case 4:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = argv[2];
		offset = simple_strtoul(argv[3], NULL, 16);
		break;
	default:
		printf("Usage:\n%s\n", cmdtp->usage);
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	if (!boot_device) {
		puts("\n** No boot device **\n");
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	dev = simple_strtoul(boot_device, &ep, 16);

	if (dev < 0 || dev >= CFG_MAX_NAND_DEVICE || !nand_info[dev].name) {
		printf("\n** Device %d not available\n", dev);
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	nand = &nand_info[dev];
	printf("\nLoading from device %d: %s (offset 0x%lx)\n",
	       dev, nand->name, offset);

	cnt = nand->writesize;
	r = nand_read(nand, offset, (size_t*) &cnt, (u_char *) addr);
	if (r) {
		printf("** Read error on %d\n", dev);
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	hdr = (image_header_t *) addr;

	if (ntohl(hdr->ih_magic) != IH_MAGIC) {
		printf("\n** Bad Magic Number 0x%x **\n", hdr->ih_magic);
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	print_image_hdr(hdr);

	cnt = (ntohl(hdr->ih_size) + sizeof (image_header_t));

	r = nand_read(nand, offset, (size_t*) &cnt, (u_char *) addr);
	if (r) {
		printf("** Read error on %d\n", dev);
		SHOW_BOOT_PROGRESS(-1);
		return 1;
	}

	/* Loading ok, update default load address */

	load_addr = addr;
#ifndef CONFIG_ATH_NAND_SUPPORT
	/* Check if we should attempt an auto-start */
	if (((ep = getenv("autostart")) != NULL) && (strcmp(ep, "yes") == 0)) {
#endif
		char *local_args[2];
		extern int do_bootm(cmd_tbl_t *, int, int, char *[]);

		local_args[0] = argv[0];
		local_args[1] = NULL;

#ifndef CONFIG_ATH_NAND_SUPPORT
		printf("Automatic boot of image at addr 0x%08lx ...\n", addr);
#endif

		do_bootm(cmdtp, 0, 1, local_args);
		return 1;
#ifndef CONFIG_ATH_NAND_SUPPORT
	}
#endif
	return 0;
}

U_BOOT_CMD(nboot, 4, 1, do_nandboot,
	"nboot   - boot from NAND device\n", "loadAddr dev\n");
#endif	/* #if !SHRINK_UBOOT */


#endif				/* (CONFIG_COMMANDS & CFG_CMD_NAND) */

#else /* CFG_NAND_LEGACY */
/*
 *
 * Legacy NAND support - to be phased out
 *
 */
#include <command.h>
#include <malloc.h>
#include <asm/io.h>
#include <watchdog.h>

#ifdef CONFIG_SHOW_BOOT_PROGRESS
# include <status_led.h>
# define SHOW_BOOT_PROGRESS(arg)	show_boot_progress(arg)
#else
# define SHOW_BOOT_PROGRESS(arg)
#endif

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
#include <linux/mtd/nand_legacy.h>
#if 0
#include <linux/mtd/nand_ids.h>
#include <jffs2/jffs2.h>
#endif

#ifdef CONFIG_OMAP1510
void archflashwp(void *archdata, int wp);
#endif

#define ROUND_DOWN(value,boundary)      ((value) & (~((boundary)-1)))

#undef	NAND_DEBUG
#undef	PSYCHO_DEBUG

/* ****************** WARNING *********************
 * When ALLOW_ERASE_BAD_DEBUG is non-zero the erase command will
 * erase (or at least attempt to erase) blocks that are marked
 * bad. This can be very handy if you are _sure_ that the block
 * is OK, say because you marked a good block bad to test bad
 * block handling and you are done testing, or if you have
 * accidentally marked blocks bad.
 *
 * Erasing factory marked bad blocks is a _bad_ idea. If the
 * erase succeeds there is no reliable way to find them again,
 * and attempting to program or erase bad blocks can affect
 * the data in _other_ (good) blocks.
 */
#define	 ALLOW_ERASE_BAD_DEBUG 0

#define CONFIG_MTD_NAND_ECC  /* enable ECC */
#define CONFIG_MTD_NAND_ECC_JFFS2

/* bits for nand_legacy_rw() `cmd'; or together as needed */
#define NANDRW_READ	0x01
#define NANDRW_WRITE	0x00
#define NANDRW_JFFS2	0x02
#define NANDRW_JFFS2_SKIP	0x04

/*
 * Imports from nand_legacy.c
 */
extern struct nand_chip nand_dev_desc[CFG_MAX_NAND_DEVICE];
extern int curr_device;
extern int nand_legacy_erase(struct nand_chip *nand, size_t ofs,
			    size_t len, int clean);
extern int nand_legacy_rw(struct nand_chip *nand, int cmd, size_t start,
			 size_t len, size_t *retlen, u_char *buf);
extern void nand_print(struct nand_chip *nand);
extern void nand_print_bad(struct nand_chip *nand);
extern int nand_read_oob(struct nand_chip *nand, size_t ofs,
			       size_t len, size_t *retlen, u_char *buf);
extern int nand_write_oob(struct nand_chip *nand, size_t ofs,
				size_t len, size_t *retlen, const u_char *buf);


int do_nand (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int rcode = 0;

    switch (argc) {
    case 0:
    case 1:
	printf ("Usage:\n%s\n", cmdtp->usage);
	return 1;
    case 2:
	if (strcmp(argv[1],"info") == 0) {
		int i;

		putc ('\n');

		for (i=0; i<CFG_MAX_NAND_DEVICE; ++i) {
			if(nand_dev_desc[i].ChipID == NAND_ChipID_UNKNOWN)
				continue; /* list only known devices */
			printf ("Device %d: ", i);
			nand_print(&nand_dev_desc[i]);
		}
		return 0;

	} else if (strcmp(argv[1],"device") == 0) {
		if ((curr_device < 0) || (curr_device >= CFG_MAX_NAND_DEVICE)) {
			puts ("\nno devices available\n");
			return 1;
		}
		printf ("\nDevice %d: ", curr_device);
		nand_print(&nand_dev_desc[curr_device]);
		return 0;

	} else if (strcmp(argv[1],"bad") == 0) {
		if ((curr_device < 0) || (curr_device >= CFG_MAX_NAND_DEVICE)) {
			puts ("\nno devices available\n");
			return 1;
		}
		printf ("\nDevice %d bad blocks:\n", curr_device);
		nand_print_bad(&nand_dev_desc[curr_device]);
		return 0;

	}
	printf ("Usage:\n%s\n", cmdtp->usage);
	return 1;
    case 3:
	if (strcmp(argv[1],"device") == 0) {
		int dev = (int)simple_strtoul(argv[2], NULL, 10);

		printf ("\nDevice %d: ", dev);
		if (dev >= CFG_MAX_NAND_DEVICE) {
			puts ("unknown device\n");
			return 1;
		}
		nand_print(&nand_dev_desc[dev]);
		/*nand_print (dev);*/

		if (nand_dev_desc[dev].ChipID == NAND_ChipID_UNKNOWN) {
			return 1;
		}

		curr_device = dev;

		puts ("... is now current device\n");

		return 0;
	}
	else if (strcmp(argv[1],"erase") == 0 && strcmp(argv[2], "clean") == 0) {
		struct nand_chip* nand = &nand_dev_desc[curr_device];
		ulong off = 0;
		ulong size = nand->totlen;
		int ret;

		printf ("\nNAND erase: device %d offset %ld, size %ld ... ",
			curr_device, off, size);

		ret = nand_legacy_erase (nand, off, size, 1);

		printf("%s\n", ret ? "ERROR" : "OK");

		return ret;
	}

	printf ("Usage:\n%s\n", cmdtp->usage);
	return 1;
    default:
	/* at least 4 args */

	if (strncmp(argv[1], "read", 4) == 0 ||
	    strncmp(argv[1], "write", 5) == 0) {
		ulong addr = simple_strtoul(argv[2], NULL, 16);
		ulong off  = simple_strtoul(argv[3], NULL, 16);
		ulong size = simple_strtoul(argv[4], NULL, 16);
		int cmd    = (strncmp(argv[1], "read", 4) == 0) ?
				NANDRW_READ : NANDRW_WRITE;
		int ret, total;
		char* cmdtail = strchr(argv[1], '.');

		if (cmdtail && !strncmp(cmdtail, ".oob", 2)) {
			/* read out-of-band data */
			if (cmd & NANDRW_READ) {
				ret = nand_read_oob(nand_dev_desc + curr_device,
						    off, size, (size_t *)&total,
						    (u_char*)addr);
			}
			else {
				ret = nand_write_oob(nand_dev_desc + curr_device,
						     off, size, (size_t *)&total,
						     (u_char*)addr);
			}
			return ret;
		}
		else if (cmdtail && !strncmp(cmdtail, ".jffs2", 2))
			cmd |= NANDRW_JFFS2;	/* skip bad blocks */
		else if (cmdtail && !strncmp(cmdtail, ".jffs2s", 2)) {
			cmd |= NANDRW_JFFS2;	/* skip bad blocks (on read too) */
			if (cmd & NANDRW_READ)
				cmd |= NANDRW_JFFS2_SKIP;	/* skip bad blocks (on read too) */
		}
#ifdef SXNI855T
		/* need ".e" same as ".j" for compatibility with older units */
		else if (cmdtail && !strcmp(cmdtail, ".e"))
			cmd |= NANDRW_JFFS2;	/* skip bad blocks */
#endif
#ifdef CFG_NAND_SKIP_BAD_DOT_I
		/* need ".i" same as ".jffs2s" for compatibility with older units (esd) */
		/* ".i" for image -> read skips bad block (no 0xff) */
		else if (cmdtail && !strcmp(cmdtail, ".i")) {
			cmd |= NANDRW_JFFS2;	/* skip bad blocks (on read too) */
			if (cmd & NANDRW_READ)
				cmd |= NANDRW_JFFS2_SKIP;	/* skip bad blocks (on read too) */
		}
#endif /* CFG_NAND_SKIP_BAD_DOT_I */
		else if (cmdtail) {
			printf ("Usage:\n%s\n", cmdtp->usage);
			return 1;
		}

		printf ("\nNAND %s: device %d offset %ld, size %ld ... ",
			(cmd & NANDRW_READ) ? "read" : "write",
			curr_device, off, size);

		ret = nand_legacy_rw(nand_dev_desc + curr_device, cmd, off, size,
			     (size_t *)&total, (u_char*)addr);

		printf (" %d bytes %s: %s\n", total,
			(cmd & NANDRW_READ) ? "read" : "written",
			ret ? "ERROR" : "OK");

		return ret;
	} else if (strcmp(argv[1],"erase") == 0 &&
		   (argc == 4 || strcmp("clean", argv[2]) == 0)) {
		int clean = argc == 5;
		ulong off = simple_strtoul(argv[2 + clean], NULL, 16);
		ulong size = simple_strtoul(argv[3 + clean], NULL, 16);
		int ret;

		printf ("\nNAND erase: device %d offset %ld, size %ld ... ",
			curr_device, off, size);

		ret = nand_legacy_erase (nand_dev_desc + curr_device,
					off, size, clean);

		printf("%s\n", ret ? "ERROR" : "OK");

		return ret;
	} else {
		printf ("Usage:\n%s\n", cmdtp->usage);
		rcode = 1;
	}

	return rcode;
    }
}

U_BOOT_CMD(
	nand,	5,	1,	do_nand,
	"nand    - NAND sub-system\n",
	"info  - show available NAND devices\n"
	"nand device [dev] - show or set current device\n"
	"nand read[.jffs2[s]]  addr off size\n"
	"nand write[.jffs2] addr off size - read/write `size' bytes starting\n"
	"    at offset `off' to/from memory address `addr'\n"
	"nand erase [clean] [off size] - erase `size' bytes from\n"
	"    offset `off' (entire device if not specified)\n"
	"nand bad - show bad blocks\n"
	"nand read.oob addr off size - read out-of-band data\n"
	"nand write.oob addr off size - read out-of-band data\n"
);

int do_nandboot (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *boot_device = NULL;
	char *ep;
	int dev;
	ulong cnt;
	ulong addr;
	ulong offset = 0;
	image_header_t *hdr;
	int rcode = 0;
	switch (argc) {
	case 1:
		addr = CFG_LOAD_ADDR;
		boot_device = getenv ("bootdevice");
		break;
	case 2:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = getenv ("bootdevice");
		break;
	case 3:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = argv[2];
		break;
	case 4:
		addr = simple_strtoul(argv[1], NULL, 16);
		boot_device = argv[2];
		offset = simple_strtoul(argv[3], NULL, 16);
		break;
	default:
		printf ("Usage:\n%s\n", cmdtp->usage);
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	if (!boot_device) {
		puts ("\n** No boot device **\n");
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	dev = simple_strtoul(boot_device, &ep, 16);

	if ((dev >= CFG_MAX_NAND_DEVICE) ||
	    (nand_dev_desc[dev].ChipID == NAND_ChipID_UNKNOWN)) {
		printf ("\n** Device %d not available\n", dev);
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	printf ("\nLoading from device %d: %s at 0x%lx (offset 0x%lx)\n",
		dev, nand_dev_desc[dev].name, nand_dev_desc[dev].IO_ADDR,
		offset);

	if (nand_legacy_rw (nand_dev_desc + dev, NANDRW_READ, offset,
			SECTORSIZE, NULL, (u_char *)addr)) {
		printf ("** Read error on %d\n", dev);
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	hdr = (image_header_t *)addr;

	if (ntohl(hdr->ih_magic) == IH_MAGIC) {

		print_image_hdr (hdr);

		cnt = (ntohl(hdr->ih_size) + sizeof(image_header_t));
		cnt -= SECTORSIZE;
	} else {
		printf ("\n** Bad Magic Number 0x%x **\n", ntohl(hdr->ih_magic));
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	if (nand_legacy_rw (nand_dev_desc + dev, NANDRW_READ,
			offset + SECTORSIZE, cnt, NULL,
			(u_char *)(addr+SECTORSIZE))) {
		printf ("** Read error on %d\n", dev);
		SHOW_BOOT_PROGRESS (-1);
		return 1;
	}

	/* Loading ok, update default load address */

	load_addr = addr;

	/* Check if we should attempt an auto-start */
	if (((ep = getenv("autostart")) != NULL) && (strcmp(ep,"yes") == 0)) {
		char *local_args[2];
		extern int do_bootm (cmd_tbl_t *, int, int, char *[]);

		local_args[0] = argv[0];
		local_args[1] = NULL;

		printf ("Automatic boot of image at addr 0x%08lx ...\n", addr);

		do_bootm (cmdtp, 0, 1, local_args);
		rcode = 1;
	}
	return rcode;
}

U_BOOT_CMD(
	nboot,	4,	1,	do_nandboot,
	"nboot   - boot from NAND device\n",
	"loadAddr dev\n"
);

#endif /* (CONFIG_COMMANDS & CFG_CMD_NAND) */

#endif /* CFG_NAND_LEGACY */
