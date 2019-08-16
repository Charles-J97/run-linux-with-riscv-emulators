/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2018 Microsemi Corporation.
 * Copyright (c) 2018 Padmarao Begari <Padmarao.Begari@microsemi.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <fdtdec.h>
#include <spi.h>
#include <malloc.h>
#include <wait_bit.h>

#include <asm/io.h>

/* HiFive SPI register footprint */
struct hifive_spi_regs {
    volatile u32 sckdiv;
    volatile u32 sckmode;
    u32 RESRVED0[2];
    volatile u32 csid;
    volatile u32 csdef;
    volatile u32 csmode;
    u32 RESRVED1[3];
    volatile u32 delay0;
    volatile u32 delay1;
    u32 RESRVED2[4];
    volatile u32 fmt;
    u32 RESRVED3;
    volatile u32 txdata;
    volatile u32 rxdata;
    volatile u32 txmark;
    volatile u32 rxmark;
    u32 RESRVED4[2];
    volatile u32 fctrl;
    volatile u32 ffmt;
    u32 RESRVED5[2];
    volatile u32 ie;
    volatile u32 ip;
};

#define SPI_REG_SCKDIV          0x00
#define SPI_REG_SCKMODE         0x04
#define SPI_REG_CSID            0x10
#define SPI_REG_CSDEF           0x14
#define SPI_REG_CSMODE          0x18

#define SPI_REG_DCSSCK          0x28
#define SPI_REG_DSCKCS          0x2a
#define SPI_REG_DINTERCS        0x2c
#define SPI_REG_DINTERXFR       0x2e

#define SPI_REG_FMT             0x40
#define SPI_REG_TXFIFO          0x48
#define SPI_REG_RXFIFO          0x4c
#define SPI_REG_TXCTRL          0x50
#define SPI_REG_RXCTRL          0x54

#define SPI_REG_FCTRL           0x60
#define SPI_REG_FFMT            0x64

#define SPI_REG_IE              0x70
#define SPI_REG_IP              0x74

/* Fields */

#define SPI_SCK_PHA             0x1
#define SPI_SCK_POL             0x2

#define SPI_FMT_PROTO(x)        ((x) & 0x3)
#define SPI_FMT_ENDIAN(x)       (((x) & 0x1) << 2)
#define SPI_FMT_DIR(x)          (((x) & 0x1) << 3)
#define SPI_FMT_LEN(x)          (((x) & 0xf) << 16)

/* TXCTRL register */
#define SPI_TXWM(x)             ((x) & 0xffff)
/* RXCTRL register */
#define SPI_RXWM(x)             ((x) & 0xffff)

#define SPI_IP_TXWM             0x1
#define SPI_IP_RXWM             0x2

#define SPI_FCTRL_EN            0x1

#define SPI_INSN_CMD_EN         0x1
#define SPI_INSN_ADDR_LEN(x)    (((x) & 0x7) << 1)
#define SPI_INSN_PAD_CNT(x)     (((x) & 0xf) << 4)
#define SPI_INSN_CMD_PROTO(x)   (((x) & 0x3) << 8)
#define SPI_INSN_ADDR_PROTO(x)  (((x) & 0x3) << 10)
#define SPI_INSN_DATA_PROTO(x)  (((x) & 0x3) << 12)
#define SPI_INSN_CMD_CODE(x)    (((x) & 0xff) << 16)
#define SPI_INSN_PAD_CODE(x)    (((x) & 0xff) << 24)

#define SPI_TXFIFO_FULL  (1 << 31)
#define SPI_RXFIFO_EMPTY (1 << 31)

/* Values */

#define SPI_CSMODE_AUTO         0
#define SPI_CSMODE_HOLD         2
#define SPI_CSMODE_OFF          3

#define SPI_DIR_RX              0
#define SPI_DIR_TX              1

#define SPI_PROTO_S             0
#define SPI_PROTO_D             1
#define SPI_PROTO_Q             2

#define SPI_ENDIAN_MSB          0
#define SPI_ENDIAN_LSB          1

#define KHZ_400 400

#ifndef CONFIG_DM_SPI

/* hifive spi slave */
struct hifive_spi_slave {
    struct spi_slave slave;
    struct hifive_spi_regs *regs;
};

/**
 * Get smallest clock divisor that divides input_khz to a quotient less than or
 * equal to max_target_khz;
 */
inline unsigned int spi_min_clk_divisor(unsigned int input_khz, unsigned int max_target_khz)
{
  // f_sck = f_in / (2 * (div + 1)) => div = (f_in / (2*f_sck)) - 1
  //
  // The nearest integer solution for div requires rounding up as to not exceed
  // max_target_khz.
  //
  // div = ceil(f_in / (2*f_sck)) - 1
  //     = floor((f_in - 1 + 2*f_sck) / (2*f_sck)) - 1
  //
  // This should not overflow as long as (f_in - 1 + 2*f_sck) does not exceed
  // 2^32 - 1, which is unlikely since we represent frequencies in kHz.
  unsigned int quotient = (input_khz + 2 * max_target_khz - 1) / (2 * max_target_khz);
  // Avoid underflow
  if (quotient == 0) {
    return 0;
  } else {
    return quotient - 1;
  }
}

static inline struct hifive_spi_slave *to_hifivespi_slave(struct spi_slave *slave)
{
    return container_of(slave, struct hifive_spi_slave, slave);
}

void spi_set_speed(struct spi_slave *slave, uint hz)
{
	struct hifive_spi_slave *as = to_hifivespi_slave(slave);
	as->regs->sckdiv = spi_min_clk_divisor(HIFIVE_PERIPH_CLK_FREQ/1000, hz/1000);
}
/* spi_init is called during boot when CONFIG_CMD_SPI is defined */
void spi_init(void)
{
   /*
    * configuration will be done in spi_setup_slave()
    */
}

/* the following is called in sequence by do_spi_xfer() */
struct spi_slave *spi_setup_slave(uint bus, uint cs, uint max_hz, uint mode)
{
    struct hifive_spi_slave *sslave;


    if (mode & SPI_SLAVE) {
    	printf("slave mode not supported\n");
        return NULL;
    }

    sslave = spi_alloc_slave(struct hifive_spi_slave, bus, cs);
    if (!sslave) {
        printf("SPI_error: Fail to allocate hifive_spi_slave\n");
        return NULL;
    }


    sslave->regs = (struct hifive_spi_regs *)HIFIVE_BASE_SPI;
    /* proto = siggle, dir = Rx, endian = MSB, len = 8 */
    sslave->regs->fmt = 0x00080000;
    sslave->regs->csdef |= 0x1;
    sslave->regs->csid = 0;
    sslave->regs->sckdiv = spi_min_clk_divisor(HIFIVE_PERIPH_CLK_FREQ/1000, max_hz/1000);
    sslave->regs->csmode = SPI_CSMODE_OFF;

    return &sslave->slave;
}
void spi_free_slave(struct spi_slave *slave)
{
    struct hifive_spi_slave *cslave = to_hifivespi_slave(slave);

    free(cslave);
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen,
    const void *dout, void *din, unsigned long flags)
{
	struct hifive_spi_slave *as = to_hifivespi_slave(slave);
	unsigned int	len_tx;
	unsigned int	len_rx;
	unsigned int	len;
	u32     out, i;
	const u8	*txp = dout;
	u8		*rxp = din;
	u8		value;

	if (bitlen == 0)
		/* Finish any previously submitted transfers */
		goto out;

	/*
	 * TODO: The controller can do non-multiple-of-8 bit
	 * transfers, but this driver currently doesn't support it.
	 *
	 * It's also not clear how such transfers are supposed to be
	 * represented as a stream of bytes...this is a limitation of
	 * the current SPI interface.
	 */
	if (bitlen % 8) {
		/* Errors always terminate an ongoing transfer */
		flags |= SPI_XFER_END;
		goto out;
	}

	len = bitlen / 8;

	if((txp == NULL) && (rxp == NULL))
	{
	    as->regs->csmode = SPI_CSMODE_OFF;
	}
	out = as->regs->rxdata;
	while((out & SPI_RXFIFO_EMPTY) == 0)
		out = as->regs->rxdata;
	/*
	 * The controller can do automatic CS control, but it is
	 * somewhat quirky, and it doesn't really buy us much anyway
	 * in the context of U-Boot.
	 */

	if (flags & SPI_XFER_BEGIN)
	{
		spi_cs_activate(slave);
	}

	for (len_tx = 0, len_rx = 0; len_rx < len; )
	{
		while(as->regs->txdata & SPI_TXFIFO_FULL);

		if (len_tx < len)
		{
			if (txp)
				value = *txp++;
			else
				value = 0xFF;
			as->regs->txdata = value;
			len_tx++;
			for(i = 0; i < 2; i++);
		}
		out = as->regs->rxdata;
		if((out & SPI_RXFIFO_EMPTY) == 0)
		{

			value = (u8)(out & 0xFF);
			if (rxp)
			{
				*rxp++ = value;
			}
			len_rx++;
		}

	}

out:
	if (flags & SPI_XFER_END)
	{
		spi_cs_deactivate(slave);
	}
	return 0;
}
int spi_claim_bus(struct spi_slave *slave)
{
    return 0;
}
void spi_release_bus(struct spi_slave *slave)
{
    struct hifive_spi_slave *as = to_hifivespi_slave(slave);
    as->regs->csmode = SPI_CSMODE_AUTO;
}
int spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
    return 1;
}

void spi_cs_activate(struct spi_slave *slave)
{
    struct hifive_spi_slave *as = to_hifivespi_slave(slave);
    as->regs->csmode = SPI_CSMODE_HOLD;
}

void spi_cs_deactivate(struct spi_slave *slave)
{
    struct hifive_spi_slave *as = to_hifivespi_slave(slave);
    as->regs->csmode = SPI_CSMODE_AUTO;
}
#endif
