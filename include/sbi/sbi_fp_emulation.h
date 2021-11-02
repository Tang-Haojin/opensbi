/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, The Regents of the University of California (Regents).
 *
 * Authors:
 *   Haojin Tang <tanghaojin@outlook.com>
 */

#ifndef __SBI_FP_EMULATION_H__
#define __SBI_FP_EMULATION_H__

#include <sbi/sbi_trap.h>

#define GET_PRECISION(insn) (((insn) >> 25) & 3)
#define GET_RM(insn) (((insn) >> 12) & 7)
#define PRECISION_S 0
#define PRECISION_D 1

register long tp asm("tp");

# define GET_F64_REG(insn, pos, regs) (*(int64_t*)((void*)((regs) + SBI_TRAP_REGS_last) + (SHIFT_RIGHT(insn, (pos)-3) & 0xf8)))
# define SET_F64_REG(insn, pos, regs, val) (GET_F64_REG(insn, pos, regs) = (val))
# define GET_F32_REG(insn, pos, regs) (*(int32_t*)&GET_F64_REG(insn, pos, regs))
# define SET_F32_REG(insn, pos, regs, val) (GET_F32_REG(insn, pos, regs) = (val))
# define GET_FCSR() ({ (int)tp & 0xFF; })
# define SET_FCSR(value) ({ asm volatile("add tp, x0, %0" :: "rI"((value) & 0xFF)); SET_FS_DIRTY(); })
# define GET_FRM() (GET_FCSR() >> 5)
# define SET_FRM(value) SET_FCSR(GET_FFLAGS() | ((value) << 5))
# define GET_FFLAGS() (GET_FCSR() & 0x1F)
# define SET_FFLAGS(value) SET_FCSR((GET_FRM() << 5) | ((value) & 0x1F))

# define SETUP_STATIC_ROUNDING(insn) ({ \
  tp &= 0xFF; \
  if (likely(((insn) & MASK_FUNCT3) == MASK_FUNCT3)) tp |= tp << 8; \
  else if (GET_RM(insn) > 4) return truly_illegal_insn(insn, regs); \
  else tp |= GET_RM(insn) << 13; \
  asm volatile ("":"+r"(tp)); })


# define softfloat_raiseFlags(which) ({ asm volatile ("or tp, tp, %0" :: "rI"(which)); })
# define softfloat_roundingMode ({ (int)tp >> 13; })
# define SET_FS_DIRTY() csr_read_set(CSR_MSTATUS, MSTATUS_FS)

#define GET_F32_RS1(insn, regs) (GET_F32_REG(insn, 15, regs))
#define GET_F32_RS2(insn, regs) (GET_F32_REG(insn, 20, regs))
#define GET_F32_RS3(insn, regs) (GET_F32_REG(insn, 27, regs))
#define GET_F64_RS1(insn, regs) (GET_F64_REG(insn, 15, regs))
#define GET_F64_RS2(insn, regs) (GET_F64_REG(insn, 20, regs))
#define GET_F64_RS3(insn, regs) (GET_F64_REG(insn, 27, regs))
#define SET_F32_RD(insn, regs, val) (SET_F32_REG(insn, 7, regs, val), SET_FS_DIRTY())
#define SET_F64_RD(insn, regs, val) (SET_F64_REG(insn, 7, regs, val), SET_FS_DIRTY())

#define GET_F32_RS2C(insn, regs) (GET_F32_REG(insn, 2, regs))
#define GET_F32_RS2S(insn, regs) (GET_F32_REG(RVC_RS2S(insn), 0, regs))
#define GET_F64_RS2C(insn, regs) (GET_F64_REG(insn, 2, regs))
#define GET_F64_RS2S(insn, regs) (GET_F64_REG(RVC_RS2S(insn), 0, regs))

#endif
