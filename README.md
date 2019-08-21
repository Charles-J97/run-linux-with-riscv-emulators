# Run linux with riscv emulators
Teach you how to run linux on riscv emulators in several ways `from scratch`. It includes the building of the whole process of running a linux kernel in qemu or spike and includes several ways so that you can choose one way you like to try. 

NOTICE THAT 1: All codes are only sutiable for `Ubuntu` and they have been successfully run in `Ubuntu 18.04(64 bit)`.
NOTICE THAT 2: My linux account name is charles, so the user's address is `/home/charles`. The account name depends on you. Here is just the example.
NOTICE THAT 3: All commands make can be added -j $(nproc), like `make -j $(nproc)`, to make the porcess faster. For example, my computer has 8 CPUs, so it has 16 processes, actually and virtually. Then I can change the command `make` to `make -j16`. This will be faster.

## Preview
Recently I have found that there are many repositories about modules related to riscv emulators such as qemu or Spike, but as to how to run a linux kernel with qemu or spike from scratch there are less information. Maybe it is so fundamental that people have no interests writing the instructions, but these instructions are important to rookies who touch the linux or riscv newly.

## riscv-gnu-toolchain
This is the RISC-V C and C++ cross-compiler. If you have finished installing it, you are able to use the Newlib cross-compiler or Linux cross-compiler to compile the program in your linux operating system.
NOTICE THAT: There are several ways to run linux kernel but each way requires the installation of riscv-gnu-toolchain. So it is `compulsory`.

### Prerequisites
They are some dependencies which must be installed in advanced so that you can installed the toolchain successfully.
```
$ sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
```

### Getting the source
Now supposed that you are in the /home/charles. Just run the following commands.
```
$ mkdir riscv
$ cd riscv
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/riscv-gnu-toolchain
```
After doing that, you need adjust the environment variable PATH and RISCV. `PATH` is used to determine the address of riscv cross compiler so that emualtor can use it. `RiSCV` is used in some scripts to determine the installation path.
```
export PATH=$PATH:/home/charles/riscv/riscv-gnu-toolchain/bin
export RISCV=/home/charles/riscv/riscv-gnu-toolchain
```
NOTICE THAT: The way above to set the PATH and RISCV is `temporary`, which means every time starting your Ubuntu, you need to set them again. There are several ways to set them permanently online. You can google them.

### Installation
```
$ cd riscv-gnu-toolchain
$ ./configure --prefix=/home/charles/riscv/riscv-gnu-toolchain
$ make newlib -j $(nproc)
$ make linux -j $(nproc)
```
The first `make newlib` aims to install the Newlib cross-compiler, the second `make linux` aims to install the linux cross-compiler. Now you can use these two riscv cross compiler. You can write a `HelloWorld.c` program in `riscv` folder and run:
```
$ riscv64-unknown-elf-gcc -o hello HelloWorld.c
or
$ riscv64-unknown-linux-gnu-gcc -o hello HelloWorld.c
```
Then you can get a new program called `hello` which can be run by qemu or spike to show `hello world`. You have installed the riscv-gnu-toolchain successfully so far. 

## The first way to run linux kernel -- freedom-u-sdk
Let's go back to `riscv` folder.
```
$ cd /home/charles/riscv
```

### Prerequisites
```
$ sudo apt-get install build-essential git autotools texinfo bison flex libgmp-dev libmpfr-dev libmpc-dev gawk libz-dev libssl-dev libglib2.0-dev libpixman-1-dev device-tree-compiler
```

### Getting the source and make it
```
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/freedom-u-sdk
$ cd freedom-u-sdk
$ make -j $(nproc)
```
After long time, you can see a folder called `work` in freedom-u-sdk folder. That is the result of command `make` and includes every modules related to run linux kernel.

### Running
If you want to run linux kernel with qemu
```
$ make qemu
```
If you want to run linux kernel with spike
```
$ make sim
```
NOTICE THAT: There may be some problems about running with spike. So if your terminal holds on sometime after using `make sim`, you should turn to use `make qemu`.

### Finishing
If your terminal shows
```
Welcome to buildroot
buildroot login:
```
Congradulations! You can use freedom-u-sdk to run the linux kernel. You could input the following username and password to log in.
```
buildroot login: root
Password: sifive
```
When you can see `#` after input the username and password, you can use your own linux operating system. Notice you are now in `/root` in the mini-OS, you can use `pwd` and `ls` commands to get familiar with the mini-OS. To close the mini-OS, just input `poweroff`.

## The second way to run linux kernel
Well, the first way is so simple and transparent that you are not able to know the whole process to build the mini-OS. However, the second way can tell you the entire process.

### Prerequisites
```
$ sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev git
```

### Getting the source code
Let's get back to `riscv` folder and install some required folders.
```
$ cd /home/charles/riscv
$ mkdir riscv64
$ cd riscv64
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/qemu
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/linux
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/riscv-pk
$ git clone --recursive https://github.com/Charles-J97/run-linux-with-riscv-emulators/tree/master/busybear-linux
```
After doing that, there will be four folders in riscv64 folder: qemu, linux, riscv-pk and busybear-linux.
* `qemu`: Its function is to build the qemu emulator to run the linux kernel.
* `linux`: It is just the source code of a linux kernel.
* `riscv-pk`: It is the Berkeley bootloader which can activate the linux kernel.
* `busybear-linux`: It aims to build the root filesystem for the mini-OS.
As we all know, there are three fundamental elements to run a linux kernel: linux kernel source code, bootloader and root filesystem. What we should do later is to build these three fundamental elements.

### Building
* qemu
```
cd qemu
git checkout v3.0.0
./configure --target-list=riscv64-softmmu
make -j $(nproc)
sudo make install
```

* linux
```
cd linux
git checkout v4.19-rc3
cp ../busybear-linux/conf/linux.config .config
make ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- olddefconfig
```
The first step to build linux for the RISC-V target is to checkout to a desired version and copy the default configuration from Busybear.
Next, enter the kernel configuration:
```
make ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- menuconfig
```
Then you will enter a block, you have nothing to do just exit and it will generate a invisible file called `.config`. Then compile the kernel
```
make ARCH=riscv CROSS_COMPILE=riscv64-unknown-linux-gnu- vmlinux -j $(nproc)
```
Finally you will get the vmlinux.

* riscv-pk
```
cd riscv-pk
mkdir build && cd build
../configure --enable-logo --host=riscv64-unknown-elf --with-payload=../../linux/vmlinux
make -j $(nproc)
```
Its function is to connect the bbl(Berkeley bootloader) with the vmlinux compiled just now. Then the vmlinux can be activated. However, we still need the root filesystem to run the vmlinux.

* busybear-linux
```
cd busybear-linux
make -j $(nproc)
```
There are some scripts in busybear-linux so you just need run these simple commands to build it.

### Running
```
cd /home/charles/riscv/riscv64
```
Now we have built all modules required to run linux kernel with qemu. Every time you want to run it, you just need to run the following commands.
```
sudo qemu-system-riscv64 -nographic -machine virt \
     -kernel riscv-pk/build/bbl -append "root=/dev/vda ro console=ttyS0" \
     -drive file=busybear-linux/busybear.bin,format=raw,id=hd0 \
     -device virtio-blk-device,drive=hd0
```
Then it will show
```
ucbvax login:
```
Well, the username is `root` and password is `busybear`. Input them and then you will get into your own mini-OS. To close it, just run `poweroff`.
