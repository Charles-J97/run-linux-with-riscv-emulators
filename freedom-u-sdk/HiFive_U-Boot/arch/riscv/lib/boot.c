/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018 Microsemi Corporation
 * Padmarao Begari, Microsemi Corporation <Padmarao.Begari@microsemi.com>
 */

#include <common.h>
#include <command.h>

#define HIFIVE_HART0_MSIP	0x2000000
#define HIFIVE_HART1_MSIP	0x2000004
#define HIFIVE_HART2_MSIP	0x2000008
#define HIFIVE_HART3_MSIP	0x200000C
#define HIFIVE_HART4_MSIP	0x2000010
#define RAISE_SOFT_INT		0x1

unsigned long do_go_exec(ulong (*entry)(int, char * const []),
			 int argc, char * const argv[])
{
	cleanup_before_linux();
	*((volatile uint32_t *)(HIFIVE_HART1_MSIP)) = RAISE_SOFT_INT;
	*((volatile uint32_t *)(HIFIVE_HART2_MSIP)) = RAISE_SOFT_INT;
	*((volatile uint32_t *)(HIFIVE_HART3_MSIP)) = RAISE_SOFT_INT;
	*((volatile uint32_t *)(HIFIVE_HART4_MSIP)) = RAISE_SOFT_INT;

	asm volatile ("li a1, 0xF0000000\n\t"
			"csrr a0, mhartid\n\t"
	  "li t4, 0x80000000\n\t"
	  "jr t4\n\t");
	return entry(argc, argv);
}
