/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_fp.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_fp_emulation.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_misaligned_ldst.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi_utils/softfloat/internals.h>

int truly_illegal_insn(ulong insn, struct sbi_trap_regs *regs)
{
	struct sbi_trap_info trap;

	trap.epc = regs->mepc;
	trap.cause = CAUSE_ILLEGAL_INSTRUCTION;
	trap.tval = insn;
	trap.tval2 = 0;
	trap.tinst = 0;

	return sbi_trap_redirect(regs, &trap);
}

int system_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	int do_write, rs1_num = (insn >> 15) & 0x1f;
	ulong rs1_val = GET_RS1(insn, regs);
	int csr_num   = (u32)insn >> 20;
	ulong csr_val, new_csr_val;

	/* TODO: Ensure that we got CSR read/write instruction */

	if (sbi_emulate_csr_read(csr_num, regs, &csr_val))
		return truly_illegal_insn(insn, regs);

	do_write = rs1_num;
	switch (GET_RM(insn)) {
	case 1:
		new_csr_val = rs1_val;
		do_write    = 1;
		break;
	case 2:
		new_csr_val = csr_val | rs1_val;
		break;
	case 3:
		new_csr_val = csr_val & ~rs1_val;
		break;
	case 5:
		new_csr_val = rs1_num;
		do_write    = 1;
		break;
	case 6:
		new_csr_val = csr_val | rs1_num;
		break;
	case 7:
		new_csr_val = csr_val & ~rs1_num;
		break;
	default:
		return truly_illegal_insn(insn, regs);
	};

	if (do_write && sbi_emulate_csr_write(csr_num, regs, new_csr_val))
		return truly_illegal_insn(insn, regs);

	SET_RD(insn, regs, csr_val);

	regs->mepc += 4;

	return 0;
}

#ifdef SBI_ENABLE_FP_EMULATION
int fmadd_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	// if FPU is disabled, punt back to the OS
	if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
		return truly_illegal_insn(insn, regs);

	bool negA = (insn >> 3) & 1;
	bool negC = (insn >> 2) & 1;
	SETUP_STATIC_ROUNDING(insn);
	if (GET_PRECISION(insn) == PRECISION_S) {
		uint32_t rs1 = GET_F32_RS1(insn, regs) ^ (negA ? INT32_MIN : 0);
		uint32_t rs2 = GET_F32_RS2(insn, regs);
		uint32_t rs3 = GET_F32_RS3(insn, regs) ^ (negC ? INT32_MIN : 0);
		SET_F32_RD(insn, regs, softfloat_mulAddF32(rs1, rs2, rs3, 0).v);
	} else if (GET_PRECISION(insn) == PRECISION_D) {
		uint64_t rs1 = GET_F64_RS1(insn, regs) ^ (negA ? INT64_MIN : 0);
		uint64_t rs2 = GET_F64_RS2(insn, regs);
		uint64_t rs3 = GET_F64_RS3(insn, regs) ^ (negC ? INT64_MIN : 0);
		SET_F64_RD(insn, regs, softfloat_mulAddF64(rs1, rs2, rs3, 0).v);
	} else {
		return truly_illegal_insn(insn, regs);
	}

	regs->mepc += 4;

	return 0;
}

int fp_opcode_insn(ulong insn, struct sbi_trap_regs *regs);

#ifndef __riscv_flen
int float_store_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong tval2 = 0, tinst = 0; // TODO: Hypervisor
	uintptr_t addr = GET_RS1(insn, regs) + IMM_S(insn);
	struct sbi_trap_info uptrap = {};

	// if FPU is disabled, punt back to the OS
	if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
		return truly_illegal_insn(insn, regs);

	switch (insn & MASK_FUNCT3) {
	case INSN_MATCH_FSW & MASK_FUNCT3:
		punt_to_misaligned_handler(4, sbi_misaligned_store_handler);
		sbi_store_u32((void *)addr, GET_F32_RS2(insn, regs), &uptrap);
		if (uptrap.cause) {
			uptrap.epc = regs->mepc;
			return sbi_trap_redirect(regs, &uptrap);
		}
		break;

	case INSN_MATCH_FSD & MASK_FUNCT3:
		punt_to_misaligned_handler(sizeof(uintptr_t), sbi_misaligned_store_handler);
		sbi_store_u64((void *)addr, GET_F64_RS2(insn, regs), &uptrap);
		if (uptrap.cause) {
			uptrap.epc = regs->mepc;
			return sbi_trap_redirect(regs, &uptrap);
		}
		break;

	default:
		return truly_illegal_insn(insn, regs);
	}

	regs->mepc += 4;

	return 0;
}

int float_load_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong tval2 = 0, tinst = 0; // TODO: Hypervisor
	uintptr_t addr = GET_RS1(insn, regs) + IMM_I(insn);
	struct sbi_trap_info uptrap = {};

	// if FPU is disabled, punt back to the OS
	if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
		return truly_illegal_insn(insn, regs);

	switch (insn & MASK_FUNCT3) {
	case INSN_MATCH_FLW & MASK_FUNCT3:
		punt_to_misaligned_handler(4, sbi_misaligned_load_handler);
		SET_F32_RD(insn, regs, sbi_load_s32((void *)addr, &uptrap));
		break;

	case INSN_MATCH_FLD & MASK_FUNCT3:
		punt_to_misaligned_handler(sizeof(uintptr_t),
					   sbi_misaligned_load_handler);
		SET_F64_RD(insn, regs, sbi_load_u64((void *)addr, &uptrap));
		break;

	default:
		return truly_illegal_insn(insn, regs);
	}

	regs->mepc += 4;

	return 0;
}
#endif // !__riscv_flen
#endif // SBI_ENABLE_FP_EMULATION

int emulate_rvc(ulong insn, ulong tval2, ulong tinst,
			     struct sbi_trap_regs *regs)
{
#ifdef __riscv_compressed
	struct sbi_trap_info uptrap = {};
	// the only emulable RVC instructions are FP loads and stores.
#if !defined(__riscv_flen) && defined(SBI_ENABLE_FP_EMULATION)
	csr_write(CSR_MEPC, regs->mepc + 2);

	// if FPU is disabled, punt back to the OS
	if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
		return truly_illegal_insn(insn, regs);

	if ((insn & INSN_MASK_C_FLD) == INSN_MATCH_C_FLD) {
		uintptr_t addr = GET_RS1S(insn, regs) + RVC_LD_IMM(insn);
		if (unlikely(addr % sizeof(uintptr_t)))
			return sbi_misaligned_load_handler(addr, tval2, tinst, regs);
		SET_F64_RD(RVC_RS2S(insn) << SH_RD, regs,
			   sbi_load_u64((void *)addr, &uptrap));
	} else if ((insn & INSN_MASK_C_FLDSP) == INSN_MATCH_C_FLDSP) {
		uintptr_t addr = GET_SP(regs) + RVC_LDSP_IMM(insn);
		if (unlikely(addr % sizeof(uintptr_t)))
			return sbi_misaligned_load_handler(addr, tval2, tinst, regs);
		SET_F64_RD(insn, regs, sbi_load_u64((void *)addr, &uptrap));
	} else if ((insn & INSN_MASK_C_FSD) == INSN_MATCH_C_FSD) {
		uintptr_t addr = GET_RS1S(insn, regs) + RVC_LD_IMM(insn);
		if (unlikely(addr % sizeof(uintptr_t)))
			return sbi_misaligned_store_handler(addr, tval2, tinst, regs);
		sbi_store_u64((void *)addr,
			       GET_F64_RS2(RVC_RS2S(insn) << SH_RS2, regs),
			       &uptrap);
	} else if ((insn & INSN_MASK_C_FSDSP) == INSN_MATCH_C_FSDSP) {
		uintptr_t addr = GET_SP(regs) + RVC_SDSP_IMM(insn);
		if (unlikely(addr % sizeof(uintptr_t)))
			return sbi_misaligned_store_handler(addr, tval2, tinst, regs);
		sbi_store_u64((void *)addr,
			       GET_F64_RS2(RVC_RS2(insn) << SH_RS2, regs),
			       &uptrap);
	} else
#if __riscv_xlen == 32
		if ((insn & INSN_MASK_C_FLW) == INSN_MATCH_C_FLW) {
		uintptr_t addr = GET_RS1S(insn, regs) + RVC_LW_IMM(insn);
		if (unlikely(addr % 4))
			return sbi_misaligned_load_handler(addr, tval2, tinst, regs);
		SET_F32_RD(RVC_RS2S(insn) << SH_RD, regs,
			   sbi_load_s32((void *)addr, &uptrap));
	} else if ((insn & INSN_MASK_C_FLWSP) == INSN_MATCH_C_FLWSP) {
		uintptr_t addr = GET_SP(regs) + RVC_LWSP_IMM(insn);
		if (unlikely(addr % 4))
			return sbi_misaligned_load_handler(addr, tval2, tinst, regs);
		SET_F32_RD(insn, regs, load_int32_t((void *)addr, &uptrap));
	} else if ((insn & INSN_MASK_C_FSW) == INSN_MATCH_C_FSW) {
		uintptr_t addr = GET_RS1S(insn, regs) + RVC_LW_IMM(insn);
		if (unlikely(addr % 4))
			return sbi_misaligned_store_handler(addr, tval2, tinst, regs);
		sbi_store_u32((void *)addr,
			       GET_F32_RS2(RVC_RS2S(insn) << SH_RS2, regs),
			       &uptrap);
	} else if ((insn & INSN_MASK_C_FSWSP) == INSN_MATCH_C_FSWSP) {
		uintptr_t addr = GET_SP(regs) + RVC_SWSP_IMM(insn);
		if (unlikely(addr % 4))
			return sbi_misaligned_store_handler(addr, tval2, tinst, regs);
		sbi_store_u32((void *)addr,
			      GET_F32_RS2(RVC_RS2(insn) << SH_RS2, regs),
			      &uptrap);
	} else
#endif
#endif
#endif

		return truly_illegal_insn(insn, regs);

	if (uptrap.cause) {
		uptrap.epc = regs->mepc;
		return sbi_trap_redirect(regs, &uptrap);
	}

	regs->mepc += 2;

	return 0;
}

static illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
#if !defined(__riscv_flen) && defined(SBI_ENABLE_FP_EMULATION)
	float_load_opcode_insn,
#else
	truly_illegal_insn, /* 1 */
#endif
	truly_illegal_insn, /* 2 */
	truly_illegal_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
#if !defined(__riscv_flen) && defined(SBI_ENABLE_FP_EMULATION)
	float_store_opcode_insn,
#else
	truly_illegal_insn, /* 9 */
#endif
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	truly_illegal_insn, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
#ifdef SBI_ENABLE_FP_EMULATION
	fmadd_opcode_insn,
	fmadd_opcode_insn,
	fmadd_opcode_insn,
	fmadd_opcode_insn,
	fp_opcode_insn,
#else
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
#endif
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

int sbi_illegal_insn_handler(ulong insn, ulong tval2, ulong tinst,
			     struct sbi_trap_regs *regs)
{
	struct sbi_trap_info uptrap;

	/*
	 * We only deal with 32-bit (or longer) illegal instructions. If we
	 * see instruction is zero OR instruction is 16-bit then we fetch and
	 * check the instruction encoding using unprivilege access.
	 *
	 * The program counter (PC) in RISC-V world is always 2-byte aligned
	 * so handling only 32-bit (or longer) illegal instructions also help
	 * the case where MTVAL CSR contains instruction address for illegal
	 * instruction trap.
	 */

	sbi_pmu_ctr_incr_fw(SBI_PMU_FW_ILLEGAL_INSN);
	if (unlikely((insn & 3) != 3)) {
		insn = sbi_get_insn(regs->mepc, &uptrap);
		if (uptrap.cause) {
			uptrap.epc = regs->mepc;
			return sbi_trap_redirect(regs, &uptrap);
		}
		if ((insn & 3) != 3)
			return emulate_rvc(insn, tval2, tinst, regs);
	}

	return illegal_insn_table[(insn & 0x7c) >> 2](insn, regs);
}
