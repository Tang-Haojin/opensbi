#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Western Digital Corporation or its affiliates.
#

# Compiler pre-processor flags
platform-cppflags-y =

# C Compiler and assembler flags.
platform-cflags-y =
platform-asflags-y =

# Linker flags: additional libraries and object files that the platform
# code needs can be added here
platform-ldflags-y =

#
# Command for platform specific "make run"
# Useful for development and debugging on plaftform simulator (such as QEMU)
#
# platform-runcmd = your_platform_run.sh

#
# Platform RISC-V XLEN, ABI, ISA and Code Model configuration.
# These are optional parameters but platforms can optionaly provide it.
# Some of these are guessed based on GCC compiler capabilities
#
PLATFORM_RISCV_XLEN = 64
PLATFORM_RISCV_ABI = lp64
PLATFORM_RISCV_ISA = rv64imac
PLATFORM_RISCV_CODE_MODEL = medany

FW_PAYLOAD=y

FW_TEXT_START=0x80000000
ifeq ($(PLATFORM_RISCV_XLEN), 32)
# This needs to be 4MB aligned for 32-bit support
  FW_PAYLOAD_OFFSET=0x400000
else
# This needs to be 2MB aligned for 64-bit support
  FW_PAYLOAD_OFFSET=0x200000
endif
FW_PAYLOAD_ALIGN=0x1000
FW_PAYLOAD_PATH=$(HOME)/linux/arch/riscv/boot/Image
FW_PAYLOAD_FDT_ADDR=0x82200000