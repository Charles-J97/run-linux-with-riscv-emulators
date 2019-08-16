/*
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#if 0
#include <common.h>
#include <asm/io.h>
#include <asm/encoding.h>

#if __riscv_xlen == 64 || __riscv64
	volatile uint64_t* mtime = (volatile uint64_t*)0x0200bff8;
	volatile uint64_t* timecmp = (volatile uint64_t*)0x02004000;
#else
 	volatile uint32_t* mtime = (volatile uint32_t*)0x0200bff8;
	volatile uint32_t* timecmp = (volatile uint32_t*)0x02004000;
#endif

/*static uint64_t g_systick_increment = 0;*/

int timer_init(void)
{
#if 0
    g_systick_increment = CONFIG_SYS_CLK_FREQ / 4;

    if (g_systick_increment > 0)
    {
        if (mtime && timecmp)
        {
            *timecmp = *mtime + g_systick_increment;
        }
        set_csr(mie, MIP_MTIP);

        set_csr(mstatus, MSTATUS_MIE);

    }
#endif
	return 0;
}


/*
 * return difference between timer ticks and base
 */
ulong get_timer(ulong base)
{
 return 0;
}

/* delay x useconds AND preserve advance timestamp value */
void __udelay(unsigned long usec)
{

}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On RISC-V it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	return get_timer(0);
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On RISC-V it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{

	return CONFIG_SYS_HZ;
}

unsigned long timer_read_counter(void)
{
	return CONFIG_SYS_CLK_FREQ;
}
#endif
