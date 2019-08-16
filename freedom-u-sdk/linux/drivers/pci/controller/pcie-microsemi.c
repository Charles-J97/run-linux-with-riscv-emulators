/*
 * PCIe host controller driver for Microsemi AXI PCIe Bridge
 *
 * Copyright (c) 2018 - Microsemi.
 * Author: Badal Nilawar <badal.nilawar@microsemi.com>
 *
 * Based on the Xilinx, Altera PCIe driver
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include "../pci.h"

/* ECAM definitions */
#define ECAM_BUS_NUM_SHIFT		20
#define ECAM_DEV_NUM_SHIFT		12

/* Number of MSI IRQs */
#define MICROSEMI_NUM_MSI_IRQS		32

/* PCIe Bridge Phy and Controller Phy offsets */
#define PCIE0_BRIDGE_PHY_ADDR_OFFSET			0x03004000u
#define PCIE0_CRTL_PHY_ADDR_OFFSET			0x03006000u

#define PCIE0_BRIDGE_ADDR		0x03004000u
#define PCIE0_CRTL_ADDR			0x03006000u

#define PCIE1_BRIDGE_ADDR		0x00008000u
#define PCIE1_CRTL_ADDR			0x0000A000u

/* PCIe LTSSM State reg */
#define LTSSM_STATE		0x5c

/* PCIe LTSSM L0 state */
#define LTSSM_L0_STATE		0x10

/* PCIe Controller Phy Regs */
#define SEC_ERROR_INT		0x28
#define SEC_ERROR_INT_MASK	0x2c
#define DED_ERROR_INT		0x30
#define DED_ERROR_INT_MASK	0x34
#define ECC_CONTROL		0x38
#define PCIE_EVENT_INT		0x14c

/* PCIe Bridge Phy Regs */
#define IMASK_LOCAL		0x180
#define ISTATUS_LOCAL		0x184
#define IMASK_HOST		0x188
#define ISTATUS_HOST		0x18c
#define	ISTATUS_MSI		0x194
#define	PCIE_PCI_IDS_DW1	0x9c

/* PCIe AXI slave table init defines */
#define ATR0_AXI4_SLV0_SRCADDR_PARAM	0x800u
#define ATR0_AXI4_SLV0_SRC_ADDR		0x804u
#define ATR0_AXI4_SLV0_TRSL_ADDR_LSB	0x808u
#define ATR0_AXI4_SLV0_TRSL_ADDR_UDW	0x80cu
#define ATR0_AXI4_SLV0_TRSL_PARAM	0x810u

#define ATR1_AXI4_SLV0_SRCADDR_PARAM	0x820u
#define ATR1_AXI4_SLV0_SRC_ADDR		0x824u
#define ATR1_AXI4_SLV0_TRSL_ADDR_LSB	0x828u
#define ATR1_AXI4_SLV0_TRSL_ADDR_UDW	0x82cu
#define ATR1_AXI4_SLV0_TRSL_PARAM	0x830u

/* PCIe Master table init defines */
#define ATR0_PCIE_WIN0_SRCADDR_PARAM	0x600u

/* Translated ID */
#define  PCIE_TX_RX_INTERFACE		0x00000000u
#define  PCIE_CONFIG_INTERFACE		0x00000001u

/* PCIe Config space MSI capability structure */
#define PCIE_ENABLE_MSI			0x10000000u

/* MSI definitions */
#define MSI_MSG_ADDR			0x190u
#define MSI_ENABLE			(0x01u << 16)
#define MSI_ENABLE_MULTI		(MICROSEMI_NUM_MSI_IRQS << 20)

/* MSI Capability Structure  */
#define MSI_CAP_CTRL			0xE0u
#define MSI_MSG_ADDR_OFFSET		0xE4u
#define MSI_MSG_UPPER_ADDR_OFFEST	0xE8u
#define MSI_MSG_DATA			0xF0u



/******************************************************************************/
/*Enable PCIe local*/
#define PCIE_LOCAL_INT_ENABLE		0xF000000u

/******************************************************************************/
/* Clear PCIe interrupt events */
#define PCIE_EVENT_INT_DATA		0x00070007u
#define PCIE_ECC_DISABLE	        0x0F000000u
#define PCIE_SEC_ERROR_INT_CLEAR	0x0000FFFFu
#define PCIE_DED_ERROR_INT_CLEAR	0x0000FFFFu
#define PCIE_ISTATUS_CLEAR		0xFFFFFFFFu
#define PCIE_CLEAR			0x00000000u
#define PCIE_SET			0x00000001u

#define ROOT_PORT_ENABLE		0x00000001u

#define NULL_POINTER			0x00000000u

/*****************************************************************************/
/* PCIe Controller 0 */
#define PF_PCIE_CTRL_0                 0u
/* PCIe Controller 1 */
#define PF_PCIE_CTRL_1                 1u

/* It indicates that the ATR table is enabled */
#define PF_PCIE_ATR_TABLE_ENABLE       1u
/* It indicates that the the ATR table is disabled */
#define PF_PCIE_ATR_TABLE_DISABLE      0u


/**
 * struct microsemi_pcie_port - PCIe port information
 * @reg_base: IO Mapped Register Base
 * @irq: Interrupt number
 * @root_busno: Root Bus number
 * @dev: Device pointer
 * @msi_domain: MSI IRQ domain pointer
 * @leg_domain: Legacy IRQ domain pointer
 * @resources: Bus Resources
 */
struct microsemi_pcie_port {
	struct platform_device	*pdev;
	void __iomem *reg_base;
	void __iomem *reg_base_apb;
	void __iomem *reg_bridge_apb;
	void __iomem *reg_ctrl_apb;
	u32 irq;
	u8 root_busno;
	struct device *dev;
	struct irq_domain *msi_domain;
	struct irq_domain *leg_domain;
	struct list_head resources;
};

static DECLARE_BITMAP(msi_irq_in_use, MICROSEMI_NUM_MSI_IRQS);

static inline u32 pcie_read(struct microsemi_pcie_port *port, u32 reg)
{
	return readl(port->reg_base + reg);
}

static inline void pcie_write(struct microsemi_pcie_port *port,
				u32 val, u32 reg)
{
	writel(val, port->reg_base + reg);
}

static inline bool microsemi_pcie_link_up(struct microsemi_pcie_port *port)
{
	return (readl(port->reg_ctrl_apb + LTSSM_STATE)
		& LTSSM_L0_STATE) ? 1 : 0;
}

/**
 * microsemi_pcie_valid_device - Check if a valid device is present on bus
 * @bus: PCI Bus structure
 * @devfn: device/function
 *
 * Return: 'true' on success and 'false' if invalid device is found
 */
static bool microsemi_pcie_valid_device(struct pci_bus *bus, unsigned int devfn)
{
	struct microsemi_pcie_port *port = bus->sysdata;

	/* Check if link is up when trying to access downstream ports */
	if (bus->number != port->root_busno)
		if (!microsemi_pcie_link_up(port))
			return false;

	/* Only one device down on each root port */
	if (bus->number == port->root_busno && devfn > 0)
		return false;

	return true;
}

/**
 * microsemi_pcie_map_bus - Get configuration base
 * @bus: PCI Bus structure
 * @devfn: Device/function
 * @where: Offset from base
 *
 * Return: Base address of the configuration space needed to be
 *	   accessed.
 */
static void __iomem *microsemi_pcie_map_bus(struct pci_bus *bus,
					 unsigned int devfn, int where)
{
	struct microsemi_pcie_port *port = bus->sysdata;
	int relbus;


	if (!microsemi_pcie_valid_device(bus, devfn))
		return NULL;

	relbus = (bus->number << ECAM_BUS_NUM_SHIFT) |
		 (devfn << ECAM_DEV_NUM_SHIFT);


	return port->reg_base + relbus + where;
}

/* PCIe operations */
static struct pci_ops microsemi_pcie_ops = {
	.map_bus = microsemi_pcie_map_bus,
	.read	= pci_generic_config_read,
	.write	= pci_generic_config_write,
};

/* MSI functions */

/**
 * microsemi_pcie_destroy_msi - Free MSI number
 * @irq: IRQ to be freed
 */
static void microsemi_pcie_destroy_msi(unsigned int irq)
{
	struct msi_desc *msi;
	struct microsemi_pcie_port *port;
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	if (!test_bit(hwirq, msi_irq_in_use)) {
		msi = irq_get_msi_desc(irq);
		port = msi_desc_to_pci_sysdata(msi);
		dev_err(port->dev, "Trying to free unused MSI#%d\n", irq);
	} else {
		clear_bit(hwirq, msi_irq_in_use);
	}
}

/**
 * microsemi_pcie_assign_msi - Allocate MSI number
 *
 * Return: A valid IRQ on success and error value on failure.
 */
static int microsemi_pcie_assign_msi(void)
{
	int pos;

	pos = find_first_zero_bit(msi_irq_in_use, MICROSEMI_NUM_MSI_IRQS);
	if (pos < MICROSEMI_NUM_MSI_IRQS)
		set_bit(pos, msi_irq_in_use);
	else
		return -ENOSPC;

	return pos;
}

/**
 * microsemi_msi_teardown_irq - Destroy the MSI
 * @chip: MSI Chip descriptor
 * @irq: MSI IRQ to destroy
 */
static void microsemi_msi_teardown_irq(struct msi_controller *chip,
				    unsigned int irq)
{
	microsemi_pcie_destroy_msi(irq);
	irq_dispose_mapping(irq);
}

/**
 * microsemi_pcie_msi_setup_irq - Setup MSI request
 * @chip: MSI chip pointer
 * @pdev: PCIe device pointer
 * @desc: MSI descriptor pointer
 *
 * Return: '0' on success and error value on failure
 */
static int microsemi_pcie_msi_setup_irq(struct msi_controller *chip,
				     struct pci_dev *pdev,
				     struct msi_desc *desc)
{
	struct microsemi_pcie_port *port = pdev->bus->sysdata;
	unsigned int irq;
	int hwirq;
	struct msi_msg msg;

	hwirq = microsemi_pcie_assign_msi();
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(port->msi_domain, hwirq);
	if (!irq)
		return -EINVAL;

	irq_set_msi_desc(irq, desc);

	msg.address_hi = 0;
	msg.address_lo = MSI_MSG_ADDR;
	msg.data = hwirq;

	pci_write_msi_msg(irq, &msg);

	return 0;
}

/* MSI Chip Descriptor */
static struct msi_controller microsemi_pcie_msi_chip = {
	.setup_irq = microsemi_pcie_msi_setup_irq,
	.teardown_irq = microsemi_msi_teardown_irq,
};

/* HW Interrupt Chip Descriptor */
static struct irq_chip microsemi_msi_irq_chip = {
	.name = "Microsemi PCIe MSI",
	.irq_enable = pci_msi_unmask_irq,
	.irq_disable = pci_msi_mask_irq,
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

/**
 * microsemi_pcie_msi_map - Set the handler for the MSI and mark IRQ as valid
 * @domain: IRQ domain
 * @irq: Virtual IRQ number
 * @hwirq: HW interrupt number
 *
 * Return: Always returns 0.
 */
static int microsemi_pcie_msi_map(struct irq_domain *domain, unsigned int irq,
			       irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq,
				&microsemi_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

/* IRQ Domain operations */
static const struct irq_domain_ops msi_domain_ops = {
	.map = microsemi_pcie_msi_map,
};

/**
 * microsemi_pcie_enable_msi - Enable MSI support
 * @port: PCIe port information
 */
static void microsemi_pcie_enable_msi(struct microsemi_pcie_port *port)
{
	u32 cap_ctrl;

	cap_ctrl = pcie_read(port, MSI_CAP_CTRL);

	pcie_write(port, cap_ctrl | MSI_ENABLE_MULTI | MSI_ENABLE, MSI_CAP_CTRL);
	pcie_write(port, MSI_MSG_ADDR, MSI_MSG_ADDR_OFFSET);
}

/* INTx Functions */

/**
 * microsemi_pcie_intx_map - Set the handler for the INTx and mark IRQ as valid
 * @domain: IRQ domain
 * @irq: Virtual IRQ number
 * @hwirq: HW interrupt number
 *
 * Return: Always returns 0.
 */
static int microsemi_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
				irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

/* INTx IRQ Domain operations */
static const struct irq_domain_ops intx_domain_ops = {
	.map = microsemi_pcie_intx_map,
	.xlate = pci_irqd_intx_xlate,
};

/* PCIe HW Functions */

/**
 * microsemi_pcie_intr_handler - Interrupt Service Handler
 * @irq: IRQ number
 * @data: PCIe port information
 *
 * Return: IRQ_HANDLED on success and IRQ_NONE on failure
 */
static irqreturn_t microsemi_pcie_intr_handler(int irq, void *data)
{
	struct microsemi_pcie_port *port = (struct microsemi_pcie_port *)data;
	struct device *dev = port->dev;
	unsigned long status;
	unsigned long msi;
	u32 bit;
	u32 virq;


	status = readl(port->reg_bridge_apb + ISTATUS_LOCAL);

	status = (status >> 24) & 0x0f;
	for_each_set_bit(bit, &status, PCI_NUM_INTX) {
		/* clear interrupts */
		writel(1 << (bit+24),
			port->reg_bridge_apb + ISTATUS_LOCAL);

		virq = irq_find_mapping(port->leg_domain, bit);

		if (virq)
			generic_handle_irq(virq);
		else
			dev_err(dev, "unexpected IRQ, INT%d\n", bit);
	}

	status = readl(port->reg_bridge_apb + ISTATUS_LOCAL);
	if ((status & 0x10000000) == 0x10000000) {
		writel((1 << 28), port->reg_bridge_apb + ISTATUS_LOCAL);
		msi = readl(port->reg_bridge_apb + ISTATUS_MSI);
		for_each_set_bit(bit, &msi, MICROSEMI_NUM_MSI_IRQS) {
		/* clear interrupts */
			writel((1 << bit),
				port->reg_bridge_apb + ISTATUS_MSI);
			virq = irq_find_mapping(port->msi_domain, bit);
			if (virq)
				generic_handle_irq(virq);
			else
				dev_err(dev, "unexpected IRQ, INT%d\n", bit);
		}
	}

	return IRQ_HANDLED;
}

/**
 * microsemi_pcie_init_irq_domain - Initialize IRQ domain
 * @port: PCIe port information
 *
 * Return: '0' on success and error value on failure
 */
static int microsemi_pcie_init_irq_domain(struct microsemi_pcie_port *port)
{
	struct device *dev = port->dev;
	struct device_node *node = dev->of_node;
	struct device_node *pcie_intc_node;

	/* Setup INTx */
	pcie_intc_node = of_get_next_child(node, NULL);
	if (!pcie_intc_node) {
		dev_err(dev, "No PCIe Intc node found\n");
		return -ENODEV;
	}
	dev_info(dev, "Intc node foundi %s\n", pcie_intc_node->name);

	port->leg_domain = irq_domain_add_linear(pcie_intc_node, PCI_NUM_INTX,
						 &intx_domain_ops,
						 port);
	if (!port->leg_domain) {
		dev_err(dev, "Failed to get a INTx IRQ domain\n");
		return -ENODEV;
	}

	/* Setup MSI */
	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		port->msi_domain = irq_domain_add_linear(node,
						MICROSEMI_NUM_MSI_IRQS,
						&msi_domain_ops,
						&microsemi_pcie_msi_chip);
		if (!port->msi_domain) {
			dev_err(dev, "Failed to get a MSI IRQ domain\n");
			return -ENODEV;
		}
		microsemi_pcie_enable_msi(port);
	}
	return 0;
}

/**
 * microsemi_pcie_init_port - Parse Device tree, Initialize hardware
 * @port: PCIe port information
 *
 * Return: '0' on success and error value on failure
 */
static int microsemi_pcie_init_port(struct microsemi_pcie_port *port)
{
	struct device *dev = port->dev;
	struct device_node *node = dev->of_node;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource regs;
	struct resource regs1;
	resource_size_t size;
	u32 bit, pf_bridge_id = 1;
	const char *type;
	int err;

	type = of_get_property(node, "device_type", NULL);
	if (!type || strcmp(type, "pci")) {
		dev_err(dev, "invalid \"device_type\" %s\n", type);
		return -EINVAL;
	}

	err = of_address_to_resource(node, 0, &regs);
	if (err) {
		dev_err(dev, "missing \"reg\" property\n");
		return err;
	}

	port->reg_base = devm_pci_remap_cfg_resource(dev, &regs);
	if (IS_ERR(port->reg_base))
		return PTR_ERR(port->reg_base);


	err = of_address_to_resource(node, 1, &regs1);
	if (err) {
		dev_err(dev, "missing \"reg\" property\n");
		return err;
	}


	port->reg_base_apb = devm_ioremap_resource(dev, &regs1);
	if (IS_ERR(port->reg_base_apb))
		return PTR_ERR(port->reg_base_apb);

	if (pf_bridge_id == 0) {
		port->reg_bridge_apb =  port->reg_base_apb + PCIE0_BRIDGE_ADDR;
		port->reg_ctrl_apb = port->reg_base_apb + PCIE0_CRTL_ADDR;
	} else {
		port->reg_bridge_apb =  port->reg_base_apb + PCIE1_BRIDGE_ADDR;
		port->reg_ctrl_apb = port->reg_base_apb + PCIE1_CRTL_ADDR;
	}

	port->irq = irq_of_parse_and_map(node, 0);

	err = devm_request_irq(dev, port->irq, microsemi_pcie_intr_handler,
			       IRQF_SHARED | IRQF_NO_THREAD,
			       "microsemi-pcie", port);
	if (err) {
		dev_err(dev, "unable to request irq %d\n", port->irq);
		return err;
	}


	/* Clear and Disable interrupts */

	writel(0x0f000000, port->reg_ctrl_apb + ECC_CONTROL);
	writel(0x00070007, port->reg_ctrl_apb + PCIE_EVENT_INT);
	writel(0x0000ffff, port->reg_ctrl_apb + SEC_ERROR_INT);
	writel(0x0000ffff, port->reg_ctrl_apb + SEC_ERROR_INT_MASK);
	writel(0x0000ffff, port->reg_ctrl_apb + DED_ERROR_INT);
	writel(0x0000ffff, port->reg_ctrl_apb + DED_ERROR_INT_MASK);

	writel(0x00000000, port->reg_bridge_apb + IMASK_LOCAL);
	writel(0xffffffff, port->reg_bridge_apb + ISTATUS_LOCAL);
	writel(0x00000000, port->reg_bridge_apb + IMASK_HOST);
	writel(0xffffffff, port->reg_bridge_apb + ISTATUS_HOST);

	dev_info(dev, "interrupt disabled\n");

	/* Configure Address Translation Table 0 for pcie config space */

	writel(PCIE_CONFIG_INTERFACE,
		port->reg_bridge_apb + ATR0_AXI4_SLV0_TRSL_PARAM);

	size = resource_size(&regs);

	bit = find_first_bit((const unsigned long *)&size, 64) - 1;

	writel((u32)regs.start | bit << 1 | 0x01,
		port->reg_bridge_apb + ATR0_AXI4_SLV0_SRCADDR_PARAM);

//	writel((u32)(regs.start >> 32),
//		port->reg_bridge_apb + ATR0_AXI4_SLV0_SRC_ADDR);

	writel((u32)regs.start,
		port->reg_bridge_apb + ATR0_AXI4_SLV0_TRSL_ADDR_LSB);

//	writel((u32)(regs.start >> 32),
//		port->reg_bridge_apb + ATR0_AXI4_SLV0_TRSL_ADDR_UDW);


	if (of_pci_range_parser_init(&parser, node)) {
		dev_err(dev, "missing \"ranges\" property\n");
		return -EINVAL;
	}


	for_each_of_pci_range(&parser, &range) {
		switch (range.flags & IORESOURCE_TYPE_BITS) {
		case IORESOURCE_MEM:

		size = range.size;
		bit = find_first_bit((const unsigned long *)&size, 64) - 1;

		/* Configure Address Translation Table 1 for pcie mem space */

		writel(PCIE_TX_RX_INTERFACE,
			port->reg_bridge_apb + ATR1_AXI4_SLV0_TRSL_PARAM);

		writel((u32)range.cpu_addr | bit << 1 | 0x01,
			port->reg_bridge_apb + ATR1_AXI4_SLV0_SRCADDR_PARAM);

//		writel((u32)(range.cpu_addr >> 32),
//			port->reg_bridge_apb + ATR1_AXI4_SLV0_SRC_ADDR);

		writel((u32)range.pci_addr,
			port->reg_bridge_apb + ATR1_AXI4_SLV0_TRSL_ADDR_LSB);

//		writel((u32)(range.pci_addr >> 32),
//			port->reg_bridge_apb + ATR1_AXI4_SLV0_TRSL_ADDR_UDW);

		break;
		}

	}


	writel(readl(port->reg_bridge_apb + ATR0_PCIE_WIN0_SRCADDR_PARAM)
		| 0x3E,
		port->reg_bridge_apb + ATR0_PCIE_WIN0_SRCADDR_PARAM);

	writel(0, port->reg_bridge_apb + 0x604);

	writel((readl(port->reg_bridge_apb + PCIE_PCI_IDS_DW1) & 0xffff)
		| (PCI_CLASS_BRIDGE_PCI << 16),
		port->reg_bridge_apb + PCIE_PCI_IDS_DW1);

	pcie_write(port, 0x00ff0100, 0x18);

	/* Enable interrupts */
	writel(PCIE_ENABLE_MSI | PCIE_LOCAL_INT_ENABLE,
		port->reg_bridge_apb + IMASK_LOCAL);


	return 0;
}

/**
 * microsemi_pcie_probe - Probe function
 * @pdev: Platform device pointer
 *
 * Return: '0' on success and error value on failure
 */
static int microsemi_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct microsemi_pcie_port *port;
	struct pci_bus *bus, *child;
	struct pci_host_bridge *bridge;
	int err;
	resource_size_t iobase = 0;
	LIST_HEAD(res);

	pr_err("%s In \n", __func__);
	if (!dev->of_node)
		return -ENODEV;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(*port));
	if (!bridge)
		return -ENODEV;

	port = pci_host_bridge_priv(bridge);

	port->dev = dev;
	port->pdev = pdev;

	err = microsemi_pcie_init_port(port);
	if (err) {
		dev_err(dev, "Pcie port initialization failed\n");
		return err;
	}


	err = microsemi_pcie_init_irq_domain(port);
	if (err) {
		dev_err(dev, "Failed creating IRQ Domain\n");
		return err;
	}

	err = devm_of_pci_get_host_bridge_resources(dev, 0, 0xff, &res,
					       &iobase);
	if (err) {
		dev_err(dev, "Getting bridge resources failed\n");
		return err;
	}

	err = devm_request_pci_bus_resources(dev, &res);
	if (err)
		goto error;


	list_splice_init(&res, &bridge->windows);
	bridge->dev.parent = dev;
	bridge->sysdata = port;
	bridge->busnr = 0;
	bridge->ops = &microsemi_pcie_ops;
	bridge->map_irq = of_irq_parse_and_map_pci;
	bridge->swizzle_irq = pci_common_swizzle;

#ifdef CONFIG_PCI_MSI
	microsemi_pcie_msi_chip.dev = dev;
	bridge->msi = &microsemi_pcie_msi_chip;
#endif
	err = pci_scan_root_bus_bridge(bridge);
	dev_info(dev, "pci_scan_root_bus_bridge done\n");
	if (err < 0)
		goto error;

	bus = bridge->bus;

	pci_assign_unassigned_bus_resources(bus);
	list_for_each_entry(child, &bus->children, node)
		pcie_bus_configure_settings(child);
	pci_bus_add_devices(bus);

	return 0;

error:
	pci_free_resource_list(&res);
	return err;
}

static const struct of_device_id microsemi_pcie_of_match[] = {
	{ .compatible = "ms-pf,axi-pcie-host", },
	{}
};

static struct platform_driver microsemi_pcie_driver = {
	.driver = {
		.name = "microsemi-pcie",
		.of_match_table = microsemi_pcie_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = microsemi_pcie_probe,
};
builtin_platform_driver(microsemi_pcie_driver);
