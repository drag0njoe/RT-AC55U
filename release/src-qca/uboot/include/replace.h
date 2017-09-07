#ifndef REPLACE_H
#define REPLACE_H

#include <common.h>

extern int replace(unsigned long addr, uchar *value, int len);
extern int chkMAC(void);
extern int chkVer(void);

#if defined(LSDK_NART_SUPPORT)
extern loff_t find_rfcaldata(void);
extern int copy_rfcaldata(loff_t offset);
#else
static inline loff_t find_rfcaldata(void) { return 0; }
static inline int copy_rfcaldata(loff_t offset) { return 0; }
#endif

#endif	/* !REPLACE_H */
