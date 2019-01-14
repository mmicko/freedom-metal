/* Copyright 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <mee/io.h>
#include <mee/drivers/sifive,clic0.h>

unsigned long long __mee_clic0_mtime_get (struct __mee_driver_sifive_clic0 *clic)
{
    __mee_io_u32 lo, hi;

    /* Guard against rollover when reading */
    do {
	hi = __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIME_OFFSET + 4));
	lo = __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIME_OFFSET));
    } while (__MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIME_OFFSET + 4)) != hi);

    return (((unsigned long long)hi) << 32) | lo;
}

int __mee_clic0_mtime_set (struct __mee_driver_sifive_clic0 *clic, unsigned long long time)
{   
    /* Per spec, the RISC-V MTIME/MTIMECMP registers are 64 bit,
     * and are NOT internally latched for multiword transfers.
     * Need to be careful about sequencing to avoid triggering
     * spurious interrupts: For that set the high word to a max
     * value first.
     */
    __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIMECMP_OFFSET + 4)) = 0xFFFFFFFF;
    __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIMECMP_OFFSET)) = (__mee_io_u32)time;
    __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base + MEE_CLIC_MTIMECMP_OFFSET + 4)) = (__mee_io_u32)(time >> 32);
    return 0;
}

void __mee_driver_sifive_clic0_init (struct mee_interrupt *controller)
{
    struct __mee_driver_sifive_clic0 *clic =
                              (struct __mee_driver_sifive_clic0 *)(controller);

    if ( !clic->init_done ) {
        struct mee_interrupt *intc = clic->interrupt_parent;

	/* Register its interrupts with with parent controller, aka sw and timerto its default isr */
        for (int i = 0; i < clic->num_interrupts; i++) {
            intc->vtable->interrupt_register(intc,
					     clic->interrupt_lines[i],
					     NULL, clic);
	}
	clic->init_done = 1;
    }	
}

int __mee_driver_sifive_clic0_register (struct mee_interrupt *controller,
                                        int id, mee_interrupt_handler_t isr,
                                        void *priv)
{
    int rc = -1;
    struct __mee_driver_sifive_clic0 *clic =
                              (struct __mee_driver_sifive_clic0 *)(controller);
    struct mee_interrupt *intc = clic->interrupt_parent;

    /* Register its interrupts with parent controller */
    if (intc) {
        rc = intc->vtable->interrupt_register(intc, id, isr, priv);
    }
    return rc;
}

int __mee_driver_sifive_clic0_enable (struct mee_interrupt *controller, int id)
{
    int rc = -1;
    struct __mee_driver_sifive_clic0 *clic =
                              (struct __mee_driver_sifive_clic0 *)(controller);

    if ( id ) {
        struct mee_interrupt *intc = clic->interrupt_parent;
        
        /* Enable its interrupts with parent controller */
        if (intc) {
            rc = intc->vtable->interrupt_enable(intc, id);
        }
    }
}

int __mee_driver_sifive_clic0_disable (struct mee_interrupt *controller, int id)
{
    int rc = -1;
    struct __mee_driver_sifive_clic0 *clic =
                              (struct __mee_driver_sifive_clic0 *)(controller);

    if ( id ) {
        struct mee_interrupt *intc = clic->interrupt_parent;
        
        /* Disable its interrupts with parent controller */
        if (intc) {
            rc = intc->vtable->interrupt_disable(intc, id);
        }
    }
}

int __mee_driver_sifive_clic0_command_request (struct mee_interrupt *controller,
                                               int command, void *data)
{
    int hartid;
    int rc = -1;
    struct __mee_driver_sifive_clic0 *clic =
                              (struct __mee_driver_sifive_clic0 *)(controller);

    switch (command) {
    case MEE_TIMER_MTIME_GET:
        if (data) {
	    *(unsigned long long *)data = __mee_clic0_mtime_get(clic);
            rc = 0;
        }
        break;
    case MEE_TIMER_MTIME_SET:
        if (data) {
	    __mee_clic0_mtime_set(clic, *(unsigned long long *)data);
            rc = 0;
        }
        break;
    case MEE_SOFTWARE_IPI_CLEAR:
	if (data) {
	    hartid = *(int *)data;
            __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base +
					       (hartid * 4))) = MEE_DISABLE;
            rc = 0;
        }
        break;
    case MEE_SOFTWARE_IPI_SET:
	if (data) {
	    hartid = *(int *)data;
            __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base +
					       (hartid * 4))) = MEE_ENABLE;
            rc = 0;
        }
        break;
    case MEE_SOFTWARE_MSIP_GET:
        rc = 0;
	if (data) {
	    hartid = *(int *)data;
            rc = __MEE_ACCESS_ONCE((__mee_io_u32 *)(clic->control_base +
						    (hartid * 4)));
        }
        break;
    default:
	break;
    }

    return rc;
}