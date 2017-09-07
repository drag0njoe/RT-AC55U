/**********************************************************
*	File:replace.c
*	This file includes a function that is used to replace the 
*	values in RF buffer.
*
*	return rc
*	=0:	replace successful
*	!=0:	fail
*
**********************************************************/

#include <common.h>
#include <command.h>
#include <asm/errno.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <asm/byteorder.h>

#if defined(LSDK_NART_SUPPORT)
#include <linux/mtd/mtd.h>
#include <nand.h>
#endif

#include "replace.h"

int replace(unsigned long addr, uchar *value, int len)
{
	if (addr >= CFG_FACTORY_SIZE || !value || len <= 0 || (addr + len) >= CFG_FACTORY_SIZE)
		return -1;

	return ra_factory_erase_write(value, addr, len, 0);
}


#ifdef DEBUG_FACTORY_RW
int do_replace(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, r, parse_method = 1;		/* 0: hex; otherwise: string (default) */
	size_t len, len2;
	unsigned long addr;
	unsigned char buf[256], *p = &buf[0], h[3], *d = NULL;

	if (argc < 3)
		return EINVAL;

	addr = simple_strtoul(argv[1], NULL, 16);
	switch (argc) {
		case 4:
			d = argv[3];
			if (!strnicmp(argv[2], "hex", 3))
				parse_method = 0;
			break;
		case 3:
			d = argv[2];
			break;
	}
	len = strlen(d);
	if (!parse_method && (len & 1)) {
		/* in hexadecimal mode, only even digits is accepts. */
		printf("Length %d is not even\n", len);
		return EINVAL;
	}

	if (len >= sizeof(buf)) {
		p = malloc(len + 1);
		if (!p)
			return ENOMEM;
	}

	if (parse_method)	{
		/* data format is string */
		strcpy(p, d);
	} else {
		/* data format is hexadecimal digit */
		len2 = len;
		i = 0;
		while (len2 > 0) {
			if (!isxdigit(*d) || !isxdigit(*(d+1))) {
				printf("Invalid hexadecimal digit found.\n");
				goto out_replace;
			}

			h[0] = *d++;
			h[1] = *d++;
			h[2] = '\0';
			len2 -= 2;
			p[i++] = (unsigned char) simple_strtoul(&h[0], NULL, 16);
		}
		len = i;
	}

	r = ra_factory_erase_write(p, addr, len, 0);
	if (r)
		printf("%s: buf %p len %x fail. (r = %d)\n", __func__, p, len, r);

out_replace:
	if (p != &buf[0])
		free(p);

	return 0;
}

U_BOOT_CMD(
	replace,	4,	1,	do_replace,
	"replace	- modify factory area\n",
	"factory_offset [hex|[string]] data	- modify factory area\n"
);
#endif	/* DEBUG_FACTORY_RW */

/* Check bootloader version number
 * @return:
 * 	1:	match.
 * 	0:	different.
 */
int chkVer(void)
{
	uchar rfbuf[4];

	memset(rfbuf, 0x0, sizeof(rfbuf));
	ra_factory_read(rfbuf, 0xd18a, 4);
        printf("\n%s bootloader%s version: %c.%c.%c.%c\n", model, bl_stage, blver[0], blver[1], blver[2], blver[3]);

	if (!memcmp(rfbuf, blver, sizeof(rfbuf)))
		return 1;

	return 0;
}

/* Check whether MAC address is valid or not.
 * @return:
 * 	0:	valid
 *     -1:	multicast MAC address
 *     -2:	empty MAC address
 */
int chkMAC(void)
{
	uchar p[6];
	uchar empty_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	ra_factory_read(p, RAMAC0_OFFSET, 6);

	printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", p[0], p[1], p[2], p[3], p[4], p[5]);
	if (!memcmp(p, empty_mac, sizeof(p))) {
		printf("\ninvalid mac ff:ff:ff:ff:ff:ff\n");
		return -2;
	}

	if (p[0] & 0x01) {
		printf("\nerr mac with head 01\n");
		return -1;
	}

	return 0;
}

#if defined(LSDK_NART_SUPPORT)
#define QC95xx_EEPROM_OFFSET		0x1000
#define QC98XX_EEPROM_OFFSET		0x5000
#define QC98XX_EEPROM_SIZE_LARGEST	2116
/**
 * Verify EEPROM checksum of QCA98xx
 * @return:
 * 	0:	checksum correct
 *  otherwise:	invalid checksum
 */
static int qc98xx_verify_checksum(void *eeprom)
{
	uint16_t *p_half, sum = 0;
	int i;

	p_half = (uint16_t *)eeprom;
	for (i = 0; i < QC98XX_EEPROM_SIZE_LARGEST / 2; i++) {
		sum ^= __le16_to_cpu(*p_half++);
	}
	if (sum != 0xffff) {
		printf("%s: Invalid checksum! (flash checksum %04x, computed %04x, sum %04x)\n",
			__func__, __le16_to_cpu(*((uint16_t *)eeprom + 1)), sum ^ 0xffff, sum);
		return -1;
	}
	return 0;
}

/**
 * Looking for valid RF caldata and return offset of it.
 * @buf:	pointer to 64KB buffer.
 * @return:
 *     <=0:	RF caldata invalid, error occurs, or not found.
 *  otherwise:	offset to valid RF caldata in NAND Flash.
 */
loff_t find_rfcaldata(void)
{
	int r;
	size_t retlen;
	loff_t from, result = 0;
	unsigned char buf[64 * 1024] __attribute__ ((aligned(4)));
	struct mtd_info *mtd = &nand_info[nand_curr_device];

	if (!mtd)
		return -ENODEV;

	for (from = mtd->size - RFCALDATA_REGION_SIZE; !result && from < mtd->size; from += mtd->erasesize) {
		if ((r = mtd->read(mtd, from, sizeof(buf), &retlen, buf)) < 0 || retlen != sizeof(buf)) {
			printf("%s: read %x fail. (r = %d,%d)\n", __func__, from, r, retlen);
			continue;
		}

		printf("check magic @ %08lx\n", (ulong)from);
		if (memcmp(buf + RFCAL_FLAG_OFFSET, RFCAL_READY, 4))
			continue;

		if (qc98xx_verify_checksum(buf + QC98XX_EEPROM_OFFSET))
			continue;

		printf("Found RF caldata at offset %08lx\n", (ulong)from);
		result = from;
	}

	return result;
}

/**
 * If RF caldata exist and it haven't been copied to factory, copy to factory.
 * @offset:	offset of RF caldata in NAND Flash.
 * @return:
 * 	0:	RF caldata have been copied successful, skip.
 * 	-1:	read factory fail.
 * 	-2:	invlid RF caldata
 * 	-3:	read RF caldata fail.
 * 	-4:	invalid flag
 * 	-5:	invalid 5G EEPROM checksum of RF caldata
 *     <-6:	copy RF caldata fail
 */
int copy_rfcaldata(loff_t offset)
{
	int r, ret;
	size_t retlen;
	unsigned char *q;
	unsigned char buf[64 * 1024] __attribute__ ((aligned(4)));
	struct mtd_info *mtd = &nand_info[nand_curr_device];
	const struct {
		unsigned int offset;
		unsigned int length;
	} frag_list[] = {
		{ 0, 0x1000 },				/* first 0x1000 bytes */
		{ QC95xx_EEPROM_OFFSET, 0x4000 },	/* 1st RF catldata */
		{ QC98XX_EEPROM_OFFSET, 0x4000 },	/* 2nd RF catldata */

		{ RFCAL_FLAG_OFFSET, 4 },		/* update copy flag */
		{ 0, 0 },
	}, *p;

	if ((r = ra_factory_read(buf, RFCAL_FLAG_OFFSET, 4)) != 0)
		return -1;
	if (!memcmp(buf, RFCAL_COPIED, 4))
		return 0;
	if (!mtd)
		return -ENODEV;
	/* Check offset */
	if (offset < (mtd->size - RFCALDATA_REGION_SIZE) || offset > (mtd->size - mtd->erasesize)) {
		printf("%s: Invalid offset %08lx\n", __func__, (ulong)offset);
		return -2;
	}

	/* Read RF caldata, check checksum of 5G EEPROM, and check magic number */
	if ((r = mtd->read(mtd, offset, sizeof(buf), &retlen, buf)) < 0 || retlen != sizeof(buf)) {
		printf("%s: read %08lx fail. (r = %d,%d)\n", __func__, (ulong)offset, r, retlen);
		return -3;
	}

	q = buf + RFCAL_FLAG_OFFSET;
	if (memcmp(q, RFCAL_READY, 4)) {
		printf("%s: Invalid magic %02X%02X%02X%02X @ %08lx!\n",
			__func__, *q, *(q+1), *(q+2), *(q+3), (ulong)offset);
		return -4;
	}

	if (qc98xx_verify_checksum(buf + QC98XX_EEPROM_OFFSET)) {
		printf("%s: invalid checksum @ %08lx\n", __func__, (ulong)offset);
		return -5;
	}

	/* Update copy flag ("LSDK" --> "RFCA") */
	memcpy(q, RFCAL_COPIED, 4);
	/* Copy RF caldata to factory fragment by fragment. */
	for (p = &frag_list[0], ret = -6; p->length; ++p, --ret) {
		if (!(r = ra_factory_erase_write(buf + p->offset, p->offset, p->length, 0)))
			continue;

		printf("%s: Copy offset %x length %x fail! (r = %d)\n",
			__func__, p->offset, p->length, r);
		return ret;
	}

	return 0;
}
#endif	/* LSDK_NART_SUPPORT */
