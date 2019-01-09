// vim:cin
/*
 * Copyright 2013, ASUSTeK Inc.
 * All Rights Reserved.
 */
#include <common.h>
#include <command.h>
#include <asm/errno.h>
#include <malloc.h>

#if defined(CONFIG_ATH_NAND_BR)
#include <cmd_tftpServer.h>
#endif

#if defined(UBI_SUPPORT)
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <ubi_uboot.h>
#include <jffs2/load_kernel.h>
#include <nand.h>

/* common/cmd_mtdparts.c */
extern int do_mtdparts(cmd_tbl_t *cmdtp, int flag, int argc, char * argv[]);

int choose_active_eeprom_set(void);

#ifndef ROUNDUP
#define ROUNDUP(x, y)		((((x)+((y)-1))/(y))*(y))
#endif

/* To compatible with MTK's WiFi driver, put factory header to
 * last NAND Flash page of every LEB in Factory/Factory2 volume.
 */
#define EEPROM_SET_HEADER_OFFSET	(LEB_SIZE - 2*1024)
#define MAX_EEPROM_SET_LENGTH		EEPROM_SET_HEADER_OFFSET
/* two eeprom set per factory volume. two factory volume. */
#define NR_EEPROM_SETS		((CFG_UBI_FACTORY_SIZE + CFG_UBI_FACTORY2_SIZE) / LEB_SIZE)

#ifdef crc32
#undef crc32
#endif

#define FACTORY_IMAGE_MAGIC	0x46545259	/* 'F', 'T', 'R', 'Y' */
typedef struct eeprom_set_hdr_s {
	uint32_t ih_magic;	/* Image Header Magic Number = 'F', 'T', 'R', 'Y' */
	uint32_t ih_hcrc;	/* Image Header CRC Checksum    */
	uint32_t ih_hdr_ver;	/* Image Header Version Number  */
	uint32_t ih_write_ver;	/* Number of writes             */
	uint32_t ih_dcrc;	/* Image Data CRC Checksum      */
} eeprom_set_hdr_t;

/* ath_nand.c */
#define ATH_NAND_BLK_DONT_KNOW	0x0
#define ATH_NAND_BLK_GOOD	0x1
#define ATH_NAND_BLK_BAD	0x2
#define ATH_NAND_BLK_ERASED	0x3
extern void ath_nand_set_blk_state_wrap(struct mtd_info *mtd, loff_t b, unsigned state);
#else
#define MAX_EEPROM_SET_LENGTH		65536
#endif

#if defined(UBI_SUPPORT)
/* The factory_offset is used to shift address to actived copy in factory data.
 * In most cases, it should point to first copy of Factory volume.
 * If 2-nd copy of Factory volume is choosed, add LEB_SIZE to it.
 * If 1-st copy of Factory2 volume is choosed, add 2 * LEB_SIZE to it.
 * If 2-nd copy of Factory2 volume is choosed, add 3 * LEB_SIZE to it.
 */
static unsigned long factory_offset = 0;
static unsigned char *eeprom_set = NULL;
static int all_sets_damaged = 0;

const static struct ubi_vol_s {
	const char name[12];
	unsigned int size;	/* volume length in bytes */
	int type;		/* 0: static; otherwise: dynamic */
} ubi_vol[] = {
	{ "nvram",	CFG_UBI_NVRAM_SIZE, 	1 },
	{ "Factory",	CFG_UBI_FACTORY_SIZE,	1 },
	{ "Factory2",	CFG_UBI_FACTORY2_SIZE,	1 },
	{ "linux",	CFG_UBI_FIRMWARE_SIZE,	1 },
	{ "linux2",	CFG_UBI_FIRMWARE2_SIZE,	1 },
#if defined(LSDK_NART_SUPPORT)
	{ "lsdk",	CFG_UBI_LSDKRFCAL_SIZE,	1 },
#endif
	/* last volume size would be auto-resize according to free space */
	{ "jffs2",	CFG_UBI_APP_SIZE,	1 },
	{ "", 0, 0 },
};

/* Get pointer to ubi_volume by volume name.
 * @ubi:	pointer to ubi_device
 * @name:	volume name
 * @return:
 * 	NULL:	invalid parameter or not found
 *  otherwise:	success
 */
static struct ubi_volume* get_vol_by_name(struct ubi_device *ubi, const char *name)
{
	int i;
	struct ubi_volume *v = NULL;

	if (!ubi || !name)
		return v;

	for (i = 0; i < (ubi->vtbl_slots + 1) && !v; ++i) {
		if (!ubi->volumes[i] || strcmp(name, ubi->volumes[i]->name))
			continue;

		v = ubi->volumes[i];
	}

	return v;
}

/* Find specified volume is X-th volume.
 * @return:
 * 	-1:	invalid parameter
 * 	-2:	UBI device is not attached
 * 	-3:	volume not found
 *     >0:	the volume is at X-th. X start from 1
 *	0:	not defined.
 */
int get_ubi_volume_idseq_by_addr(const char *name)
{
	int i, seq = 0;
	struct ubi_device *ubi;
	struct ubi_volume *v = NULL;

	if (!name || *name == '\0')
		return -1;
	if ((ubi = get_ubi_device()) == NULL)
		return -2;
	if ((v = get_vol_by_name(ubi, name)) == NULL)
		return -3;

	for (i = 0; i < (ubi->vtbl_slots + 1); ++i) {
		if (!ubi->volumes[i] || !strcmp(name, ubi->volumes[i]->name))
			continue;

		if (ubi->volumes[i]->vol_id < v->vol_id)
			seq++;
	}

	seq++;
	return seq;
}

/* Get volume name and it's start offset address.
 * @addr:	relative address to flash
 * @offset:	if success, it would be store start offset of the volume regards to Flash.
 * @return
 *  NULL:	specified address is not belong to any volume.
 *  otherwise:	volume name
 */
char *get_ubi_volume_param(unsigned long addr, unsigned long *offset, unsigned int *size)
{
	char *found = NULL;
	unsigned long flash_offset = CFG_UBI_DEV_OFFSET;
	const struct ubi_vol_s *p;

	if (!offset || !size)
		return NULL;

	for (p = &ubi_vol[0]; !found && p->size; ++p) {
		if (addr >= flash_offset && addr < (flash_offset + p->size)) {
			found = (char*) p->name;
			*offset = flash_offset;
			*size = p->size;
			break;
		}

		flash_offset += p->size;
	}

	return found;
}

/* Initialize UBI volumes
 * @return:
 * 	0:	success
 *  otherwise:	fail
 */
int init_ubi_volumes(struct ubi_device *ubi)
{
	int id, rcode, show=0;
	struct ubi_volume *v, *j;
	unsigned int size, restore_len=0;
	const struct ubi_vol_s *p, *q;
	char tmp[] = "ffffffffXXXX", tmp1[] = "ffffffffXXX", type[] = "dynamicXXX";
	char *ubi_create_vol[] = { "ubi", "createvol", NULL, tmp, type };
	char *ubi_info_wlayout[] = { "ubi", "info", "wlayout" };

	if (!ubi) {
		char *mtdparts[] = { "mtdparts" };
		char *ubi_part[] = { "ubi", "part", "UBI_DEV" };

		/* Show MTD partition layout */
		setenv("mtdids", MTDIDS);
		setenv("mtdparts", MTDPARTS);
		do_mtdparts(NULL, 0, ARRAY_SIZE(mtdparts), mtdparts);

		printf("Initialize UBI device area!!\n");
		rcode = do_ubi(NULL, 0, ARRAY_SIZE(ubi_part), ubi_part);

		if (rcode && rcode != 1)
			return rcode;
	}

	j = get_vol_by_name(ubi, "jffs2");
#if defined(LSDK_NART_SUPPORT)
	/* If lsdk volume absent (old Flash layout) and jffs2 volume id is old, remove jffs2 volume.
	 * Later, both lsdk volume and jffs2 volume will be created again.
	 */
	if (!get_vol_by_name(ubi, "lsdk") && j && j->vol_id == 5) {
		char *ubi_remove[] = { "ubi", "remove", "jffs2" };

		printf("Old Flash layout, remove jffs2 volume!\n");
		do_ubi(NULL, 0, ARRAY_SIZE(ubi_remove), ubi_remove);
	}
#endif

	for (p = &ubi_vol[0], id = 0; p->size; ++p, ++id) {
		q = p + 1;
		v = get_vol_by_name(ubi, p->name);
		if (v) {
			char *ubi_remove[] = { "ubi", "removevol", NULL };
			char *ubi_read[] = { "ubi", "readvol", tmp, NULL, tmp1 };

			/* volume already exist. sanity check */
			if (v->vol_id != id) {
				printf("UBI volume [%s] id %d mismatch! (expect %d)\n",
					p->name, v->vol_id, id);
			}

			size = v->reserved_pebs * v->usable_leb_size;
			if (size >= p->size && q->size)
				continue;

			printf("UBI volume [%s] size %x smaller than %x!\n", p->name, size, p->size);
			/* Resize linux* volume only. */
			if (strncmp(p->name, "linux", 5))
				continue;

			/* backup content. */
			restore_len = 0;
			sprintf(tmp, "%x", CFG_LOAD_ADDR);
			ubi_read[3] = (char*) p->name;
			sprintf(tmp1, "%x", size);
			rcode = do_ubi(NULL, 0, ARRAY_SIZE(ubi_read), ubi_read);

			if (rcode && rcode != 1) {
				printf("Backup volume %s fail. (ret %d)\n", p->name, rcode);
				continue;
			}
			restore_len = size;

			/* remove jffs2 volume, if exist. */
			if (j) {
				ubi_remove[2] = "jffs2";
				rcode = do_ubi(NULL, 0, ARRAY_SIZE(ubi_remove), ubi_remove);

				if (rcode && rcode != 1) {
					printf("Remove volume jffs2 fail. (ret %d)\n", rcode);
					continue;
				}
				j = NULL;
			}

			/* remove old and small UBI volume. */
			ubi_remove[2] = (char*) p->name;
			rcode = do_ubi(NULL, 0, ARRAY_SIZE(ubi_remove), ubi_remove);

			if (rcode && rcode != 1) {
				printf("Remove volume %s fail. (ret %d)\n", p->name, rcode);
				continue;
			}
		}

		show++;
		ubi_create_vol[2] = (char*) p->name;
		if (!strcmp(p->name, "jffs2"))
			strcpy(tmp, "0");	/* auto-resize */
		else
			sprintf(tmp, "%x", p->size);
		/* If next volume size is zero, assign all available
		 * space to current volume instead of specified size.
		 */
		if (q->size == 0)
			strcpy(tmp, "0");
		if (p->type)
			strcpy(type, "dynamic");
		else
			strcpy(type, "static");
		rcode = do_ubi(NULL, 0, ARRAY_SIZE(ubi_create_vol), ubi_create_vol);
		if (rcode < 0)
			printf("Create volume %s fail. rcode 0x%x\n", p->name, rcode);

		if (!p->type || restore_len) {
			char *ubi_write_vol[] = { "ubi", "writevol", tmp, (char*)p->name, tmp1 };

			sprintf(tmp, "%x", CFG_LOAD_ADDR);
			if (!restore_len) {
				sprintf(tmp1, "%x", p->size);
				memset((void*)CFG_LOAD_ADDR, 0x00, p->size);
			} else {
				sprintf(tmp1, "%x", restore_len);
			}
			do_ubi(NULL, 0, ARRAY_SIZE(ubi_write_vol), ubi_write_vol);
		}
	}

	if (show) {
		printf("Latest UBI volumes layout.\n");
		do_ubi(NULL, 0, ARRAY_SIZE(ubi_info_wlayout), ubi_info_wlayout);
	}

	return 0;
}
#endif

#if defined(UBI_SUPPORT) || defined(CFG_ENV_IS_IN_NAND)
int ranand_block_isbad(loff_t offs)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];

	return mtd->block_isbad(mtd, offs);

}

/** Erase blocks.
 * @offs:	block alilgned address
 * @len:	block aligned length
 * @return:
 * 	0:	success
 *  -EIO:	I/O error
 */
int ranand_erase(unsigned int offs, int len)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	int r;
	struct erase_info instr;

	if (!mtd)
		return -ENODEV;

	memset(&instr, 0, sizeof(instr));
	instr.mtd = mtd;
	instr.addr = offs;
	instr.len = len;
	r = mtd->erase(mtd, &instr);

	return r;
}

/** Write data to NAND Flash
 * @buffer:	pointer to data
 * @to:		start offset address of NAND Flash to be written
 * @datalen:	number of bytes to be written
 * @return:
 *  >=0		length of writted bytes
 *  < 0		error occurs
 *  -EIO:	write page fail.
 *  -EAGAIN:	write-disturb, suggested caller to erase and write again.
 *  -ENODEV:	No such device.
 */
int ranand_write(unsigned char *buf, unsigned int to, int datalen)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	int r;
	size_t retlen;

	if (!mtd)
		return -ENODEV;

	r = mtd->write(mtd, to, datalen, &retlen, buf);
	if (r < 0)
		return r;

	return retlen;
}

/* read data from NAND Flash
 * @return:
 *     >0:	success, number of read bytes.
 *     =0:	none of any bytes are read
 *     -1:	invalid parameter
 *  otherwise:	not defined.
 *  If return value is less than datalen, it may be caused by uncorrectable error.
 */
int ranand_read(unsigned char *buf, unsigned int from, int datalen)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	int r;
	size_t retlen;

	if (!mtd)
		return -ENODEV;

	r = mtd->read(mtd, from, datalen, &retlen, buf);
	if (r < 0)
		return r;

	return retlen;
}

/* erase and write to nand flash
 * @return:
 *    >=0:	success, number of bytes be written to flash
 *     -1:	invalid parameter
 *  otherwise:	error
 */
int ranand_erase_write(unsigned char *buf, unsigned int offs, int count)
{
	int r;

	r = ranand_erase(offs, count);
	if (r)
		return r;
	return ranand_write(buf, offs, count);
}
#endif	/* UBI_SUPPORT || CFG_ENV_IS_IN_NAND */

#if defined(CONFIG_ATH_NAND_BR)
#define STAGE1_MAGIC	0xbd004000
#define BL_MAGIC	0xa0100000

/* Prepare large enough buffer to hold bootloader code, even it becomes big as much as whole bootbootloader area. */
struct bootloader_desc {
	unsigned int	offset;			/* start offset of a bootloader code in flash */
	unsigned int	boundary;		/* block-aligned end offset of a bootloader code in flash */
	unsigned int	len;			/* length of bootloader code */
	unsigned int	blk_len;		/* length of bootloader code and aligned to block size boundary */
	uint32_t	failed;			/* number of uncorrectable error */
	uint32_t	corrected;		/* number of correctable error */
	unsigned int	crc_error;		/* 0: good bootloader code; otherwise: CRC error */
	char		name[15];		/* such as: bootloader-0, bootloader-1, etc */
	unsigned char	*code;			/* pointer to start address of this copy bootloader code in RAM */
};

#define MAX_NR_BOOTLOADER	(6 + 1)
struct bootloader_loc {
	unsigned int	count;			/* number of all items */
	unsigned int	nr_blk_read;		/* number of blocks are readed to buffer */
	struct bootloader_desc *good;		/* pointer to descriptor of good bootloader code */
	struct bootloader_desc desc[MAX_NR_BOOTLOADER];
	unsigned char code[CFG_BOOTLOADER_SIZE]  __attribute__((aligned(4)));	/* buffer to hold all stage2 code. */
};

typedef struct {
	uint32_t	ep,	/* entry point */
			la,	/* load addr */
			sz,	/* firmware size */
			cs;	/* firmware crc checksum */
} nf_fw_hdr_t;

static struct bootloader_loc g_bl;

/**
 * Do CRC32 check on a bootloader image
 * @hdr:	pointer to RAM address of header of a bootloader image
 * @type:	0: check magic number of header 1 and header 2 only
 * 		1: check all items
 * @return:
 * 	0:	successful
 *     -1:	invalid param
 *     -2:	invalid stage1 magic number
 *     -3:	invalid stage1 length
 *     -4:	invalid bootloader magic number
 *     -5:	invalid bootloader length
 *     -6:	stage1 checksum mismatch
 *     -7:	stage1 checksum mismatch
 */
static int check_bootloader_image(unsigned char* code, int type)
{
	uint32_t crc;
	unsigned int addr = (unsigned int) code;
	nf_fw_hdr_t *hdr = (nf_fw_hdr_t*) code, *hdr2;

	if (!code)
		return -1;

	if (hdr->ep != STAGE1_MAGIC || hdr->la != STAGE1_MAGIC)
		return -2;

	/* stage1 code can't exceed 16KB. */
	if (hdr->sz >= 0x4000)
		return -3;

	hdr2 = (nf_fw_hdr_t*) (code + hdr->sz);
	if (hdr2->ep != BL_MAGIC || hdr2->la != BL_MAGIC)
		return -4;

	if (hdr2->sz > CFG_MAX_BOOTLOADER_BINARY_SIZE)
		return -5;

	if (!type)
		return 0;

	if ((crc = __checksum((unsigned int*)(code + sizeof(*hdr)), hdr->sz - sizeof(*hdr), 0) != hdr->cs))
		return -6;

	addr = (unsigned int)(code + hdr->sz);
	if ((crc = __checksum((unsigned int*)(addr + sizeof(*hdr2)), hdr2->sz - sizeof(*hdr2), 0) != hdr2->cs))
		return -7;

	return 0;
}

/**
 * Fill a bootloader descriptor.
 * @desc:
 * @id:
 * @offset:
 * @boundary:
 * @code:
 * @failed:
 * @corrected:
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 */
static int __fill_bootloader_desc(struct bootloader_desc *desc, unsigned int id, unsigned int offset, unsigned int boundary, unsigned char *code, uint32_t failed, uint32_t corrected)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	nf_fw_hdr_t *hdr = (nf_fw_hdr_t*) code, *hdr2;

	if (!mtd || !desc || offset >= CFG_BOOTLOADER_SIZE || boundary > CFG_BOOTLOADER_SIZE || !code || code >= (unsigned char*) CFG_FLASH_BASE) {
		debug("%s: invalid parameter (desc %p, id %x, offset %x, boundary %x, code %p, failed %x, corrected %x)\n",
			__func__, desc, id, offset, boundary, code, failed, corrected);
		return -1;
	}

	sprintf(desc->name, "bootloader-%d", id);
	desc->offset = offset;
	desc->boundary = boundary;
	desc->code = code;
	desc->failed = failed;
	desc->corrected = corrected;

	desc->crc_error = 1;
	desc->len = CFG_MAX_BOOTLOADER_BINARY_SIZE;
	if (!check_bootloader_image(code, 1)) {
		hdr2 = (nf_fw_hdr_t*) (((unsigned char*)hdr) + hdr->sz);
		desc->len = hdr->sz + hdr2->sz;
		desc->crc_error = 0;
	}
	desc->blk_len = (desc->len + (mtd->erasesize - 1)) & ~(mtd->erasesize - 1);

	return 0;
}

/**
 * Try to assemble a good bootloader from fragments.
 * @bl:		pointer to a struct bootloader_loc
 * @type:	0: fast method.
 *		   assemble bootloader code depends on partition defined in bl parameter.
 * 		otherwise: aggresive method.
 * 		   ogmpre partition defined in bl parameter.
 * @return:
 * 	0:	success, bl->good points to a descriptor of good bootloader code.
 * 	1:	fail.
 *     -1:	invalid parameter
 *  otherwise:	not defined
 */
static int assemble_bootloader(struct bootloader_loc *bl, int type)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	static unsigned char s2_buf[CFG_BOOTLOADER_SIZE]  __attribute__((aligned(4)));
	struct bootloader_desc *desc;
	unsigned char *p, *code;
	int lvar[(CFG_BOOTLOADER_SIZE + mtd->erasesize - 1) / mtd->erasesize];
	int i, r, v, val, base, *q, tmp, skip, bound, nr_blks = 2;
	int nr_copy = bl->count;
	unsigned int o;

	if (!bl || !mtd)
		return -1;

	/* take first reasonable image length of bootloader */
	for (i = 0; i < bl->count; ++i) {
		tmp = bl->desc[i].blk_len / mtd->erasesize;
		if (tmp <= 0 || tmp >= (sizeof(lvar)/sizeof(lvar[0])))
			continue;

		nr_blks = tmp;
		break;
	}

	/* use all read blocks to assemble bootloader */
	if (type)
		nr_copy = bl->nr_blk_read;

	bound = 1;
	for (i = 0; i < nr_blks; ++i)
		bound *= nr_copy;
	printf("assemble bootloader: %d,%d/%d/%d ...\n", type, nr_blks, nr_copy, bound);
	desc = &bl->desc[MAX_NR_BOOTLOADER - 1];
	for (val = 0, base = nr_copy; !bl->good && val < bound; val++) {
		/* calculate loop-variables */
		memset(lvar, 0, sizeof(lvar));
		for (v = val, q = &lvar[nr_blks - 1]; v > 0; v /= base)
			*q-- = v % base;

		/* if all loop-variables are equal to each other, skip. */
		for (v = lvar[0], i = 1, q = &lvar[i], skip = 1; skip && i < nr_blks; ++i, ++q) {
			if (v == *q)
				continue;

			skip = 0;
		}
		if (skip)
			continue;

		skip = 0;
		for (i = 0, o = 0, p = s2_buf, q = &lvar[i];
		     !skip && i < nr_blks;
		     ++i, o += mtd->erasesize, p += mtd->erasesize, ++q)
		{
			if (!type)
				code = bl->desc[*q].code + o;
			else
				code = bl->code + (*q) * mtd->erasesize;

			if (!i && (r = check_bootloader_image(code, 0)) != 0) {
				/* first block, checksum must be right. */
				skip = 1;
				continue;
			}
			if (type && i && (r = check_bootloader_image(code, 0)) == 0) {
				/* not first block, skip any block associated with header */
				skip = 1;
				continue;
			}

			memcpy(p, code, mtd->erasesize);
		}
		if (skip)
			continue;

		/* create dummy bootloader code descriptor and check image */
		__fill_bootloader_desc(desc, MAX_NR_BOOTLOADER - 1, 0, CFG_BOOTLOADER_SIZE, s2_buf, 0, 0);
		if (desc->crc_error)
			continue;

		debug("Assemble good bootloader code from multiple fragments successful.\n");
		bl->good = desc;
	}

	return (bl->good)? 0:1;
}

/**
 * Find all bootloader code, include bad one, and record to a table.
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	read error
 *    -12:	-ENOMEM, allocate memory fail.
 *  otherwise:	error
 */
int ranand_locate_bootloader(struct bootloader_loc *bl)
{
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	int i, ret;
	struct bootloader_desc *desc;
	struct mtd_ecc_stats stats;
	uint32_t failed, corrected;
	unsigned int o, offset, l, bl_len, space;
	unsigned char *code, *buf;
	nf_fw_hdr_t *hdr, *hdr2;

	if (!bl || !mtd)
		return -1;

	memset(bl, 0, sizeof(struct bootloader_loc));
	for (offset = 0, o = space = 0, failed = corrected = 0,
		desc = &bl->desc[0], bl_len = CFG_MAX_BOOTLOADER_BINARY_SIZE,
		buf = code = bl->code;
	     offset < CFG_BOOTLOADER_SIZE;
	     offset += mtd->erasesize)
	{
		if (space >= bl_len) {
			if (o != offset) {
				__fill_bootloader_desc(desc, bl->count, o, offset, code, failed, corrected);
				desc++;
				bl->count++;
			}

			code = buf;
			o = space = 0;
			failed = corrected = 0;
			bl_len = CFG_MAX_BOOTLOADER_BINARY_SIZE;
		}
		if (ranand_block_isbad(offset))
			continue;

		if (!o && bl->count && offset)
			o = offset;
		hdr = (nf_fw_hdr_t*) buf;
		memset(hdr, 0, sizeof(*hdr));
		stats = mtd->ecc_stats;
		ret = ranand_read(buf, offset, mtd->erasesize);
		failed += mtd->ecc_stats.failed - stats.failed;
		corrected += mtd->ecc_stats.corrected - stats.corrected;
		if ((hdr->ep == STAGE1_MAGIC && hdr->la == STAGE1_MAGIC) || space >= bl_len) {
			if (o != offset) {
				__fill_bootloader_desc(desc, bl->count, o, offset, code, failed, corrected);
				desc++;
				bl->count++;
			}

			code = buf;
			o = space = 0;
			failed = corrected = 0;
			bl_len = CFG_MAX_BOOTLOADER_BINARY_SIZE;
			if (hdr->ep == STAGE1_MAGIC && hdr->la == STAGE1_MAGIC) {
				hdr2 = (nf_fw_hdr_t*)(buf + hdr->sz);
				printf("%8x: hdr1 [ 0x%08x,%08x,%08x,%08x ], hdr2 [ 0x%08x,%08x,%08x,%08x ]\n",
					offset, hdr->ep, hdr->la, hdr->sz, hdr->cs, hdr2->ep, hdr2->la, hdr2->sz, hdr2->cs);
				o = offset;
				l = sizeof(*hdr) + hdr->sz + sizeof(*hdr2) + hdr2->sz;
				if (l < bl_len)
					bl_len = l;
			}
		}
		space += mtd->erasesize;
		if (ret < mtd->erasesize)
			continue;
		buf += mtd->erasesize;
	}
	bl->nr_blk_read = ((unsigned int)(buf - bl->code)) / mtd->erasesize;
	if (o != offset) {
		__fill_bootloader_desc(desc, bl->count, o, offset, code, failed, corrected);
		bl->count++;
	}

	/* looking for and choose a good bootloader code */
	bl->good = NULL;
	for (i = 0, desc = &bl->desc[0]; i < bl->count; ++i, ++desc) {
		char *s = "unknown status";
		unsigned int l = desc->boundary - desc->offset;

		if (desc->crc_error && desc->len <= l)
			s = "CRC error";
		else if (desc->crc_error && desc->len > l)
			s = "fragment";
		else if (desc->failed)
			s = "Uncorrectable ECC error";
		else if (desc->corrected)
			s = "Correctable ECC error";
		else if (!desc->crc_error)
			s = "OK";
		printf("%s: 0x%x-%x, len %x/%x, buf %p: %s\n",
			desc->name, desc->offset, desc->boundary,
			desc->len, desc->blk_len, desc->code, s);
		if (!bl->good && !desc->crc_error && desc->code) {
			bl->good = desc;
		}
	}

	/* Try to assembly a good bootloader from several fragments. */
	if (!bl->good && bl->nr_blk_read > 1)
		assemble_bootloader(bl, 1);

	if (bl->good)
		printf("choose %s at %p, length %x bytes\n",
			bl->good->name, bl->good->code, bl->good->len);

	return 0;
}

/**
 * Check bootloader and fix it if necessary.
 * @return:
 * 	0:	success, nothing happen
 *  negative:	error
 *  positive:	reprogram bootloader success
 */
int ranand_check_and_fix_bootloader(void)
{
	int i, ret, prog = 0;
	struct bootloader_loc *bl = &g_bl;
	struct bootloader_desc *d;

	ret = ranand_locate_bootloader(bl);
	if (ret) {
		debug("%s: locate bootloader fail! (ret %d)\n", __func__, ret);
		return -1;
	}

	if (!bl->count || !bl->good) {
		debug("%s: count %d, good %p skip\n", __func__, bl->count, bl->good);
		return -2;
	}

	for (i = 0, d = &bl->desc[0]; !prog && i < bl->count; ++i, ++d) {
		if (d->failed || d->corrected) {
			prog++;
			continue;
		}

		if (d->crc_error && ((d->boundary - d->offset) >= bl->good->len)) {
			prog++;
			continue;
		}

	}

	if (!prog)
		return 0;

	d = bl->good;
	ret = program_bootloader((ulong)d->code, (ulong)d->len);

	return (!ret)? 1:ret;
}
#endif	/* CONFIG_ATH_NAND_BR */

#if defined(UBI_SUPPORT) && defined(LSDK_NART_SUPPORT)
/**
 * Find available block in UBI_DEV partition.
 * @return:
 * 	>0:	offset of available block. (should be 1MB ~ 126MB)
 *      <=0:	fail
 */
static loff_t find_free_block(void)
{
	int r;
	loff_t o, ret = 0;
	size_t retlen;
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	unsigned char tmp[2 * 1024] __attribute__ ((aligned(4)));
	unsigned char *p = &tmp[0];

	if (!mtd)
		return -ENODEV;

	if (mtd->writesize > sizeof(tmp) && !(p = malloc(mtd->writesize)))
		return -ENOMEM;
	/* looking for available block from tail and write layout volume there. */
	for (o = mtd->size - RFCALDATA_REGION_SIZE - mtd->erasesize;
	     !ret && o >= 0x100000;	/* start offset of UBI_DEV */
	     o -= mtd->erasesize)
	{
		if ((r = mtd->read(mtd, o + mtd->writesize, mtd->writesize, &retlen, tmp)) < 0 || retlen != mtd->writesize)
			continue;

		if (!memcmp(tmp, "UBI!", 4))	/* skip block that have valid VID header */
			continue;

		ret = o;
	}

	if (p != &tmp[0])
		free(p);

	return ret;
}
/**
 * Move all block with valid VID header in last 1MB of NAND Flash to UBI_DEV partition.
 * @return:
 * 	0:	success
 *  otherwise:	fail
 */
static int move_leb_to_ubi(void)
{
	loff_t o, o1;
	size_t retlen;
	int r, cnt = 0;
	struct erase_info instr;
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	unsigned char buf[128 * 1024] __attribute__ ((aligned(4)));
	unsigned char *p = &buf[0];

	if (!mtd)
		return -ENODEV;

	if (mtd->erasesize > sizeof(buf) && !(p = malloc(mtd->erasesize)))
		return -ENOMEM;
	/* looking for all block with valid EC header and VID header and move them to UBI_dev. */
	for (o = mtd->size - mtd->erasesize;
	     o >= (mtd->size - RFCALDATA_REGION_SIZE);
	     o -= mtd->erasesize)
	{
		if ((r = mtd->read(mtd, o, mtd->erasesize, &retlen, buf)) < 0 || retlen != mtd->erasesize)
			continue;

		if (memcmp(buf, "UBI#", 4) ||			/* EC header */
		    memcmp(buf + mtd->writesize, "UBI!", 4))	/* VID header */
			continue;

		/* Copy block with valid EC/VID header to new block. */
		if ((o1 = find_free_block()) <= 0) {
			debug("Can't find free block to store LEB!\n");
			continue;
		}

		memset(&instr, 0, sizeof(instr));
		instr.mtd = mtd;
		instr.addr = o1;
		instr.len = mtd->erasesize;
		if ((r = mtd->erase(mtd, &instr)) < 0) {
			printf("%s: erase offset %08lx fail. (r = %d)\n", __func__, (ulong)o1, r);
			continue;
		}

		if ((r = mtd->write(mtd, o1, mtd->erasesize, &retlen, buf)) < 0 || retlen != mtd->erasesize)
			continue;
		printf("Copy LEB from offset %08lx to %08lx\n", (ulong)o, (ulong)o1);

		/* Destroy VID header of origin block. */
		memset(buf + mtd->writesize, 0, mtd->writesize);
		memset(&instr, 0, sizeof(instr));
		instr.mtd = mtd;
		instr.addr = o;
		instr.len = mtd->erasesize;
		if ((r = mtd->erase(mtd, &instr)) < 0) {
			printf("%s: erase offset %08lx fail. (r = %d)\n", __func__, (ulong)o, r);
			continue;
		}

		if ((r = mtd->write(mtd, o, mtd->erasesize, &retlen, buf)) < 0 || retlen != mtd->erasesize)
			continue;

		debug("Destroy VID header of LEB @ offset %08lx\n", (ulong)o);
		cnt++;
	}

	if (p != &buf[0])
		free(p);

	if (cnt <= 0)
		return -1;

	return 0;
}
#endif

#if defined(UBI_SUPPORT)
/**
 * Erase all software marked bad-block
 * @return:
 *    >=0:	success
 *     <0:	any error
 */
int remove_software_marked_bad_block(void)
{
	int ret, ok = 0, fail = 0;
	ulong off;
	nand_info_t *nand;
	u_char buf[4], buf1[4];

	nand = &nand_info[nand_curr_device];
	printf("Scanning software marked bad-block ...\n");
	for (off = 0; off < nand->size; off += nand->erasesize) {
		off &= ~nand->erasesize_mask;	/* aligned to block address */
		if ((ret = nand_read_raw(nand, buf, off, 0, sizeof(buf))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off, ret);
			fail++;
			continue;
		}
		if ((ret = nand_read_raw(nand, buf1, off + nand->writesize, 0, sizeof(buf1))) != 0) {
			printf("read oob from %lx fail. (ret %d)\n", off + nand->writesize, ret);
			fail++;
			continue;
		}
		if (buf[0] == 0xFF && buf1[0] == 0xFF) {
			continue;
		} else if ((buf[0] != SW_BAD_BLOCK_INDICATION && buf[0] != 0xFF) ||
			   (buf1[0] != SW_BAD_BLOCK_INDICATION && buf1[0] != 0xFF))
		{
			printf("skip unknown bad-block indication byte at %08lx. (mark %02x,%02x)\n", off, buf[0], buf1[0]);
			continue;
		}
		ath_nand_set_blk_state_wrap(nand, off, ATH_NAND_BLK_GOOD);	/* cheat mtd->erase */
		if ((ret = nand_erase(nand, off, nand->erasesize)) != 0) {
			printf("erase offset %lx fail. (ret %d)\n", off, ret);
			fail++;
			continue;
		}

		printf("Erase software marked bad-block at %08lx successful.\n", off);
		ok++;
	}

	if (ok > 0)
		return ok;
	else if (fail > 0)
		return -1;
	return 0;
}
#endif

/**
 * Initialize Flash layout.
 * @return:
 * 	0:	success
 *  otherwise:	error
 */
int ra_flash_init_layout(void)
{
#if defined(UBI_SUPPORT)
	int r;
	struct ubi_device *ubi;
	char *ubi_part[] = { "ubi", "part", "UBI_DEV" };
	char *ubi_info_layout[] = { "ubi", "info", "wlayout" };

	eeprom_set = malloc(LEB_SIZE);
	if (!eeprom_set)
		printf("Allocate %d bytes memory for eeprom_set fail!\n", LEB_SIZE);

	setenv("mtdids", MTDIDS);
	setenv("mtdparts", MTDPARTS);
	r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_part), ubi_part);

#if defined(LSDK_NART_SUPPORT)
	if (r == EINVAL && !move_leb_to_ubi())
		r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_part), ubi_part);
#endif

	if (r) {
		printf("Mount UBI device fail. (r = %d)\n", r);
		if (remove_software_marked_bad_block() >= 0)
			r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_part), ubi_part);

		if (r)
			return -ENOENT;
	}
	do_ubi(NULL, 0, ARRAY_SIZE(ubi_info_layout), ubi_info_layout);

	ubi = get_ubi_device();
	r = init_ubi_volumes(ubi);
	if (r)
		return -ENOENT;

	choose_active_eeprom_set();

#elif defined(CFG_ENV_IS_IN_NAND)
#else
#endif
	return 0;
}

/**
 * Read data from NAND/SPI/Parallel_NOR Flash.
 * @buf:	buffer address
 * @addr:	Absolute address to read (include CFG_FLASH_BASE).
 * @len:	length to read
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *  -ENOENT:	UBI not ready
 *     -3:	address not belongs to UBI device
 *     -5:	error or read length is not equal to len
 *  otherwise:	error
 */
int ra_flash_read(uchar * buf, ulong addr, ulong len)
{
	int ret = 0;
#if defined(UBI_SUPPORT) || defined(CFG_ENV_IS_IN_NAND)
	unsigned int flash_offset = addr - CFG_FLASH_BASE;
#endif

	if (!buf || !len || addr < CFG_FLASH_BASE) {
		debug("%s(): invalid parameter. buf %p addr 0x%08lx len 0x%08lx\n",
			__func__, buf, addr, len);
		return -1;
	}

#if defined(UBI_SUPPORT)
	if (flash_offset >= CFG_UBI_DEV_OFFSET) {
		/* Redirect to UBI volume */
		char *vol;
		char addr_buf[] = "ffffffffXXX", len_buf[] = "ffffffffXXX", vol_offset_buf[] = "ffffffffXXX";
		char *ubi_readvol[] = { "ubi", "readvol", addr_buf, NULL, len_buf, vol_offset_buf };
		unsigned int size;
		unsigned long vol_offset;
		const struct ubi_device *ubi = get_ubi_device();

		if (!ubi)
			return -ENOENT;

		vol = get_ubi_volume_param(flash_offset, &vol_offset, &size);
		if (!vol) {
			printf("%s: addr %08lx not belongs to any volume\n", __func__, addr);
			return -3;
		}
		ubi_readvol[3] = vol;
		sprintf(addr_buf, "%x", buf);
		sprintf(len_buf, "%x", len);
		sprintf(vol_offset_buf, "%x", flash_offset - vol_offset);
		return do_ubi(NULL, 0, ARRAY_SIZE(ubi_readvol), ubi_readvol);
	} else {
		ret = ranand_read(buf, flash_offset, len);
	}
#elif defined(CFG_ENV_IS_IN_NAND)
	ret = ranand_read(buf, flash_offset, len);
#else
	memmove(buf, (void*) addr, len);
	ret = len;
#endif

	if (ret > 0 && ret == len)
		return 0;
	else
		return -5;
}


/**
 * Write data to NAND/SPI/Parallel_NOR Flash.
 * @buf:	buffer address
 * @addr:	Absolute address to read. (include CFG_FLASH_BASE)
 * @len:	length to read
 * @prot:	Unprotect/protect sector. (Parallel NOR Flash only)
 * @return:
 * 	0:	success
 *  -ENOENT:	UBI not ready
 *  otherwise:	error
 ******************************************************************
 * WARNING
 ******************************************************************
 * For Parallel NOR Flash, you must specify sector-aligned address.
 */
int ra_flash_erase_write(uchar * buf, ulong addr, ulong len, int prot)
{
	int ret = 0;
#if defined(UBI_SUPPORT) || defined(CFG_ENV_IS_IN_NAND)
	unsigned int flash_offset = addr - CFG_FLASH_BASE;
#endif
#if defined(CFG_ENV_IS_IN_FLASH)
	int rc;
	ulong e_end;
#endif

	if (!buf || !len || addr < CFG_FLASH_BASE) {
		debug("%s(): invalid parameter. buf %p addr 0x%08lx len 0x%08lx\n",
			__func__, buf, addr, len);
		return -1;
	}

#if defined(UBI_SUPPORT)
	if (flash_offset >= CFG_UBI_DEV_OFFSET) {
		/* Redirect to UBI volume */
		int r;
		char *vol;
		char addr_buf[] = "ffffffffXXX", len_buf[] = "ffffffffXXX";
		char *ubi_readvol[] = { "ubi", "readvol", addr_buf, NULL, len_buf };
		char *ubi_writevol[] = { "ubi", "writevol", addr_buf, NULL, len_buf };
		unsigned char *tmp;
		unsigned int size;
		unsigned long vol_offset, o;
		const struct ubi_device *ubi = get_ubi_device();

		if (!ubi)
			return -ENOENT;

		vol = get_ubi_volume_param(flash_offset, &vol_offset, &size);
		if (!vol) {
			printf("%s: addr %08lx not belongs to any volume\n", __func__, addr);
			return -2;
		}

		/* For Factory volume, always read whole volume, update data, and write whole volume. */
		o = flash_offset - vol_offset;
		if (!strcmp(vol, "Factory") || !strcmp(vol, "Factory2")) {
			/* Read whole volume,  update data, write back to volume. */
			tmp = malloc(size);
			if (!tmp) {
				printf("%s: allocate %lu bytes buffer fail.\n", __func__, size);
				return -ENOMEM;
			}

			if (len < size) {
				ubi_readvol[3] = vol;
				sprintf(addr_buf, "%x", tmp);
				sprintf(len_buf, "%x", size);
				r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_readvol), ubi_readvol);
				if (r) {
					printf("%s: read volume [%s] fail. (r = %d)\n", __func__, vol, r);
					free(tmp);
					return r;
				}
			}

			memcpy(tmp + o, buf, len);
			ubi_writevol[3] = vol;
			sprintf(addr_buf, "%x", tmp);
			sprintf(len_buf, "%x", size);
			r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_writevol), ubi_writevol);
			if (r) {
				printf("%s: write volume [%s] fail. (r = %d)\n", __func__, vol, r);
				free(tmp);
				return r;
			}

			free(tmp);
		} else {
			if (o) {
				printf("Start offset address have to be zero!\n");
				return -EINVAL;
			}

			ubi_writevol[3] = vol;
			sprintf(addr_buf, "%x", buf);
			sprintf(len_buf, "%x", len);
			r = do_ubi(NULL, 0, ARRAY_SIZE(ubi_writevol), ubi_writevol);
			if (r) {
				printf("%s: write volume [%s] fail. (r = %d)\n", __func__, vol, r);
				return r;
			}
			return 0;
		}
	} else {
		ranand_set_sbb_max_addr(CFG_UBI_DEV_OFFSET);
		ret = ranand_erase_write(buf, flash_offset, len);
		if (ret == len)
			ret = 0;
		ranand_set_sbb_max_addr(0);
	}
#elif defined(CFG_ENV_IS_IN_NAND)
	ret = ranand_erase_write(buf, flash_offset, len);
	if (ret == len)
		ret = 0;
#else
	e_end = addr + len - 1;
	if (get_addr_boundary(&e_end))
		return -2;

	if (prot)
		flash_sect_protect(0, addr, e_end);

	flash_sect_erase(addr, e_end);
	rc = flash_write((char*) buf, addr, len);
	if (rc)
		printf("%s: buf %p addr %lx len %lx fail. (code %d)\n", __func__, buf, addr, len, rc);

	if (prot)
		flash_sect_protect(1, addr, e_end);
#endif

	return ret;
}


/**
 * Erase NAND/SPI/Parallel_NOR Flash.
 * @addr:	Absolute address to read.
 * 		If addr is less than CFG_FLASH_BASE, ignore it.
 * @len:	length to read
 * @return:
 * 	0:	success
 *  otherwise:	error
 ******************************************************************
 * WARNING
 ******************************************************************
 * For Parallel NOR Flash, you must specify sector-aligned address.
 */
int ra_flash_erase(ulong addr, ulong len)
{
	int ret = 0;
#if defined(UBI_SUPPORT) || defined(CFG_ENV_IS_IN_NAND)
	unsigned int flash_offset = addr - CFG_FLASH_BASE;
#endif
#if defined(CFG_ENV_IS_IN_FLASH)
	ulong e_end;
#endif

	if (!len || addr < CFG_FLASH_BASE) {
		debug("%s(): invalid parameter. addr 0x%08lx len 0x%08lx\n",
			__func__, addr, len);
		return -1;
	}

#if defined(UBI_SUPPORT)
	if (flash_offset < CFG_UBI_DEV_OFFSET)
		ret = ranand_erase(flash_offset, len);
#elif defined(CFG_ENV_IS_IN_NAND)
	ret = ranand_erase(flash_offset, len);
#else
	e_end = addr + len - 1;
	if (get_addr_boundary(&e_end))
		return -2;

	flash_sect_erase(addr, e_end);
#endif

	return ret;
}

#if ! defined(UBOOT_STAGE1)
#if defined(UBI_SUPPORT)
/* Check EEPROM set checksum in RAM.
 * @buf:	pointer to one copy of EEPROM set.
 * 		length of the EEPROM set must greater than or equal to LEB_SIZE
 * @return:
 *     >=0:	OK, write times of the EEPROM set
 * 	-1:	Invalid parameter.
 * 	-2:	Invalid magic number.
 * 	-3:	Invalid header checksum.
 * 	-4:	Invalid data checksum
 *  otherwise:	Not defined.
 */
static int check_eeprom_set_checksum(unsigned char *buf)
{
	unsigned long hcrc, checksum;
	eeprom_set_hdr_t header, *hdr = &header;

	if (!buf)
		return -1;

	memcpy(hdr, buf + EEPROM_SET_HEADER_OFFSET, sizeof(eeprom_set_hdr_t));
	if (hdr->ih_magic != htonl(FACTORY_IMAGE_MAGIC))
		return -2;

	hcrc = ntohl(hdr->ih_hcrc);
	hdr->ih_hcrc = 0;
	/* check header checksum */
	checksum = crc32(0, (const unsigned char*) hdr, sizeof(eeprom_set_hdr_t));
	if (hcrc != checksum) {
		debug("Header checksum mismatch. (%x/%x)\n", hcrc, checksum);
		return -3;
	}

	/* check image checksum */
	checksum = crc32(0, buf, EEPROM_SET_HEADER_OFFSET);
	if (ntohl(hdr->ih_dcrc) != checksum) {
		debug("Data checksum mismatch. (%x/%x)\n", ntohl(hdr->ih_dcrc), checksum);
		return -4;
	}

	return ntohl(hdr->ih_write_ver);
}

/* Check all EEPROM set in Factory, Factory2 volume and select correct and latest one.
 * @return:
 * 	0:	Success. Latest and correct EEPROM set is copied to RAM.
 *     -1:	All EEPROM set is damaged. (always choose first EEPROM set)
 */
int choose_active_eeprom_set(void)
{
	int i, r, w, active_set = -1, ret = 0;
	unsigned int o, max_w = 0;
	unsigned char buf[LEB_SIZE];

	for (i = 0, o = 0; i < NR_EEPROM_SETS; ++i, o += LEB_SIZE) {
		r = ra_flash_read(buf, CFG_FACTORY_ADDR + o, LEB_SIZE);
		if (r) {
			printf("Read data fail at 0x%x\n", i * LEB_SIZE);
			continue;
		}
		w = check_eeprom_set_checksum(buf);

		if (w >= 0) {
			printf("EEPROM set %d: OK (version %d)\n", i, w);
			if (w > max_w || (active_set < 0 && !max_w)) {
				active_set = i;
				max_w = w;
			}
		} else {
			printf("EEPROM set %d: DAMAGED ", i);
			if (w == -2)
				printf("(Invalid magic number)\n");
			else if (w == -3)
				printf("(Invalid header checksum)\n");
			else if (w == -4)
				printf("(Invalid data checksum)\n");
			else
				printf("(w = %d)\n", w);
		}
	}

	/* All EEPROM set is damaged. choose first one. */
	all_sets_damaged = 0;
	if (active_set < 0) {
		ret = -1;
		active_set = 0;
		all_sets_damaged = 1;
	}

	factory_offset = active_set * LEB_SIZE;
	r = ra_flash_read(eeprom_set, CFG_FACTORY_ADDR + factory_offset, LEB_SIZE);
	printf("Select EEPROM set %d at offset 0x%lx.\n", active_set, factory_offset);

	return ret;
}

/* Reload EEPROM set if necessary. If active EEPROM set in Flash is damaged too, choose again.
 * @return:
 * 	0:	success
 *  otherwise:	error
 */
static int __reload_eeprom_set(void)
{
	int r,w;

	w = check_eeprom_set_checksum(eeprom_set);
	if (w < 0) {
		printf("EEPROM set in RAM damaged! (w = %d, all_sets_damaged %d)\n",
			w, all_sets_damaged);
		if (!all_sets_damaged) {
			printf("Read from Flash offset 0x%lx!\n", factory_offset);
			r = ra_flash_read(eeprom_set, CFG_FACTORY_ADDR + factory_offset, LEB_SIZE);
			if (r) {
				printf("Read EEPROM set from Flash 0x%lx fail! (r = %d)\n", factory_offset, r);
				return -2;
			}
			w = check_eeprom_set_checksum(eeprom_set);
			if (w < 0) {
				printf("EEPROM set in RAM still damaged. Select new one!. (w = %d)\n", w);
					choose_active_eeprom_set();
			}
		}
	}
	return 0;
}

/* Update header checksum, data checksum, write times, etc.
 * @return:
 * 	0:	Success
 * 	-1:	Invalid parameter.
 */
static int update_eeprom_checksum(unsigned char *buf)
{
	unsigned long checksum;
	eeprom_set_hdr_t *hdr;

	if (!buf)
		return -1;

	hdr = (eeprom_set_hdr_t *) (buf + EEPROM_SET_HEADER_OFFSET);
	checksum = crc32(0, (const unsigned char *)buf, MAX_EEPROM_SET_LENGTH);

	/* reset write version to 0 if header magic number is incorrect or wrap */
	if (hdr->ih_magic != htonl(FACTORY_IMAGE_MAGIC) ||
	    ntohl(hdr->ih_write_ver) >= 0x7FFFFFFFU)
		hdr->ih_write_ver = htonl(0);

	/* Build new header */
	hdr->ih_magic = htonl(FACTORY_IMAGE_MAGIC);
	hdr->ih_hcrc = 0;
	hdr->ih_hdr_ver = htonl(1);
	hdr->ih_write_ver = htonl(ntohl(hdr->ih_write_ver) + 1);
	hdr->ih_dcrc = htonl(checksum);

	checksum = crc32(0, (const unsigned char *)hdr, sizeof(eeprom_set_hdr_t));
	hdr->ih_hcrc = htonl(checksum);

	debug("header/data checksum: 0x%08x/0x%08x\n", ntohl(hdr->ih_hcrc), ntohl(hdr->ih_dcrc));
	return 0;
}

/* Write EEPROM set in RAM to all factory volume.
 * @return:
 * 	0:	Success.
 *  otherwise:	fail.
 */
static int update_eeprom_sets(void)
{
	int i, r, ret = 0;
	unsigned int o;
	unsigned char buf[LEB_SIZE * 2];

	memcpy(buf, eeprom_set, LEB_SIZE);
	memcpy(buf + LEB_SIZE, eeprom_set, LEB_SIZE);
	for (i = 0, o = 0; i < (NR_EEPROM_SETS / 2); ++i, o += (LEB_SIZE * 2)) {
		r = ra_flash_erase_write(buf, CFG_FACTORY_ADDR + o, LEB_SIZE * 2, 0);
		if (r) {
			printf("Write EEPROM set to 0x%x fail. (r = %d)\n", CFG_FACTORY_ADDR + o, r);
			ret--;
			continue;
		}
	}
	all_sets_damaged = 0;

	return ret;
}
#endif


/**
 * Read date from FACTORY area. Only first MAX_EEPROM_SET_LENGTH can be read.
 * @buf:	buffer address
 * @off:	offset address respect to FACTORY partition
 * @len:	length to read
 * @return:
 * 	0:	success
 *  otherwise:	error
 */
int ra_factory_read(uchar *buf, ulong off, ulong len)
{
	if (!buf || !len || off >= MAX_EEPROM_SET_LENGTH || len > MAX_EEPROM_SET_LENGTH || (off + len) > MAX_EEPROM_SET_LENGTH) {
		debug("%s(): invalid parameter. buf %p off %08lx len %08lx (%08x/%08x).\n",
			__func__, buf, off, len, CFG_FACTORY_ADDR, MAX_EEPROM_SET_LENGTH);
		return -1;
	}

#if defined(UBI_SUPPORT)
	if (__reload_eeprom_set())
		return -1;

	memcpy(buf, eeprom_set + off, len);
	return 0;
#else
	off += CFG_FACTORY_ADDR;
	return ra_flash_read(buf, off, len);
#endif
}


/**
 * Write date to FACTORY area. Only first MAX_EEPROM_SET_LENGTH can be read.
 * For Parallel Flash, whole factory area would be unprotect, read,
 * modified data, write, and protect.
 * @buf:	buffer address
 * @off:	offset address respect to FACTORY partition
 * @len:	length to write
 * @prot:	Unprotect/protect sector. (Parallel NOR Flash only)
 * @return:
 * 	0:	success
 *  otherwise:	error
 */
int ra_factory_erase_write(uchar *buf, ulong off, ulong len, int prot)
{
#if defined(CFG_ENV_IS_IN_FLASH)
	uchar rfbuf[CFG_FACTORY_SIZE];
#endif

	if (!buf || !len || off >= MAX_EEPROM_SET_LENGTH || len > MAX_EEPROM_SET_LENGTH || (off + len) > MAX_EEPROM_SET_LENGTH) {
		debug("%s(): invalid parameter. buf %p off %08lx len %08lx prot %d (%08x/%08x).\n",
			__func__, buf, off, len, prot, CFG_FACTORY_ADDR, MAX_EEPROM_SET_LENGTH);
		return -1;
	}

#if defined(UBI_SUPPORT)
	if (__reload_eeprom_set())
		return -1;

	memcpy(eeprom_set + off, buf, len);
	update_eeprom_checksum(eeprom_set);
	return update_eeprom_sets();
#else
#if defined(CFG_ENV_IS_IN_FLASH)
	memmove(rfbuf, (void*)CFG_FACTORY_ADDR, CFG_FACTORY_SIZE);
	memmove(rfbuf + off, buf, len);
	buf = rfbuf;
	off = 0;
	len = CFG_FACTORY_SIZE;
#endif

	off += CFG_FACTORY_ADDR;
	return ra_flash_erase_write(buf, off, len, prot);
#endif
}

#if defined(CONFIG_ATH_NAND_BR)
/**
 * Duplicate image at load_address as many as possible
 * @ptr:	pointer to image source address
 * @len:	image length in bytes
 * @max_len:	maximum length in bytes can be used at ptr.
 * @return:
 *    >=0:	success, number of new copy
 *     -1:	invalid parameter.
 *  otherwise:	error
 */
static int duplicate_image(unsigned char *ptr, unsigned int len, unsigned int max_len)
{
	int c = 0;
	unsigned char *next_ptr, *max_ptr;
	const struct mtd_info *mtd = &nand_info[nand_curr_device];
	const unsigned int unit_len = ROUNDUP(len, mtd->erasesize);

	if (!ptr || ptr >= (unsigned char*) CFG_FLASH_BASE || !len || len >= max_len ||
	    !max_len || (ptr + len) > (unsigned char*) CFG_FLASH_BASE ||
	    (ptr + max_len) > (unsigned char*) CFG_FLASH_BASE)
	{
		printf("%s: invalid parameter. (ptr %x len %x max_len %x)\n",
			__func__, ptr, len, max_len);
		return -1;
	}

	next_ptr = ptr + unit_len;
	max_ptr = ptr + max_len;
	for (; (next_ptr + unit_len) <= max_ptr; next_ptr+=unit_len, c++) {
		memcpy(next_ptr, ptr, len);
		memset(next_ptr + len, 0, unit_len - len);
	}
	if (next_ptr < max_ptr)
		memcpy(next_ptr, ptr, max_ptr - next_ptr);

	debug("duplicate %d new copy of %x\n", c, ptr);
	return c;
}

/**
 * Duplicate image at addr as many as possible
 * @addr:	start address of image source, flash address can be used.
 * @offset:	offset of flash to write image
 * @len:	image length in bytes
 * @max_len:	maximum length
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	read image from flash fail
 *     -3:	write fail
 *    -12:	-ENOMEM, allocate memory fail.
 *  otherwise:	error
 */
int ranand_dup_erase_write_image(unsigned int addr, unsigned int offset, unsigned int len, unsigned int max_len)
{
	int ret = 0, c = 0, w = 0;
	const struct mtd_info *mtd = &nand_info[nand_curr_device];
	const unsigned int unit_len = ROUNDUP(len, mtd->erasesize);
	unsigned char *src, *ptr;

	if (!mtd || !addr || offset >= mtd->size || offset >= CFG_FLASH_BASE || !len || !max_len || len > max_len) {
		printf("%s: invalid parameter. (mtd %p, addr %x, offset %x, len %x, max_len %x)\n",
			__func__, mtd, addr, offset, len, max_len);
		return -1;
	}

	src = (unsigned char*) addr;
	if (src >= (unsigned char*) CFG_FLASH_BASE) {
		if (!(src = malloc(len))) {
			printf("%s: allocate 0x%x bytes memory fail!\n", __func__, len);
			ret = -12;
			goto exit_dup_write_0;
		}
		ret = ranand_read(src, addr, len);
		if (ret != len) {
			printf("%s: read 0x%x bytes image from flash fail! (ret %d)\n",
				__func__, len, ret);
			ret = -2;
			goto exit_dup_write_0;
		}
	}
	if (!(ptr = malloc(max_len))) {
		printf("%s: allocate 0x%x bytes memory fail! Use %x instead\n",
			__func__, max_len, CFG_LOAD_ADDR_2);
		ptr = (unsigned char*) CFG_LOAD_ADDR_2;
	}

	memcpy(ptr, src, len);
	c = duplicate_image(ptr, len, max_len);
	if (c >= 0)
		c++;
	ranand_set_sbb_max_addr(offset + max_len);
	w = ranand_erase(offset, max_len);
	if (!w || w == -ENOSPC)
		w = ranand_write(ptr, offset, max_len);
	ranand_set_sbb_max_addr(0);

	if (ptr != (unsigned char*) CFG_LOAD_ADDR_2)
		free(ptr);

	if (src != (unsigned char*) addr)
		free(src);

exit_dup_write_0:

	if (w < 0 && w != -ENOSPC)
		return -3;

	printf("write %d copies to flash. (total %d copies)\n", w / unit_len, c);

	return ret;
}

/**
 * Duplicate bootloader image at addr and write to bootloader area as many as possible
 * @addr:	start address of image source, flash address can be used.
 * @len:	image length in bytes
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *  otherwise:	error
 */
int ranand_write_bootloader(unsigned int addr, unsigned int len)
{
	int r, ret, retry = 5, redo = 0;
	unsigned int s, o, l, rlen;
	unsigned char blk_buf[0x20000];
	struct mtd_ecc_stats stats;
	struct mtd_info *mtd = &nand_info[nand_curr_device];

	if (!mtd) {
		printf("%s: mtd = NULL (curr: %d)\n", __func__, nand_curr_device);
		return -2;
	}

	while (retry-- > 0) {
		ret = ranand_dup_erase_write_image(addr, 0, len, CFG_BOOTLOADER_SIZE);
		if (ret) {
			printf("%s: program bootloader fail!!! (ret %d)\n", __func__, ret);
			return ret;
		}

		/* Make sure whole bootloader area is good and no any correctable/uncorrectable exist. */
		for (s = addr, o = 0, l = len, redo = 0;
		     !redo && o < (CFG_BOOTLOADER_SIZE);
		     o += mtd->erasesize)
		{
			if (mtd->block_isbad(mtd, o))
				continue;

			stats = mtd->ecc_stats;
			rlen = (l >= mtd->erasesize)? mtd->erasesize:l;
			r = ranand_read(blk_buf, o, rlen);
			if (r < rlen) {
				redo = 1;
				continue;
			}
			if (mtd->ecc_stats.failed != stats.failed ||
			    mtd->ecc_stats.corrected != stats.corrected)
			{
				printf("correctable/uncorrectable error found!\n");
				redo = 1;
				continue;
			}

			if (l >= mtd->erasesize) {
				l -= mtd->erasesize;
				s += mtd->erasesize;
			} else {
				l = len;
				s = addr;
			}
		}

		if (!redo) {
			printf("verify bootloader: OK\n");
			break;
		}
	}

	return (redo)? -3:0;
}
#endif	/* CONFIG_ATH_NAND_BR */

#ifdef DEBUG_FACTORY_RW
int do_factory_read(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, r, force = 0;
	size_t len;
	unsigned long addr;
	unsigned char buf[256], *p = &buf[0];

	if (argc < 3)
		return EINVAL;

	if (argc == 4)
		force = !!simple_strtoul(argv[3], NULL, 16);

	addr = simple_strtoul(argv[1], NULL, 16);
	len = simple_strtoul(argv[2], NULL, 16);
	if (len >= sizeof(buf)) {
		p = malloc(len + 1);
		if (!p)
			return ENOMEM;
	}

	if (force)
		choose_active_eeprom_set();

	r = ra_factory_read(p, addr, len);
	if (!r) {
		for (i = 0; i < len; ++i) {
			if (!(i % 16))
				printf("\n");
			printf("%02X ", *(p + i));
		}
		printf("\n");
	} else {
		printf("%s: buf %p len %x fail. (r = %d)\n", __func__, p, len, r);
	}

	if (p != &buf[0])
		free(p);

	return 0;
}

U_BOOT_CMD(
	factory_read,	4,	1,	do_factory_read,
	"factory_read	- read factory area\n",
	"factory_offset length [[0]|1]	- read factory area\n"
);

int do_factory_write(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int r;
	size_t len;
	unsigned long ram_addr, flash_offset;

	if (argc < 3)
		return EINVAL;

	ram_addr = simple_strtoul(argv[1], NULL, 16);
	flash_offset = simple_strtoul(argv[2], NULL, 16);
	len = simple_strtoul(argv[3], NULL, 16);
	r = ra_factory_erase_write((uchar*) ram_addr, flash_offset, len, 0);
	if (!r) {
		printf("%s: OK\n", __func__);
	} else {
		printf("%s: ram_addr %lx flash_offset %lx len %x fail. (r = %d)\n",
			__func__, ram_addr, flash_offset, len, r);
	}

	return 0;
}

U_BOOT_CMD(
	factory_write,	4,	1,	do_factory_write,
	"factory_write	- write factory area\n",
	"ram_addr factory_offset length - write factory area\n"
);

#endif	/* DEBUG_FACTORY_RW */

#ifdef DEBUG_FLASH_WRAPPER
int do_flash_erase(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int r;
	size_t len;
	unsigned long addr;

	if (argc < 3)
		return EINVAL;

	addr = simple_strtoul(argv[1], NULL, 16);
	if (addr < CFG_FLASH_BASE) {
		printf("flash_addr must greater than or equal to 0x%x\n", CFG_FLASH_BASE);
		return EINVAL;
	}
	len = simple_strtoul(argv[2], NULL, 16);
	r = ra_flash_erase(addr, len);
	if (!r) {
		printf("%s: OK\n", __func__);
	} else {
		printf("%s: addr %lx len %x fail. (r = %d)\n", __func__, addr, len, r);
	}

	return 0;
}

U_BOOT_CMD(
	flash_erase,	4,	1,	do_flash_erase,
	"flash_erase	- erase flash area\n",
	"flash_addr length - erase flash area\n"
);

int do_flash_read(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, r;
	size_t len;
	unsigned long addr;
	unsigned char buf[256], *p = &buf[0];

	if (argc < 3)
		return EINVAL;

	addr = simple_strtoul(argv[1], NULL, 16);
	if (addr < CFG_FLASH_BASE) {
		printf("flash_addr must greater than or equal to 0x%x\n", CFG_FLASH_BASE);
		return EINVAL;
	}
	len = simple_strtoul(argv[2], NULL, 16);
	if (len >= sizeof(buf)) {
		p = malloc(len + 1);
		if (!p)
			return ENOMEM;
	}

	r = ra_flash_read(p, addr, len);
	if (!r) {
		for (i = 0; i < len; ++i) {
			if (!(i % 16))
				printf("\n");
			printf("%02X ", *(p + i));
		}
		printf("\n");
	} else {
		printf("%s: buf %p len %x fail. (r = %d)\n", __func__, p, len, r);
	}

	if (p != &buf[0])
		free(p);

	return 0;
}

U_BOOT_CMD(
	flash_read,	4,	1,	do_flash_read,
	"flash_read	- read flash area\n",
	"flash_offset length - read flash area\n"
);

int do_flash_erase_write(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int r;
	size_t len;
	unsigned long ram_addr, flash_addr;

	if (argc < 4)
		return EINVAL;

	ram_addr = simple_strtoul(argv[1], NULL, 16);
	flash_addr = simple_strtoul(argv[2], NULL, 16);
	len = simple_strtoul(argv[3], NULL, 16);
	if (flash_addr < CFG_FLASH_BASE) {
		printf("flash_addr must greater than or equal to 0x%x\n", CFG_FLASH_BASE);
		return EINVAL;
	}
	r = ra_flash_erase_write((uchar*)ram_addr, flash_addr, len, 0);
	if (!r) {
		printf("%s: OK\n", __func__);
	} else {
		printf("%s: ram_addr %lx flash_addr %lx len %x fail. (r = %d)\n", __func__, ram_addr, flash_addr, len, r);
	}

	return 0;
}

U_BOOT_CMD(
	flash_erase_write,	4,	1,	do_flash_erase_write,
	"flash_erase_write	- erase and write flash area\n",
	"ram_addr flash_addr length - erase and write flash area\n"
);

#endif	/* DEBUG_FLASH_WRAPPER */
#endif	/* ! UBOOT_STAGE1 */

#if defined(UBI_SUPPORT)
#define LSDK_ART_CALDATA_SIZE	0x100000
/* Return offset of last non-0xFF byte.
 * @return:
 *     -1:	All bytes between *p ~ *(p + len - 1) is 0xFF.
 *    >=0:	Last non 0xFF between *p ~ *(p + len - 1)
 */
static int last_non_ff(unsigned char *p, size_t len)
{
	int i;

	if (!p || !len)
		return 0;

	for (i = len - 1; i >= 0; --i) {
		if (*(p + i) != 0xFF)
			break;
	}

	return i;
}

#ifndef ROUNDUP
#define	ROUNDUP(x, y)		((((x)+((y)-1))/(y))*(y))
#endif

extern u32 crc32_le(u32 crc, unsigned char const *p, size_t len);
/* Change erase counter and recalculate checksum
 * @hdr:	poinster to EC header
 * @ec:		new erase counter
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	invalid UBI EC eader
 */
static int change_ech(struct ubi_ec_hdr *hdr, unsigned long ec)
{
	uint32_t crc;

	if (!hdr)
		return -1;

	/* Check the EC header */
	if (be32_to_cpu(hdr->magic) != UBI_EC_HDR_MAGIC) {
		printf("bad UBI magic %#08x, should be %#08x\n",
		       be32_to_cpu(hdr->magic), UBI_EC_HDR_MAGIC);
		return -2;
	}

	hdr->ec = cpu_to_be64(ec);
	crc = crc32_le(UBI_CRC32_INIT, (unsigned char const *)hdr, UBI_EC_HDR_SIZE_CRC);
	hdr->hdr_crc = cpu_to_be32(crc);

	return 0;
}

/* Erase blocks belongs to UBI_DEV
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	UBI is not detached.
 *  otherwise:	error
 */
static int erase_ubi_block(unsigned int offset, unsigned int end_offset, int mean_ec, const struct ubi_ec_hdr *base_ec_hdr)
{
	const struct mtd_info *mtd = &nand_info[nand_curr_device];
	unsigned int o = offset;
	int erase_len = end_offset - offset;
	int write_ec, w, retry;
	struct ubi_ec_hdr ec_flash, *ec_f = &ec_flash;
	unsigned long ec;
	u32 crc;
	const struct ubi_device *ubi = get_ubi_device();

	if (offset < CFG_UBI_DEV_OFFSET || offset >= mtd->size ||
	    end_offset <= CFG_UBI_DEV_OFFSET || end_offset > mtd->size||
	    offset >= end_offset || (erase_len % mtd->erasesize) || mean_ec < 0 ||
	    !base_ec_hdr || be32_to_cpu(base_ec_hdr->magic) != UBI_EC_HDR_MAGIC)
		return -1;
	if (ubi) {
		printf("%s: Detach UBI device prior to erase!\n", __func__);
		return -2;
	}

	/* skip last 1MB for LSDK ART firmware. */
	if (end_offset > (mtd->size - LSDK_ART_CALDATA_SIZE)) {
		end_offset = mtd->size - LSDK_ART_CALDATA_SIZE;
		erase_len = end_offset - offset;
	}

	for (; erase_len > 0; erase_len -= mtd->erasesize, o += mtd->erasesize) {
		if (ranand_block_isbad(o)) {
			printf("skip bad-block at 0x%x\n", o);
			continue;
		}

		/* preserved erase counter */
		write_ec = 0;
		ec = mean_ec + 1;
		if ((w = ranand_read((unsigned char*)ec_f, o, sizeof(*ec_f))) == sizeof(*ec_f)) {
			if (be32_to_cpu(ec_f->magic) == UBI_EC_HDR_MAGIC &&
			    (crc = crc32_le(UBI_CRC32_INIT, (unsigned char const *)ec_f, UBI_EC_HDR_SIZE_CRC)) == be32_to_cpu(ec_f->hdr_crc))
			{
				ec = be64_to_cpu(ec_f->ec) + 1;
			}
			memcpy(ec_f, base_ec_hdr, sizeof(*ec_f));
			change_ech(ec_f, ec);
			write_ec = 1;
		}

		retry = 0;
retry_erase_ubi:
		if (ranand_erase(o, mtd->erasesize)) {
			printf("erase block at 0x%lx fail, leave it alone and ignore it\n", o);
			continue;
		}

		if (write_ec && (w = ranand_write((unsigned char *)ec_f, o, sizeof(*ec_f))) != sizeof(*ec_f)) {
			printf("write EC header back to empty block fail. (w = %d, retry %d)\n", w, retry);
			if (++retry <= 3)
				goto retry_erase_ubi;
		}
	}

	return 0;
}

/* Read first EEPROM set and judge whether it is worth to backup or not.
 * @factory:	pointer to a buffer that is used to return a full EEPROM set.
 * 		length of the buffer must greather than or equal to LEB_SIZE.
 * @return:
 * 	1:	read success and worth to backup the factory
 * 	0:	read success but don't worth to backup the factory
 *     <0:	error, don't backup the factory
 */
static int backup_factory(unsigned char *factory)
{
	int r;
	unsigned char *mac = factory + RAMAC0_OFFSET;
	const unsigned char default_oui_mac[3] = { 0x00, 0x03, 0x7F };
	const unsigned char default_oui_mac_asus[3] = { 0x00, 0x27, 0x21 };
	const unsigned char zero_mac[6] = { 0x00, 0x00, 0x00 , 0x00, 0x00, 0x00 };
	const unsigned char default_init_mac[6] = { 0x00, 0x11, 0x11, 0x11, 0x11, 0x11 };

	if (!factory)
		return -1;

	if ((r = ra_flash_read(factory, CFG_FACTORY_ADDR, LEB_SIZE)) != 0) {
		printf("Read factory fail! (r = %d)\n", r);
		return -2;
	}

	if ((r = check_eeprom_set_checksum(factory)) < 0) {
		printf("Invalid EEPROM set! (r = %d)\n", r);
		return -3;
	}

	if (*mac == 0xFF) {
		printf("MAC[0] = 0xFF, drop factory\n");
		return 0;
	}

	if (!memcmp(mac, default_oui_mac, 3)) {
		printf("OUI = 00:03:7F, drop factory\n");
		return 0;
	}

	if (!memcmp(mac, default_oui_mac_asus, 3)) {
		printf("OUI = 00:27:21, drop factory\n");
		return 0;
	}

	if (!memcmp(mac, zero_mac, 6)) {
		printf("MAC = 0, drop factory\n");
		return 0;
	}

	if (!memcmp(mac, default_init_mac, 6)) {
		printf("MAC = 00:11:11:11:11:11, drop factory\n");
		return 0;
	}

	return 1;
}

/* Program UBI image with or without OOB information to UBI_DEV.
 * @return:
 * 	0:	success
 * 	1:	image length is not multiple of block_size or block_size + oob_size both
 * 	2:	can't found EC header at first page of a block
 *     -1:	invalid parameter
 *     -2:	allocate buffer for strip oob information from input image fail
 *     -3:	write page fail.
 *     -4:	out of nand flash space to program UBI image
 *     -5:	attach UBI_DEV fail
 *     -6:	restore factory raw data to first EEPROM set fail.
 *     -7:	sync factory fail
 *  otherwise	error
 */
int __SolveUBI(unsigned char *ptr, unsigned int start_offset, unsigned int copysize)
{
	const struct mtd_info *mtd = &nand_info[nand_curr_device];
	const int pages_per_block = mtd->erasesize / mtd->writesize;
	const int block_size_incl_oob = mtd->erasesize + mtd->oobsize * pages_per_block;
	const struct ubi_device *ubi = get_ubi_device();
	size_t len, blk_len, empty_blk_len;
	unsigned int magic_addr;
	char *ubi_detach[] = { "ubi", "detach"};
	int r, block, pos, omit_oob = 0, datalen, retry;
	int in_page_len = mtd->writesize, in_blk_len = mtd->erasesize;
	int ubi_dev_len = mtd->size - CFG_UBI_DEV_OFFSET - LSDK_ART_CALDATA_SIZE;
	unsigned int offset = start_offset;
	unsigned char *p, *q, *data, *blk_buf = NULL;
	int mean_ec = 0;
	struct ubi_ec_hdr ec_flash, *ec_f = &ec_flash;
	unsigned long ec;
	u32 crc;
	int restore_factory = 0;
	unsigned char factory_raw_data[LEB_SIZE];

	if (!ptr || !copysize || copysize < mtd->writesize || start_offset % mtd->erasesize)
		return -1;

	printf("Check UBI image length\n");
	if (!(copysize % (mtd->erasesize)) && !(copysize % block_size_incl_oob)) {
		debug("0x%x is least common multiple of erasesize and block_size_incl_oob\n", copysize);
		if (be32_to_cpu(*(uint32_t*)(ptr + mtd->erasesize)) == UBI_EC_HDR_MAGIC)
			omit_oob=0;
		else if (be32_to_cpu(*(uint32_t*)(ptr + block_size_incl_oob)) == UBI_EC_HDR_MAGIC)
			omit_oob=1;
		else {
			printf("Can't find UBI image of block1!\n");
			return 2;
		}
	} else if (!(copysize % (mtd->erasesize))) {
		omit_oob = 0;
	} else if (!(copysize % block_size_incl_oob)) {
		omit_oob = 1;
	} else {
		printf("%x bytes is not multiple of 0x%x or 0x%x, omit\n",
			copysize, mtd->erasesize, mtd->erasesize + mtd->oobsize * pages_per_block);
		return 1;
	}

	if (omit_oob) {
		in_page_len = mtd->writesize + mtd->oobsize;
		in_blk_len = block_size_incl_oob;
		printf("OOB information found, omit it\n");
	}

	/* check EC header of first page of every block */
	printf("Check all EC header of UBI image\n");
	for (len = copysize, magic_addr = (unsigned int)ptr, block = 0;
		len > 0;
		len -= in_blk_len, magic_addr += in_blk_len, block++)
	{
		if (be32_to_cpu(*(uint32_t*)magic_addr) == UBI_EC_HDR_MAGIC)
			continue;

		printf("can't found EC header at block 0x%x of image (offset 0x%x), stop!\n", block);
		return 2;
	}

	if (omit_oob) {
		blk_buf = malloc(mtd->erasesize);
		if (!blk_buf) {
			printf("can't %d bytes allocate buffer to strip OOB information from input image!\n",
				mtd->erasesize);
			return -2;
		}
	}

	if (ubi) {
		mean_ec = ubi->mean_ec;

		/* Is it possible/worth to backup factory? */
		if (backup_factory(factory_raw_data) == 1) {
			printf("Backup factory\n");
			restore_factory = 1;
		}

		/* detach UBI_DEV */
		do_ubi(NULL, 0, ARRAY_SIZE(ubi_detach), ubi_detach);
	}
	ubi = NULL;

	/* now, we can't call any flash wrapper or functions that would trigger UBI action. */
	/* erase leading block */
	if (offset > CFG_UBI_DEV_OFFSET) {
		printf("erase leading blocks (offset 0x%x~%x length 0x%x, %d blocks)\n",
			CFG_UBI_DEV_OFFSET, offset, offset - CFG_UBI_DEV_OFFSET,
			(offset - CFG_UBI_DEV_OFFSET) / mtd->erasesize);
		erase_ubi_block(CFG_UBI_DEV_OFFSET, offset, mean_ec, (struct ubi_ec_hdr *)ptr);
	}

	/* program input image page by page */
	printf("program UBI image to 0x%x, length 0x%x%s\n", offset, copysize, (omit_oob)? ", omit OOB":"");
	for (len = copysize, data = ptr, block = 0; len > 0 && ubi_dev_len > 0; offset += mtd->erasesize)
	{
		p = data;
		if (ranand_block_isbad(offset)) {
			printf("skip bad-block at 0x%x\n", offset);
			continue;
		}

		/* preserved erase counter */
		ec = mean_ec + 1;
		if (!ranand_read((unsigned char*)ec_f, offset, sizeof(*ec_f))) {
			if (be32_to_cpu(ec_f->magic) == UBI_EC_HDR_MAGIC &&
			    (crc = crc32_le(UBI_CRC32_INIT, (unsigned char const *)ec_f, UBI_EC_HDR_SIZE_CRC)) == be32_to_cpu(ec_f->hdr_crc))
			{
				ec = be64_to_cpu(ec_f->ec) + 1;
			}
		}
		change_ech((struct ubi_ec_hdr *)p, ec);

		if (omit_oob) {
			/* copy the block to bounce buffer page by page to strip OOB information */
			for (blk_len = mtd->erasesize, p = data, q = blk_buf;
				blk_len > 0;
				blk_len -= mtd->writesize, p += in_page_len, q += mtd->writesize)
			{
				memcpy(q, p, mtd->writesize);
			}
			p = blk_buf;
		}

		/* find position of last non-0xFF in this block. don't write tailed empty page to flash */
		pos = last_non_ff(p, mtd->erasesize);
		q = p + ROUNDUP(pos + 1, mtd->writesize);

		retry = 0;
		datalen = q - p;
retry_solve_ubi:
		if (ranand_erase(offset, mtd->erasesize)) {
			printf("erase block at 0x%x fail, leave it alone and ignore it\n", offset);
			continue;
		}
		if ((r = ranand_write(p, offset, datalen)) != datalen) {
			printf("write to 0x%x, length 0x%x fail. (r = %d, retry %d)\n", offset, datalen, r, retry);
			if (++retry <= 3)
				goto retry_solve_ubi;

			return -3;
		}
		empty_blk_len = mtd->erasesize - datalen;

		if (empty_blk_len > 0) {
			printf("skip %d tailed empty page of block %d (0x%x bytes)\n",
				empty_blk_len / mtd->writesize, block, empty_blk_len);
		}

		len -= in_blk_len;
		data += in_blk_len;
		block++;
		ubi_dev_len -= mtd->erasesize;
	}

	if (len > 0 && ubi_dev_len <= 0) {
		printf("Out of space to program image! (len 0x%x)", len);
		return -4;
	}

	/* erase remain blocks */
	printf("erase remain blocks (offset 0x%x length 0x%x, %d blocks)\n",
		offset, ubi_dev_len, ubi_dev_len / mtd->erasesize);
	erase_ubi_block(offset, mtd->size - LSDK_ART_CALDATA_SIZE, mean_ec, (struct ubi_ec_hdr *)ptr);

	/* don't need to restore factory, return success */
	if (!restore_factory) {
		printf("Success.\n");
		return 0;
	}

	printf("Restore factory\n");
	if ((r = ra_flash_init_layout()) != 0) {
		printf("Attach UBI_DEV fail! (r = %d)\n", r);
		return -5;
	}

	if ((r = ra_flash_erase_write(factory_raw_data, CFG_FACTORY_ADDR, LEB_SIZE, 0)) != 0) {
		printf("Restore factory to first EEPROM set fail! (r = %d)\n", r);
		return -6;
	}

	/* Reload factory to RAM */
	printf("Reload factory\n");
	choose_active_eeprom_set();

	printf("Success.\n");
	return 0;
}
#endif
