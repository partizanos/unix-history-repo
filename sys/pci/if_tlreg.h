/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_tlreg.h,v 1.1 1998/05/21 03:19:56 wpaul Exp $
 */


struct tl_type {
	u_int16_t		tl_vid;
	u_int16_t		tl_did;
	char			*tl_name;
};

/*
 * ThunderLAN TX/RX list format. The TX and RX lists are pretty much
 * identical: the list begins with a 32-bit forward pointer which points
 * at the next list in the chain, followed by 16 bits for the total
 * frame size, and a 16 bit status field. This is followed by a series
 * of 10 32-bit data count/data address pairs that point to the fragments
 * that make up the complete frame.
 */

#define TL_MAXFRAGS		10
#define TL_RX_LIST_CNT		10
#define TL_TX_LIST_CNT		10
#define TL_MIN_FRAMELEN		64

struct tl_frag {
	u_int32_t		tlist_dcnt;
	u_int32_t		tlist_dadr;
};

struct tl_list {
	u_int32_t		tlist_fptr;	/* phys address of next list */
	u_int16_t		tlist_cstat;	/* status word */
	u_int16_t		tlist_frsize;	/* size of data in frame */
	struct tl_frag		tl_frag[TL_MAXFRAGS];
};

/*
 * This is a special case of an RX list. By setting the One_Frag
 * bit in the NETCONFIG register, the driver can force the ThunderLAN
 * chip to use only one fragment when DMAing RX frames.
 */

struct tl_list_onefrag {
	u_int32_t		tlist_fptr;
	u_int16_t		tlist_cstat;
	u_int16_t		tlist_frsize;
	struct tl_frag		tl_frag;
};

struct tl_list_data {
	struct tl_list_onefrag	tl_rx_list[TL_RX_LIST_CNT];
	struct tl_list		tl_tx_list[TL_TX_LIST_CNT];
	unsigned char		tl_pad[TL_MIN_FRAMELEN];
};

struct tl_chain {
	struct tl_list		*tl_ptr;
	struct mbuf		*tl_mbuf;
	struct tl_chain		*tl_next;
};

struct tl_chain_data {
	struct tl_chain		tl_rx_chain[TL_RX_LIST_CNT];
	struct tl_chain		tl_tx_chain[TL_TX_LIST_CNT];

	struct tl_chain		*tl_rx_head;
	struct tl_chain		*tl_rx_tail;

	struct tl_chain		*tl_tx_head;
	struct tl_chain		*tl_tx_tail;
	struct tl_chain		*tl_tx_free;
};

struct tl_iflist;

struct tl_softc {
	struct arpcom		arpcom;		/* interface info */
	struct ifmedia		ifmedia;	/* media info */
	struct tl_csr		*csr;		/* pointer to register map */
	struct tl_type		*tl_dinfo;	/* ThunderLAN adapter info */
	struct tl_type		*tl_pinfo;	/* PHY info struct */
	u_int8_t		tl_ctlr;	/* chip number */
	u_int8_t		tl_unit;	/* interface number */
	u_int8_t		tl_phy_addr;	/* PHY address */
	u_int8_t		tl_autoneg;	/* autoneg in progress */
	u_int16_t		tl_phy_sts;	/* PHY status */
	u_int16_t		tl_phy_vid;	/* PHY vendor ID */
	u_int16_t		tl_phy_did;	/* PHY device ID */
	struct tl_iflist	*tl_iflist;	/* Pointer to controller list */
	caddr_t			tl_ldata_ptr;
	struct tl_list_data	*tl_ldata;	/* TX/RX lists and mbufs */
	struct tl_chain_data	tl_cdata;
	int			tl_txeoc;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct callout_handle	tl_stat_ch;
#endif

};

#define TX_THR		0x00000007

#define TL_FLAG_FORCEDELAY	1
#define TL_FLAG_SCHEDDELAY	2
#define TL_FLAG_DELAYTIMEO	3

/*
 * The ThunderLAN supports up to 32 PHYs.
 */
#define TL_PHYADDR_MIN		0x00
#define TL_PHYADDR_MAX		0x1F

#define PHY_UNKNOWN	6

struct tl_iflist {
	struct tl_csr		*csr;			/* Register map */
	struct tl_type		*tl_dinfo;
	int			tl_active_phy;		/* # of active PHY */
	int			tlc_unit;		/* TLAN chip # */
	struct tl_softc		*tl_sc[TL_PHYADDR_MAX];	/* pointers to PHYs */
	pcici_t			tl_config_id;
	struct tl_iflist	*tl_next;
};

#define TL_PHYS_IDLE	-1

/*
 * General constants that are fun to know.
 *
 * The ThunderLAN controller is made by Texas Instruments. The
 * manual indicates that if the EEPROM checksum fails, the PCI
 * vendor and device ID registers will be loaded with TI-specific
 * values.
 */
#define	TI_VENDORID		0x104C
#define	TI_DEVICEID_THUNDERLAN	0x0500

/*
 * Known PHY Ids. According to the Level 1 documentation (which is
 * very nice, incidentally), here's how they work:
 *
 * The PHY identifier register #1 is composed of bits 3 through 18
 * of the OUI. (First 16-bit word.)
 * The PHY identifier register #2 is composed of bits 19 through 24
 * if the OUI.
 * This is followed by 6 bits containing the manufacturer's model
 * number.
 * Lastly, there are 4 bits for the manufacturer's revision number.
 *
 * Honestly, there are a lot of these that don't make any sense; the
 * only way to be really sure is to look at the data sheets.
 */

/*
 * Texas Instruments PHY identifiers
 *
 * The ThunderLAN manual has a curious and confusing error in it.
 * In chapter 7, which describes PHYs, it says that TI PHYs have
 * the following ID codes, where xx denotes a revision:
 *
 * 0x4000501xx			internal 10baseT PHY
 * 0x4000502xx			TNETE211 100VG-AnyLan PMI
 *
 * The problem here is that these are not valid 32-bit hex numbers:
 * there's one digit too many. My guess is that they mean the internal
 * 10baseT PHY is 0x4000501x and the TNETE211 is 0x4000502x since these
 * are the only numbers that make sense.
 */
#define TI_PHY_VENDORID		0x4000
#define TI_PHY_10BT		0x501F
#define TI_PHY_100VGPMI		0x502F

/*
 * These ID values are for the NS DP83840A 10/100 PHY
 */
#define NS_PHY_VENDORID		0x2000
#define NS_PHY_83840A		0x5C0F

/*
 * Level 1 10/100 PHY
 */
#define LEVEL1_PHY_VENDORID	0x7810
#define LEVEL1_PHY_LXT970	0x000F

/*
 * Intel 82555 10/100 PHY
 */
#define INTEL_PHY_VENDORID	0x0A28
#define INTEL_PHY_82555		0x015F

/*
 * SEEQ 80220 10/100 PHY
 */
#define SEEQ_PHY_VENDORID	0x0016
#define SEEQ_PHY_80220		0xF83F

/*
 * These are the PCI vendor and device IDs for Compaq ethernet
 * adapters based on the ThunderLAN controller.
 */
#define COMPAQ_VENDORID				0x0E11
#define COMPAQ_DEVICEID_NETEL_10_100		0xAE32
#define COMPAQ_DEVICEID_NETEL_10		0xAE34
#define COMPAQ_DEVICEID_NETFLEX_3P_INTEGRATED	0xAE35
#define COMPAQ_DEVICEID_NETEL_10_100_DUAL	0xAE40
#define COMPAQ_DEVICEID_NETEL_10_100_PROLIANT	0xAE43
#define COMPAQ_DEVICEID_DESKPRO_4000_5233MMX	0xB011
#define COMPAQ_DEVICEID_NETFLEX_3P		0xF130
#define COMPAQ_DEVICEID_NETFLEX_3P_BNC		0xF150

/*
 * PCI low memory base and low I/O base
 */
#define TL_PCI_LOIO		0x10
#define TL_PCI_LOMEM		0x14

/*
 * ThunderLAN host register layout
 */
struct tl_regbytes {
	volatile u_int8_t	byte0;
	volatile u_int8_t	byte1;
	volatile u_int8_t	byte2;
	volatile u_int8_t	byte3;
};

struct tl_regwords {
	volatile u_int16_t	word0;
	volatile u_int16_t	word1;
};

struct tl_csr {
	volatile	u_int32_t	tl_host_cmd;
	volatile	u_int32_t	tl_ch_parm;
	volatile	u_int16_t	tl_dio_addr;
	volatile	u_int16_t	tl_host_int;
	union {
		volatile	u_int32_t		tl_dio_data;
		volatile	struct tl_regwords	tl_dio_words;
		volatile	struct tl_regbytes	tl_dio_bytes;
	} u;
};

/*
 * The DIO access macros allow us to read and write the ThunderLAN's
 * internal registers. The ThunderLAN manual gives examples using PIO.
 * This driver uses memory mapped I/O, which allows us to totally avoid
 * the use of inb/outb & friends. Memory mapped registers are keen.
 *
 * Note that the set/clr macros go to the trouble of reading the registers
 * back after they've been written. During initial development of this
 * driver, I discovered that the EEPROM access routines wouldn't work
 * properly unless I did this. I'm not sure why, though I suspect it
 * may have something to do with defeating the cache on the processor.
 */


/* Select a register */
#define DIO_SEL(x)	csr->tl_dio_addr = (u_int16_t)x

/*
 * Set/clear/get a bit in the selected byte register
 */
#define DIO_BYTE0_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte0 |=	\
							(u_int8_t)x;	\
					f = csr->u.tl_dio_bytes.byte0;	\
				}
#define DIO_BYTE0_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte0 &=	\
							(u_int8_t)~x;	\
					f = csr->u.tl_dio_bytes.byte0;	\
				}
#define DIO_BYTE0_GET(x)	csr->u.tl_dio_bytes.byte0 & (u_int8_t)x

#define DIO_BYTE1_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte1 |=	\
							(u_int8_t)x;	\
					f = csr->u.tl_dio_bytes.byte1;	\
				}
#define DIO_BYTE1_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte1 &=	\
							(u_int8_t)~x;	\
					f = csr->u.tl_dio_bytes.byte1;	\
				}
#define DIO_BYTE1_GET(x)	csr->u.tl_dio_bytes.byte1 & (u_int8_t)x

#define DIO_BYTE2_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte2 |=	\
							(u_int8_t)x;	\
					f = csr->u.tl_dio_bytes.byte2;	\
				}
#define DIO_BYTE2_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte2 &=	\
							(u_int8_t)~x;	\
					f = csr->u.tl_dio_bytes.byte2;	\
				}
#define DIO_BYTE2_GET(x)	csr->u.tl_dio_bytes.byte2 & (u_int8_t)x

#define DIO_BYTE3_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte3 |=	\
							(u_int8_t)x;	\
					f = csr->u.tl_dio_bytes.byte3;	\
				}
#define DIO_BYTE3_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_bytes.byte3 &=	\
							(u_int8_t)~x;	\
					f = csr->u.tl_dio_bytes.byte3;	\
				}
#define DIO_BYTE3_GET(x)	csr->u.tl_dio_bytes.byte3 & (u_int8_t)x
/*
 * Read/write 16-bit word
 */
#define DIO_WORD0_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_words.word0 |=	\
							(u_int16_t)x;	\
					f = csr->u.tl_dio_words.word0;	\
				}
#define DIO_WORD0_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_words.word0 &=	\
							~(u_int16_t)x;	\
					f = csr->u.tl_dio_words.word0;	\
				}
#define DIO_WORD0_GET(x)	(csr->u.tl_dio_words.word0 & x)

#define DIO_WORD1_SET(x)	{					\
					int			f;	\
					csr->u.tl_dio_words.word1 |=	\
							(u_int16_t)x;	\
					f = csr->u.tl_dio_words.word1;	\
				}
#define DIO_WORD1_CLR(x)	{					\
					int			f;	\
					csr->u.tl_dio_words.word1 &=	\
							~(u_int16_t)x;	\
					f = csr->u.tl_dio_words.word1;	\
				}
#define DIO_WORD1_GET(x)	(csr->u.tl_dio_words.word1 & x)

/*
 * Read/write 32-bit word
 */
#define DIO_LONG_GET(x)	x = csr->u.tl_dio_data
#define DIO_LONG_PUT(x)	csr->u.tl_dio_data = (u_int32_t)x


#define	TL_DIO_ADDR_INC		0x8000	/* Increment addr on each read */
#define TL_DIO_RAM_SEL		0x4000	/* RAM address select */
#define	TL_DIO_ADDR_MASK	0x3FFF	/* address bits mask */

/*
 * Interrupt types
 */
#define TL_INTR_INVALID		0x0
#define TL_INTR_TXEOF		0x1
#define TL_INTR_STATOFLOW	0x2
#define TL_INTR_RXEOF		0x3
#define TL_INTR_DUMMY		0x4
#define TL_INTR_TXEOC		0x5
#define TL_INTR_ADCHK		0x6
#define TL_INTR_RXEOC		0x7

#define TL_INT_MASK		0x001C
#define TL_VEC_MASK		0x1FE0
/*
 * Host command register bits
 */
#define TL_CMD_GO               0x80000000
#define TL_CMD_STOP             0x40000000
#define TL_CMD_ACK              0x20000000
#define TL_CMD_CHSEL7		0x10000000
#define TL_CMD_CHSEL6		0x08000000
#define TL_CMD_CHSEL5		0x04000000
#define TL_CMD_CHSEL4		0x02000000
#define TL_CMD_CHSEL3		0x01000000
#define TL_CMD_CHSEL2           0x00800000
#define TL_CMD_CHSEL1           0x00400000
#define TL_CMD_CHSEL0           0x00200000
#define TL_CMD_EOC              0x00100000
#define TL_CMD_RT               0x00080000
#define TL_CMD_NES              0x00040000
#define TL_CMD_ZERO0            0x00020000
#define TL_CMD_ZERO1            0x00010000
#define TL_CMD_ADRST            0x00008000
#define TL_CMD_LDTMR            0x00004000
#define TL_CMD_LDTHR            0x00002000
#define TL_CMD_REQINT           0x00001000
#define TL_CMD_INTSOFF          0x00000800
#define TL_CMD_INTSON		0x00000400
#define TL_CMD_RSVD0		0x00000200
#define TL_CMD_RSVD1		0x00000100
#define TL_CMD_ACK7		0x00000080
#define TL_CMD_ACK6		0x00000040
#define TL_CMD_ACK5		0x00000020
#define TL_CMD_ACK4		0x00000010
#define TL_CMD_ACK3		0x00000008
#define TL_CMD_ACK2		0x00000004
#define TL_CMD_ACK1		0x00000002
#define TL_CMD_ACK0		0x00000001

#define TL_CMD_CHSEL_MASK	0x01FE0000
#define TL_CMD_ACK_MASK		0xFF

/*
 * EEPROM address where station address resides.
 */
#define TL_EEPROM_EADDR		0x83
#define TL_EEPROM_EADDR2	0x99
#define TL_EEPROM_EADDR3	0xAF

/*
 * ThunderLAN host command register offsets.
 * (Can be accessed either by IO ports or memory map.)
 */
#define TL_HOSTCMD		0x00
#define TL_CH_PARM		0x04
#define TL_DIO_ADDR		0x08
#define TL_HOST_INT		0x0A
#define TL_DIO_DATA		0x0C

/*
 * ThunderLAN internal registers
 */
#define TL_NETCMD		0x00
#define TL_NETSIO		0x01
#define TL_NETSTS		0x02
#define TL_NETMASK		0x03

#define TL_NETCONFIG		0x04
#define TL_MANTEST		0x06

#define TL_VENID_LSB		0x08
#define TL_VENID_MSB		0x09
#define TL_DEVID_LSB		0x0A
#define TL_DEVID_MSB		0x0B

#define TL_REVISION		0x0C
#define TL_SUBCLASS		0x0D
#define TL_MINLAT		0x0E
#define TL_MAXLAT		0x0F

#define TL_AREG0_B5		0x10
#define TL_AREG0_B4		0x11
#define TL_AREG0_B3		0x12
#define TL_AREG0_B2		0x13

#define TL_AREG0_B1		0x14
#define TL_AREG0_B0		0x15
#define TL_AREG1_B5		0x16
#define TL_AREG1_B4		0x17

#define TL_AREG1_B3		0x18
#define TL_AREG1_B2		0x19
#define TL_AREG1_B1		0x1A
#define TL_AREG1_B0		0x1B

#define TL_AREG2_B5		0x1C
#define TL_AREG2_B4		0x1D
#define TL_AREG2_B3		0x1E
#define TL_AREG2_B2		0x1F

#define TL_AREG2_B1		0x20
#define TL_AREG2_B0		0x21
#define TL_AREG3_B5		0x22
#define TL_AREG3_B4		0x23

#define TL_AREG3_B3		0x24
#define TL_AREG3_B2		0x25
#define TL_AREG3_B1		0x26
#define TL_AREG3_B0		0x27

#define TL_HASH1		0x28
#define TL_HASH2		0x2C
#define TL_TXGOODFRAMES		0x30
#define TL_TXUNDERRUN		0x33
#define TL_RXGOODFRAMES		0x34
#define TL_RXOVERRUN		0x37
#define TL_DEFEREDTX		0x38
#define TL_CRCERROR		0x3A
#define TL_CODEERROR		0x3B
#define TL_MULTICOLTX		0x3C
#define TL_SINGLECOLTX		0x3E
#define TL_EXCESSIVECOL		0x40
#define TL_LATECOL		0x41
#define TL_CARRIERLOSS		0x42
#define TL_ACOMMIT		0x43
#define TL_LDREG		0x44
#define TL_BSIZEREG		0x45
#define TL_MAXRX		0x46

/*
 * ThunderLAN SIO register bits
 */
#define TL_SIO_MINTEN		0x80
#define TL_SIO_ECLOK		0x40
#define TL_SIO_ETXEN		0x20
#define TL_SIO_EDATA		0x10
#define TL_SIO_NMRST		0x08
#define TL_SIO_MCLK		0x04
#define TL_SIO_MTXEN		0x02
#define TL_SIO_MDATA		0x01

/*
 * Thunderlan NETCONFIG bits
 */
#define TL_CFG_RCLKTEST		0x8000
#define TL_CFG_TCLKTEST		0x4000
#define TL_CFG_BITRATE		0x2000
#define TL_CFG_RXCRC		0x1000
#define TL_CFG_PEF		0x0800
#define TL_CFG_ONEFRAG		0x0400
#define TL_CFG_ONECHAN		0x0200
#define TL_CFG_MTEST		0x0100
#define TL_CFG_PHYEN		0x0080
#define TL_CFG_MACSEL6		0x0040
#define TL_CFG_MACSEL5		0x0020
#define TL_CFG_MACSEL4		0x0010
#define TL_CFG_MACSEL3		0x0008
#define TL_CFG_MACSEL2		0x0004
#define TL_CFG_MACSEL1		0x0002
#define TL_CFG_MACSEL0		0x0001

/*
 * ThunderLAN NETSTS bits
 */
#define TL_STS_MIRQ		0x80
#define TL_STS_HBEAT		0x40
#define TL_STS_TXSTOP		0x20
#define TL_STS_RXSTOP		0x10

/*
 * ThunderLAN NETCMD bits
 */
#define TL_CMD_NRESET		0x80
#define TL_CMD_NWRAP		0x40
#define TL_CMD_CSF		0x20
#define TL_CMD_CAF		0x10
#define TL_CMD_NOBRX		0x08
#define TL_CMD_DUPLEX		0x04
#define TL_CMD_TRFRAM		0x02
#define TL_CMD_TXPACE		0x01

/*
 * ThunderLAN NETMASK bits
 */
#define TL_MASK_MASK7		0x80
#define TL_MASK_MASK6		0x40
#define TL_MASK_MASK5		0x20
#define TL_MASK_MASK4		0x10

/*
 * MII frame format
 */
#ifdef ANSI_DOESNT_ALLOW_BITFIELDS
struct tl_mii_frame {
	u_int16_t		mii_stdelim:2,
				mii_opcode:2,
				mii_phyaddr:5,
				mii_regaddr:5,
				mii_turnaround:2;
	u_int16_t		mii_data;
};
#else
struct tl_mii_frame {
	u_int8_t		mii_stdelim;
	u_int8_t		mii_opcode;
	u_int8_t		mii_phyaddr;
	u_int8_t		mii_regaddr;
	u_int8_t		mii_turnaround;
	u_int16_t		mii_data;
};
#endif
/*
 * MII constants
 */
#define TL_MII_STARTDELIM	0x01
#define TL_MII_READOP		0x02
#define TL_MII_WRITEOP		0x01
#define TL_MII_TURNAROUND	0x02

#define TL_LAST_FRAG		0x80000000
#define TL_CSTAT_UNUSED		0x8000
#define TL_CSTAT_FRAMECMP	0x4000
#define TL_CSTAT_READY		0x3000
#define TL_CSTAT_UNUSED13	0x2000
#define TL_CSTAT_UNUSED12	0x1000
#define TL_CSTAT_EOC		0x0800
#define TL_CSTAT_RXERROR	0x0400
#define TL_CSTAT_PASSCRC	0x0200
#define TL_CSTAT_DPRIO		0x0100

#define TL_FRAME_MASK		0x00FFFFFF
#define tl_tx_goodframes(x)	(x.tl_txstat & TL_FRAME_MASK)
#define tl_tx_underrun(x)	((x.tl_txstat & ~TL_FRAME_MASK) >> 24)
#define tl_rx_goodframes(x)	(x.tl_rxstat & TL_FRAME_MASK)
#define tl_rx_overrun(x)	((x.tl_rxstat & ~TL_FRAME_MASK) >> 24)

struct tl_stats {
	u_int32_t		tl_txstat;
	u_int32_t		tl_rxstat;
	u_int16_t		tl_deferred;
	u_int8_t		tl_crc_errors;
	u_int8_t		tl_code_errors;
	u_int16_t		tl_tx_multi_collision;
	u_int16_t		tl_tx_single_collision;
	u_int8_t		tl_excessive_collision;
	u_int8_t		tl_late_collision;
	u_int8_t		tl_carrier_loss;
	u_int8_t		acommit;
};

/*
 * These are the register definitions for the PHY (physical layer
 * interface chip).
 * The ThunderLAN chip has a built-in 10Mb/sec PHY which may be used
 * in some configurations. The Compaq 10/100 cards based on the ThunderLAN
 * use a National Semiconductor DP83840A PHY. The generic BMCR and BMSR
 * layouts for both PHYs are identical, however some of the bits are not
 * used by the ThunderLAN's internal PHY (most notably those dealing with
 * switching between 10 and 100Mb/sec speeds). Since Both PHYs use the
 * same bits, we #define them with generic names here.
 */
/*
 * PHY BMCR Basic Mode Control Register
 */
#define PHY_BMCR			0x00
#define PHY_BMCR_RESET			0x8000
#define PHY_BMCR_LOOPBK			0x4000
#define PHY_BMCR_SPEEDSEL		0x2000
#define PHY_BMCR_AUTONEGENBL		0x1000
#define PHY_BMCR_RSVD0			0x0800	/* write as zero */
#define PHY_BMCR_ISOLATE		0x0400
#define PHY_BMCR_AUTONEGRSTR		0x0200
#define PHY_BMCR_DUPLEX			0x0100
#define PHY_BMCR_COLLTEST		0x0080
#define PHY_BMCR_RSVD1			0x0040	/* write as zero, don't care */
#define PHY_BMCR_RSVD2			0x0020	/* write as zero, don't care */
#define PHY_BMCR_RSVD3			0x0010	/* write as zero, don't care */
#define PHY_BMCR_RSVD4			0x0008	/* write as zero, don't care */
#define PHY_BMCR_RSVD5			0x0004	/* write as zero, don't care */
#define PHY_BMCR_RSVD6			0x0002	/* write as zero, don't care */
#define PHY_BMCR_RSVD7			0x0001	/* write as zero, don't care */
/*
 * RESET: 1 == software reset, 0 == normal operation
 * Resets status and control registers to default values.
 * Relatches all hardware config values.
 *
 * LOOPBK: 1 == loopback operation enabled, 0 == normal operation
 *
 * SPEEDSEL: 1 == 100Mb/s, 0 == 10Mb/s
 * Link speed is selected byt his bit or if auto-negotiation if bit
 * 12 (AUTONEGENBL) is set (in which case the value of this register
 * is ignored).
 *
 * AUTONEGENBL: 1 == Autonegotiation enabled, 0 == Autonegotiation disabled
 * Bits 8 and 13 are ignored when autoneg is set, otherwise bits 8 and 13
 * determine speed and mode. Should be cleared and then set if PHY configured
 * for no autoneg on startup.
 *
 * ISOLATE: 1 == isolate PHY from MII, 0 == normal operation
 *
 * AUTONEGRSTR: 1 == restart autonegotiation, 0 = normal operation
 *
 * DUPLEX: 1 == full duplex mode, 0 == half duplex mode
 *
 * COLLTEST: 1 == collision test enabled, 0 == normal operation
 */

/* 
 * PHY, BMSR Basic Mode Status Register 
 */   
#define PHY_BMSR			0x01
#define PHY_BMSR_100BT4			0x8000
#define PHY_BMSR_100BTXFULL		0x4000
#define PHY_BMSR_100BTXHALF		0x2000
#define PHY_BMSR_10BTFULL		0x1000
#define PHY_BMSR_10BTHALF		0x0800
#define PHY_BMSR_RSVD1			0x0400	/* write as zero, don't care */
#define PHY_BMSR_RSVD2			0x0200	/* write as zero, don't care */
#define PHY_BMSR_RSVD3			0x0100	/* write as zero, don't care */
#define PHY_BMSR_RSVD4			0x0080	/* write as zero, don't care */
#define PHY_BMSR_MFPRESUP		0x0040
#define PHY_BMSR_AUTONEGCOMP		0x0020
#define PHY_BMSR_REMFAULT		0x0010
#define PHY_BMSR_CANAUTONEG		0x0008
#define PHY_BMSR_LINKSTAT		0x0004
#define PHY_BMSR_JABBER			0x0002
#define PHY_BMSR_EXTENDED		0x0001

#define PHY_CTL_IGLINK			0x8000
#define PHY_CTL_SWAPOL			0x4000
#define PHY_CTL_AUISEL			0x2000
#define PHY_CTL_SQEEN			0x1000
#define PHY_CTL_MTEST			0x0800
#define PHY_CTL_NFEW			0x0004
#define PHY_CTL_INTEN			0x0002
#define PHY_CTL_TINT			0x0001

#define TL_PHY_GENCTL			0x00
#define TL_PHY_GENSTS			0x01

/*
 * PHY Generic Identifier Register, hi bits
 */
#define TL_PHY_VENID			0x02

/*
 * PHY Generic Identifier Register, lo bits
 */
#define TL_PHY_DEVID			0x03

#define TL_PHY_ANAR			0x04
#define TL_PHY_LPAR			0x05 
#define TL_PHY_ANEXP			0x06

#define TL_PHY_PHYID			0x10
#define TL_PHY_CTL			0x11
#define TL_PHY_STS			0x12

#define TL_LPAR_RMFLT			0x2000
#define TL_LPAR_RSVD0			0x1000
#define TL_LPAR_RSVD1			0x0800
#define TL_LPAR_100BT4			0x0400
#define TL_LPAR_100BTXFULL		0x0200
#define TL_LPAR_100BTXHALF		0x0100
#define TL_LPAR_10BTFULL		0x0080
#define TL_LPAR_10BTHALF		0x0040

/*
 * PHY Antoneg advertisement register.
 */
#define PHY_ANAR			TL_PHY_ANAR
#define PHY_ANAR_NEXTPAGE		0x8000
#define PHY_ANAR_RSVD0			0x4000
#define PHY_ANAR_TLRFLT			0x2000
#define PHY_ANAR_RSVD1			0x1000
#define PHY_RSVD_RSDV2			0x0800
#define PHY_RSVD_RSVD3			0x0400
#define PHY_ANAR_100BT4			0x0200
#define PHY_ANAR_100BTXFULL		0x0100
#define PHY_ANAR_100BTXHALF		0x0080
#define PHY_ANAR_10BTFULL		0x0040
#define PHY_ANAR_10BTHALF		0x0020
#define PHY_ANAR_PROTO4			0x0010
#define PHY_ANAR_PROTO3			0x0008
#define PHY_ANAR_PROTO2			0x0004
#define PHY_AHAR_PROTO1			0x0002
#define PHY_AHAR_PROTO0			0x0001

/*
 * DP83840 PHY, PCS Confifguration Register
 */
#define TL_DP83840_PCS			0x17
#define TL_DP83840_PCS_LED4_MODE	0x0002
#define TL_DP83840_PCS_F_CONNECT	0x0020
#define TL_DP83840_PCS_BIT8		0x0100
#define TL_DP83840_PCS_BIT10		0x0400

/*
 * DP83840 PHY, PAR register
 */
#define TL_DP83840_PAR			0x19

#define PAR_RSVD0			0x8000
#define PAR_RSVD1			0x4000
#define PAR_RSVD2			0x2000
#define PAR_RSVD3			0x1000
#define PAR_DIS_CRS_JAB			0x0800
#define PAR_AN_EN_STAT			0x0400
#define PAR_RSVD4			0x0200
#define PAR_FEFI_EN			0x0100
#define PAR_DUPLEX_STAT			0x0080
#define PAR_SPEED_10			0x0040
#define PAR_CIM_STATUS			0x0020
#define PAR_PHYADDR4			0x0010
#define PAR_PHYADDR3			0x0008
#define PAR_PHYADDR2			0x0004
#define PAR_PHYADDR1			0x0002
#define PAR_PHYADDR0			0x0001


/*
 * Microchip Technology 24Cxx EEPROM control bytes
 */
#define EEPROM_CTL_READ			0xA1	/* 0101 0001 */
#define EEPROM_CTL_WRITE		0xA0	/* 0101 0000 */
