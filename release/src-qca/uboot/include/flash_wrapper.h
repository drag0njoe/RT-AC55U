// vim:cin
/* 
 * Copyright 2013, ASUSTeK Inc.
 * All Rights Reserved.
 */

#ifndef _FLASH_WRAPPER_H_
#define _FLASH_WRAPPER_H_

#if defined(CONFIG_ATH_NAND_BR)
extern int ranand_set_sbb_max_addr(loff_t addr);
#else
static inline int ranand_set_sbb_max_addr(loff_t addr) { return 0; }
#endif

extern int get_ubi_volume_idseq_by_addr(const char *name);

extern int ra_flash_init_layout(void);
extern char *ra_flash_id(void);

#if defined(UBI_SUPPORT)
extern int choose_active_eeprom_set(void);
extern int __SolveUBI(unsigned char *ptr, unsigned int offset, unsigned int copysize);
#else
static inline int choose_active_eeprom_set(void) { return 0; }
#endif

#if defined(CONFIG_ATH_NAND_BR)
extern int ranand_dup_erase_write_image(unsigned int addr, unsigned int offset, unsigned int len, unsigned int max_len);
extern int ranand_write_bootloader(unsigned int addr, unsigned int len);
extern int ranand_check_and_fix_bootloader(void);
#endif

/* Below function use absolute address, include CFG_FLASH_BASE. */
extern int ra_flash_read(uchar * buf, ulong addr, ulong len);
extern int ra_flash_erase_write(uchar * buf, ulong addr, ulong len, int prot);
extern int ra_flash_erase(ulong addr, ulong len);

/* Below function use relative address, respect to start address of factory area. */
extern int ra_factory_read(uchar *buf, ulong off, ulong len);
extern int ra_factory_erase_write(uchar *buf, ulong off, ulong len, int prot);
#endif	/* _FLASH_WRAPPER_H_ */
