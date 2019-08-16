# HiFive_U-Boot
Development repository for HiFive Unleashed U-Boot

This repository contains the U-Boot created by Microsemi a Microchip company for RISC-V core on HiFive Unleashed 540 device. The HiFive Unleashed Board is used for HiFive U-Boot. 

### RISC-V cross tools

Get the RISC-V cross tool from [SiFive GitHub](https://github.com/sifive/freedom-u-sdk.git) and follow the README file for installation.

        $ git clone --recursive https://github.com/sifive/freedom-u-sdk.git    
        $ cd freedom-u-sdk    
        $ make

### Getting U-Boot sources

        $ git clone https://github.com/Microsemi-SoC-IP/HiFive_U-Boot
        $ cd HiFive_U-Boot
    
### Build U-Boot

1. Set the PATH

        $ export PATH=$PATH:/{path to freedom-u-sdk}/toolchain/bin
        
1. Create U-Boot configuration based on architecture defaults:

        $ make HiFive-U540_defconfig

1. Optionally edit the configuration via an ncurses interface:

        $ make menuconfig

1. Build the U-Boot image:

        $ make

### Load uboot image on SD card

1. Copy uboot.bin image to `freedom-u-sdk/work` directory
    
1. Connect SD card to a Host machine.
    
1. Open `make` file from `freedom-u-sdk` directory and edit at `.PHONY: format-boot-loader`

        .PHONY: format-boot-loader
        format-boot-loader: $(bin)
	       @test -b $(DISK) || (echo "$(DISK): is not a block device"; exit 1)
	       sgdisk --clear                                                               \
		          --new=1:2048:4095   --change-name=1:uboot      --typecode=1:$(FSBL)   \
		          --new=2:4096:69631  --change-name=2:bootloader --typecode=2:$(BBL)   \
		          --new=3:264192:     --change-name=3:root       --typecode=3:$(LINUX) \
		      $(DISK)
        @sleep 1
        ifeq ($(DISK)p1,$(wildcard $(DISK)p1))
	       @$(eval PART1 := $(DISK)p1)
	       @$(eval PART2 := $(DISK)p2)
	       @$(eval PART3 := $(DISK)p3)
        else ifeq ($(DISK)s1,$(wildcard $(DISK)s1))
	       @$(eval PART1 := $(DISK)s1)
	       @$(eval PART2 := $(DISK)s2)
	       @$(eval PART3 := $(DISK)s3)
        else ifeq ($(DISK)1,$(wildcard $(DISK)1))
	       @$(eval PART1 := $(DISK)1)
	       @$(eval PART2 := $(DISK)2)
	       @$(eval PART3 := $(DISK)3)
        else
	       @echo Error: Could not find bootloader partition for $(DISK)
	   @exit 1
	   
       endif
	   
	   dd if=/{Path_To}/freedom-u-sdk/work/u-boot.bin of=$(PART1) bs=4096
	   
	   dd if=/{Path_To}/freedom-u-sdk/work/bbl.bin of=$(PART2) bs=4096
	   
	   mke2fs -t ext3 $(PART3)
    
1. Go to `freedom-u-sdk` directory
    
        $ cd freedom-u-sdk
        $ sudo make DISK=/dev/sdb format-boot-loader
If any error, follow the [HiFive Getting started user guide](https://sifive.cdn.prismic.io/sifive%2Ffa3a584a-a02f-4fda-b758-a2def05f49f9_hifive-unleashed-getting-started-guide-v1p1.pdf) section 7.2.4.

1. Remove SD card from Host machine and insert it on the HiFive unleashed Board

1. Select dip switch for `mode select` to `1011`(boot from SD card)

1. Power suppy the board and connect usb cable to Host Machine and open serial terminal with 115200 baud rate.

1. Press reset button and see the uboot messages on serial terminal

### Boot Linux from SD card

Enter below commands on serial terminal

        # mmc_spi 1 20000000 0
        # mmc read 0x80000000 0x1000 0x10000
        # go 0x80000000
    
### Boot Linux from TFTP

Enter below commands on serial terminal

        # setenv serverip xx.xx.xx.xx
        # bootp
        # go 0x80000000
