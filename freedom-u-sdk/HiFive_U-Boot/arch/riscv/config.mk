# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# Copyright (c) 2017 Microsemi Corporation.
# Padmarao Begari, Microsemi Corporation <padmarao.begari@microsemi.com>
#
# Copyright (C) 2017 Andes Technology Corporation
# Rick Chen, Andes Technology Corporation <rick@andestech.com>
#

ifeq ($(CROSS_COMPILE),)
ifdef CONFIG_32BIT
CROSS_COMPILE := riscv32-unknown-linux-gnu-
else
CROSS_COMPILE := riscv64-linux-gnu-
endif
endif

32bit-emul		:= elf32lriscv
64bit-emul		:= elf64lriscv

ifdef CONFIG_32BIT
PLATFORM_LDFLAGS	+= -m $(32bit-emul)
EFI_LDS			:= elf_riscv32_efi.lds
endif

ifdef CONFIG_64BIT
PLATFORM_LDFLAGS	+= -m $(64bit-emul)
EFI_LDS			:= elf_riscv64_efi.lds
endif
ifdef TARGET_AX25_AE350
CONFIG_STANDALONE_LOAD_ADDR = 0x00000000 \
			      -T $(srctree)/examples/standalone/riscv.lds

PLATFORM_CPPFLAGS	+= -ffixed-gp -fpic
PLATFORM_RELFLAGS += -fno-strict-aliasing -fno-common -gdwarf-2 -ffunction-sections
LDFLAGS_u-boot += --gc-sections -static -pie
else
CONFIG_STANDALONE_LOAD_ADDR = 0x08100000 \
			      -T $(srctree)/examples/standalone/riscv.lds

PLATFORM_RELFLAGS 	+= -fno-strict-aliasing -fno-common -gdwarf-2 -ffunction-sections
PLATFORM_CPPFLAGS	+= -ffixed-gp -mcmodel=medany -fpic 

LDFLAGS_u-boot = --gc-sections -static -pie
endif
EFI_CRT0		:= crt0_riscv_efi.o
EFI_RELOC		:= reloc_riscv_efi.o
