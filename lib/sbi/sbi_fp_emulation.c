/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, The Regents of the University of California (Regents).
 *
 * Authors:
 *   Haojin Tang <tanghaojin@outlook.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_emulate_csr.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_fp_emulation.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_unpriv.h>
#include <sbi_utils/softfloat/internals.h>
#include <sbi_utils/softfloat/softfloat.h>
#include <sbi_utils/softfloat/softfloat_types.h>

#ifdef SBI_ENABLE_FP_EMULATION

#if defined(__riscv_flen) && __riscv_flen != 64
# error single-float only is not supported
#endif

int fp_opcode_insn(ulong insn, struct sbi_trap_regs *regs)
{
  asm (".pushsection .rodata\n"
       "fp_emulation_table:\n"
       "  .word emulate_fadd - fp_emulation_table\n"
       "  .word emulate_fsub - fp_emulation_table\n"
       "  .word emulate_fmul - fp_emulation_table\n"
       "  .word emulate_fdiv - fp_emulation_table\n"
       "  .word emulate_fsgnj - fp_emulation_table\n"
       "  .word emulate_fmin - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_ff - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fsqrt - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcmp - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_if - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fcvt_fi - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fmv_if - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .word emulate_fmv_fi - fp_emulation_table\n"
       "  .word truly_illegal_insn - fp_emulation_table\n"
       "  .popsection");

  int ret = 0;
  // if FPU is disabled, punt back to the OS
  if (unlikely((regs->mstatus & MSTATUS_FS) == 0))
    return truly_illegal_insn(insn, regs);

  extern uint32_t fp_emulation_table[];
  int32_t* pf = (void*)fp_emulation_table + ((insn >> 25) & 0x7c);
  illegal_insn_func f = (illegal_insn_func)((void *)fp_emulation_table + *pf);

  SETUP_STATIC_ROUNDING(insn);
  ret = f(insn, regs);
  regs->mepc += 4;
  return ret;
}

#define f32(x) ((float32_t){ .v = x })
#define f64(x) ((float64_t){ .v = x })

int emulate_any_fadd(ulong insn, struct sbi_trap_regs *regs, int32_t neg_b)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs) ^ neg_b;
    SET_F32_RD(insn, regs, f32_add(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs) ^ ((uint64_t)neg_b << 32);
    SET_F64_RD(insn, regs, f64_add(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fadd(ulong insn, struct sbi_trap_regs *regs)
{
  return emulate_any_fadd(insn, regs, 0);
}

int emulate_fsub(ulong insn, struct sbi_trap_regs *regs)
{
  return emulate_any_fadd(insn, regs, INT32_MIN);
}

int emulate_fmul(ulong insn, struct sbi_trap_regs *regs)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_mul(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_mul(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fdiv(ulong insn, struct sbi_trap_regs *regs)
{
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, f32_div(f32(rs1), f32(rs2)).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, f64_div(f64(rs1), f64(rs2)).v);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fsqrt(ulong insn, struct sbi_trap_regs *regs)
{
  if ((insn >> 20) & 0x1f)
	  return truly_illegal_insn(insn, regs);

  if (GET_PRECISION(insn) == PRECISION_S) {
    SET_F32_RD(insn, regs, f32_sqrt(f32(GET_F32_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    SET_F64_RD(insn, regs, f64_sqrt(f64(GET_F64_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fsgnj(ulong insn, struct sbi_trap_regs *regs)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(insn, regs);

  #define DO_FSGNJ(rs1, rs2, rm) ({ \
    typeof(rs1) rs1_sign = (rs1) >> (8*sizeof(rs1)-1); \
    typeof(rs1) rs2_sign = (rs2) >> (8*sizeof(rs1)-1); \
    rs1_sign &= (rm) >> 1; \
    rs1_sign ^= (rm) ^ rs2_sign; \
    ((rs1) << 1 >> 1) | (rs1_sign << (8*sizeof(rs1)-1)); })

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    SET_F32_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    SET_F64_RD(insn, regs, DO_FSGNJ(rs1, rs2, rm));
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fmin(ulong insn, struct sbi_trap_regs *regs)
{
  int rm = GET_RM(insn);
  if (rm >= 2)
    return truly_illegal_insn(insn, regs);

  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    uint32_t arg1 = rm ? rs2 : rs1;
    uint32_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f32_lt_quiet(f32(arg1), f32(arg2)) || isNaNF32UI(rs2);
    SET_F32_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    uint64_t arg1 = rm ? rs2 : rs1;
    uint64_t arg2 = rm ? rs1 : rs2;
    int use_rs1 = f64_lt_quiet(f64(arg1), f64(arg2)) || isNaNF64UI(rs2);
    SET_F64_RD(insn, regs, use_rs1 ? rs1 : rs2);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fcvt_ff(ulong insn, struct sbi_trap_regs *regs)
{
  int rs2_num = (insn >> 20) & 0x1f;
  if (GET_PRECISION(insn) == PRECISION_S) {
    if (rs2_num != 1)
      return truly_illegal_insn(insn, regs);
    SET_F32_RD(insn, regs, f64_to_f32(f64(GET_F64_RS1(insn, regs))).v);
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    if (rs2_num != 0)
      return truly_illegal_insn(insn, regs);
    SET_F64_RD(insn, regs, f32_to_f64(f32(GET_F32_RS1(insn, regs))).v);
  } else {
    return truly_illegal_insn(insn, regs);
  }

  return 0;
}

int emulate_fcvt_fi(ulong insn, struct sbi_trap_regs *regs)
{
  if (GET_PRECISION(insn) != PRECISION_S && GET_PRECISION(insn) != PRECISION_D)
    return truly_illegal_insn(insn, regs);

  int negative = 0;
  uint64_t uint_val = GET_RS1(insn, regs);

  switch ((insn >> 20) & 0x1f)
  {
    case 0: // int32
      negative = (int32_t)uint_val < 0;
      uint_val = (uint32_t)(negative ? -uint_val : uint_val);
      break;
    case 1: // uint32
      uint_val = (uint32_t)uint_val;
      break;
#if __riscv_xlen == 64
    case 2: // int64
      negative = (int64_t)uint_val < 0;
      uint_val = negative ? -uint_val : uint_val;
    case 3: // uint64
      break;
#endif
    default:
      return truly_illegal_insn(insn, regs);
  }

  uint64_t float64 = ui64_to_f64(uint_val).v;
  if (negative)
    float64 ^= INT64_MIN;

  if (GET_PRECISION(insn) == PRECISION_S)
    SET_F32_RD(insn, regs, f64_to_f32(f64(float64)).v);
  else
	  SET_F64_RD(insn, regs, float64);

  return 0;
}

int emulate_fcvt_if(ulong insn, struct sbi_trap_regs *regs)
{
  int rs2_num = (insn >> 20) & 0x1f;
#if __riscv_xlen == 64
  if (rs2_num >= 4)
    return truly_illegal_insn(insn, regs);
#else
  if (rs2_num >= 2)
    return truly_illegal_insn(insn, regs);
#endif

  int64_t float64;
  if (GET_PRECISION(insn) == PRECISION_S)
    float64 = f32_to_f64(f32(GET_F32_RS1(insn, regs))).v;
  else if (GET_PRECISION(insn) == PRECISION_D)
    float64 = GET_F64_RS1(insn, regs);
  else
    return truly_illegal_insn(insn, regs);

  int negative = 0;
  if (float64 < 0) {
    negative = 1;
    float64 ^= INT64_MIN;
  }
  uint64_t uint_val = f64_to_ui64(f64(float64), softfloat_roundingMode, true);
  uint64_t result, limit, limit_result;

  switch (rs2_num)
  {
    case 0: // int32
      if (negative) {
        result = (int32_t)-uint_val;
        limit_result = limit = (uint32_t)INT32_MIN;
      } else {
        result = (int32_t)uint_val;
        limit_result = limit = INT32_MAX;
      }
      break;

    case 1: // uint32
      limit = limit_result = UINT32_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint32_t)uint_val;
      break;

    case 2: // int32
      if (negative) {
        result = (int64_t)-uint_val;
        limit_result = limit = (uint64_t)INT64_MIN;
      } else {
        result = (int64_t)uint_val;
        limit_result = limit = INT64_MAX;
      }
      break;

    case 3: // uint64
      limit = limit_result = UINT64_MAX;
      if (negative)
        result = limit = 0;
      else
        result = (uint64_t)uint_val;
      break;

    default:
      __builtin_unreachable();
  }

  if (uint_val > limit) {
    result = limit_result;
    softfloat_raiseFlags(softfloat_flag_invalid);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);

  return 0;
}

int emulate_fcmp(ulong insn, struct sbi_trap_regs *regs)
{
  int rm = GET_RM(insn);
  if (rm >= 3)
    return truly_illegal_insn(insn, regs);

  uintptr_t result;
  if (GET_PRECISION(insn) == PRECISION_S) {
    uint32_t rs1 = GET_F32_RS1(insn, regs);
    uint32_t rs2 = GET_F32_RS2(insn, regs);
    if (rm != 1)
      result = f32_eq(f32(rs1), f32(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f32_lt(f32(rs1), f32(rs2));
    goto success;
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    uint64_t rs1 = GET_F64_RS1(insn, regs);
    uint64_t rs2 = GET_F64_RS2(insn, regs);
    if (rm != 1)
      result = f64_eq(f64(rs1), f64(rs2));
    if (rm == 1 || (rm == 0 && !result))
      result = f64_lt(f64(rs1), f64(rs2));
    goto success;
  }
  return truly_illegal_insn(insn, regs);
success:
  SET_FS_DIRTY();
  SET_RD(insn, regs, result);

  return 0;
}

int emulate_fmv_if(ulong insn, struct sbi_trap_regs *regs)
{
  uintptr_t result;
  if ((insn >> 20) & 0x1f)
    return truly_illegal_insn(insn, regs);

  if (GET_PRECISION(insn) == PRECISION_S) {
    result = GET_F32_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_W): break;
      case GET_RM(MATCH_FCLASS_S): result = f32_classify(f32(result)); break;
      default: return truly_illegal_insn(insn, regs);
    }
  } else if (GET_PRECISION(insn) == PRECISION_D) {
    result = GET_F64_RS1(insn, regs);
    switch (GET_RM(insn)) {
      case GET_RM(MATCH_FMV_X_D): break;
      case GET_RM(MATCH_FCLASS_D): result = f64_classify(f64(result)); break;
      default: return truly_illegal_insn(insn, regs);
    }
  } else {
    return truly_illegal_insn(insn, regs);
  }

  SET_FS_DIRTY();
  SET_RD(insn, regs, result);

  return 0;
}

int emulate_fmv_fi(ulong insn, struct sbi_trap_regs *regs)
{
  uintptr_t rs1 = GET_RS1(insn, regs);

  if ((insn & MASK_FMV_W_X) == MATCH_FMV_W_X)
    SET_F32_RD(insn, regs, rs1);
#if __riscv_xlen == 64
  else if ((insn & MASK_FMV_D_X) == MATCH_FMV_D_X)
    SET_F64_RD(insn, regs, rs1);
#endif
  else
	  return truly_illegal_insn(insn, regs);

  return 0;
}

int emulate_fmadd(ulong insn, struct sbi_trap_regs *regs)
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

  return 0;
}

#endif
