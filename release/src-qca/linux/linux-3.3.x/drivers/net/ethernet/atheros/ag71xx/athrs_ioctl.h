/* 
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
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

#ifndef _ATHRS_IOCTL_H
#define _ATHRS_IOCTL_H
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <asm/system.h>
#include <linux/netdevice.h>
#include <net/inet_ecn.h> /* XXX for TOS */
#include <linux/if_ether.h>
//#include "athrs_config.h"
#include "athrs_phy_ctrl.h"

/* Ioctl subroutines */
#define ATHR_GMAC_QOS_CTRL_IOC      	   (SIOCDEVPRIVATE | 0x01) 		                /* QOS  CTRL COMMANDS */ 
#define ATHR_GMAC_CTRL_IOC          	   (SIOCDEVPRIVATE | 0x02)                              /* GMAC CTRL COMMANDS */ 
#define ATHR_PHY_CTRL_IOC           	   (SIOCDEVPRIVATE | 0x03)                              /* PHY  CTRL COMMANDS */ 
#define ATHR_VLAN_IGMP_IOC          	   (SIOCDEVPRIVATE | 0x04)                              /* VLAN IGMP COMMANDS */  
#define ATHR_HW_ACL_IOC		    	   (SIOCDEVPRIVATE | 0x05)                              /* ACL COMMANDS */ 

/* 
 *GMAC_CTRL_IOC_COMMANDS
 */
#define ATHR_GMAC_TX_FLOW_CTRL            ((ATHR_GMAC_CTRL_IOC << 16) | 0x1)                   /*  Enabling & disabling the tx flow ctrl*/   
#define ATHR_GMAC_RX_FLOW_CTRL            ((ATHR_GMAC_CTRL_IOC << 16) | 0x2)                   /*  Enabling & disabling the rx flow ctrl*/  
#define ATHR_GMAC_DMA_CHECK               ((ATHR_GMAC_CTRL_IOC << 16) | 0x3)                   /*  To enable & disable  the dma */
#define ATHR_GMAC_SOFT_LED_BLINK          ((ATHR_GMAC_CTRL_IOC << 16) | 0x4)
#define ATHR_GMAC_SW_ONLY_MODE            ((ATHR_GMAC_CTRL_IOC << 16) | 0x5)                   /* To enable & disable the switch only mode*/
#define ATHR_GMAC_STATS			  ((ATHR_GMAC_CTRL_IOC << 16) | 0x6)		       /* To print the mac counter statistics*/
#define ATHR_JUMBO_FRAME                  ((ATHR_GMAC_CTRL_IOC << 16) | 0x7)                   /* Jumbo packet frame Enable*/
#define ATHR_FRAME_SIZE_CTL               ((ATHR_GMAC_CTRL_IOC << 16) | 0x8)                   /* Jumbo packet frame size*/

#define ATHR_GMAC_FLOW_CTRL               ((ATHR_GMAC_CTRL_IOC << 16) | 0x9)

#ifdef ETHDEBUG_ENABLED
#define ATHR_DBG_CONFIG                   ((ATHR_GMAC_CTRL_IOC << 16) | 0xa)
#define ATHR_DBG_RESTART                  ((ATHR_GMAC_CTRL_IOC << 16) | 0xb)
#define ATHR_DBG_STATS                    ((ATHR_GMAC_CTRL_IOC << 16) | 0xc)
#define ATHR_DBG_ENABLE                   ((ATHR_GMAC_CTRL_IOC << 16) | 0xd)
#endif


/*
 *PHY_CTRL_COMMANDS
 */
#define ATHR_PHY_FORCE                     ((ATHR_PHY_CTRL_IOC << 16) | 0x1)                  /* To force the phy */
#define ATHR_PHY_RD                        ((ATHR_PHY_CTRL_IOC << 16) | 0x2)                  /* To Read from phy */   
#define ATHR_PHY_WR                        ((ATHR_PHY_CTRL_IOC << 16) | 0x3)                  /* to write to phy  */
#define ATHR_PHY_MIB                       ((ATHR_PHY_CTRL_IOC << 16) | 0x4)
#define ATHR_PHY_STATS                     ((ATHR_PHY_CTRL_IOC << 16) | 0x5)
#define ATHR_PORT_STATS                    ((ATHR_PHY_CTRL_IOC << 16) | 0x6)
#define ATHR_PORT_LINK                     ((ATHR_PHY_CTRL_IOC << 16) | 0x7)
#define ATHR_FLOW_LINK_EN                  ((ATHR_PHY_CTRL_IOC << 16) | 0x8)
#define ATHR_PHY_RXFCTL                    ((ATHR_PHY_CTRL_IOC << 16) | 0x9)
#define ATHR_PHY_TXFCTL                    ((ATHR_PHY_CTRL_IOC << 16) | 0x10)
#define ATHR_PHY_FLOW_CTRL                 ((ATHR_PHY_CTRL_IOC << 16) | 0x11)

#ifdef CONFIG_ATHR_VLAN_IGMP                                                                  

#define ATHR_PACKET_FLAG                   ((ATHR_VLAN_IGMP_IOC << 16) | 0x1)
#define ATHR_VLAN_ADDPORTS                 ((ATHR_VLAN_IGMP_IOC << 16) | 0x2)
#define ATHR_VLAN_DELPORTS                 ((ATHR_VLAN_IGMP_IOC << 16) | 0x3)
#define ATHR_VLAN_SETTAGMODE               ((ATHR_VLAN_IGMP_IOC << 16) | 0x4)
#define ATHR_VLAN_SETDEFAULTID             ((ATHR_VLAN_IGMP_IOC << 16) | 0x5)
#define ATHR_VLAN_ENABLE                   ((ATHR_VLAN_IGMP_IOC << 16) | 0x6)
#define ATHR_VLAN_DISABLE                  ((ATHR_VLAN_IGMP_IOC << 16) | 0x7)
#define ATHR_IGMP_ON_OFF                   ((ATHR_VLAN_IGMP_IOC << 16) | 0x8)
#define ATHR_LINK_GETSTAT                  ((ATHR_VLAN_IGMP_IOC << 16) | 0x9)
#define ATHR_ARL_ADD                       ((ATHR_VLAN_IGMP_IOC << 16) | 0xa)
#define ATHR_ARL_DEL                       ((ATHR_VLAN_IGMP_IOC << 16) | 0xb)
#define ATHR_MCAST_CLR                     ((ATHR_VLAN_IGMP_IOC << 16) | 0xc)
#define VLAN_DEV_INFO(x) 	     ((struct eth_vlan_dev_info *)x->priv)


struct eth_vlan_dev_info {
    unsigned long inmap[8];
    char *outmap[16];
    unsigned short vlan_id;
};
#endif

#define ATHR_S26_RD_PHY                    ((ATHR_VLAN_IGMP_IOC << 16) | 0xd)
#define ATHR_S26_WR_PHY                    ((ATHR_VLAN_IGMP_IOC << 16) | 0xe)
#define ATHR_S26_FORCE_PHY                 ((ATHR_VLAN_IGMP_IOC << 16) | 0xf)

struct eth_cfg_params {
    int cmd;
    char ad_name[IFNAMSIZ]; /* if name, e.g. "eth0" */
    uint16_t vlanid;
    uint16_t portnum;   /* pack to fit, yech */
    uint32_t phy_reg;
    uint32_t tos;
    uint32_t val;
    uint8_t duplex;
    uint8_t mac_addr[6];
    struct rx_stats rxcntr;
    struct tx_stats txcntr;
    struct tx_mac_stats txmac;
    struct rx_mac_stats rxmac;
    struct phystats phy_st;
};
#ifdef CONFIG_ATHRS_HW_ACL
#define ATHR_ACL_COMMIT 	 	((ATHR_HW_ACL_IOC << 16) | 0x1)
#define ATHR_ACL_FLUSH  		((ATHR_HW_ACL_IOC << 16) | 0x2)
#endif


#ifdef CONFIG_ATHRS_QOS

/*
 * GMC_QOS_CTRL_COMMANDS
 */
#define ATHR_QOS_ETH_SOFT_CLASS   	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x1)                
#define ATHR_QOS_ETH_PORT         	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x2)
#define ATHR_QOS_ETH_VLAN         	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x3)
#define ATHR_QOS_ETH_DA           	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x4)
#define ATHR_QOS_ETH_IP           	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x5)		    	
#define ATHR_QOS_PORT_ILIMIT      	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x6)  	       /* ingress reate limit */         
#define ATHR_QOS_PORT_ELIMIT      	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x7)                 /* egress reate limit  */  
#define ATHR_QOS_PORT_EQLIMIT     	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x8)
#define MAX_QOS_COMMAND           	((ATHR_GMAC_QOS_CTRL_IOC << 16) | 0x9)
#endif

#endif                          //_ATHRS_IOCTL_H
