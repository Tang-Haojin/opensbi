/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_hartmask.h>

/*
 * Include these files as needed.
 * See config.mk PLATFORM_xxx configuration parameters.
 */
#include <libfdt.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/irqchip/fdt_irqchip.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/ipi/fdt_ipi.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/aclint_mtimer.h>

static u32 generic_hart_index2id[SBI_HARTMASK_MAX_BITS] = { 0 };

static int yuquan_early_init(bool cold_boot)
{
	return 0;
}

static int yuquan_final_init(bool cold_boot)
{
	return 0;
}

const struct sbi_platform_operations platform_ops = {
	.early_init   = yuquan_early_init,
	.final_init   = yuquan_final_init,
	.console_init = fdt_serial_init,
	.irqchip_init = fdt_irqchip_init,
	.irqchip_exit = fdt_irqchip_exit,
	.ipi_init     = fdt_ipi_init,
	.ipi_exit     = fdt_ipi_exit,
	.timer_init   = fdt_timer_init,
	.timer_exit   = fdt_timer_exit
};

const struct sbi_platform platform = {
	.opensbi_version   = OPENSBI_VERSION,
	.platform_version  = SBI_PLATFORM_VERSION(0x0, 0x00),
	.name              = "YSYX3",
	.features          = SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count        = 1,
	.hart_index2id     = generic_hart_index2id,
	.hart_stack_size   = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr = (unsigned long)&platform_ops
};
