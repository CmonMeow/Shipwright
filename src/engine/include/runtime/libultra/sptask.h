#pragma once

#include "types.h"

/* Task Types */
#define M_NULTASK 0
#define M_GFXTASK 1
#define M_AUDTASK 2
#define M_VIDTASK 3
#define M_NJPEGTASK 4
#define M_HVQTASK 6
#define M_HVQMTASK 7

/* Task Flags  */
#define M_TASK_FLAG0 1
#define M_TASK_FLAG1 2

/* Task Flag Fields */
#define OS_TASK_YIELDED 0x0001
#define OS_TASK_DP_WAIT 0x0002
#define OS_TASK_LOADABLE 0x0004
#define OS_TASK_SP_ONLY 0x0008
#define OS_TASK_USR0 0x0010
#define OS_TASK_USR1 0x0020
#define OS_TASK_USR2 0x0040
#define OS_TASK_USR3 0x0080

#define OS_YIELD_DATA_SIZE 0xC00

typedef struct {
    /* 0x00 */ uint32_t type;
    /* 0x04 */ uint32_t flags;

    /* 0x08 */ uint64_t* ucode_boot;
    /* 0x0C */ uint32_t ucode_boot_size;

    /* 0x10 */ uint64_t* ucode;
    /* 0x14 */ uint32_t ucode_size;

    /* 0x18 */ uint64_t* ucode_data;
    /* 0x1C */ uint32_t ucode_data_size;

    /* 0x20 */ uint64_t* dram_stack;
    /* 0x24 */ uint32_t dram_stack_size;

    /* 0x28 */ uint64_t* output_buff;
    /* 0x2C */ uint64_t* output_buff_size;

    /* 0x30 */ uint64_t* data_ptr;
    /* 0x34 */ uint32_t data_size;

    /* 0x38 */ uint64_t* yield_data_ptr;
    /* 0x3C */ uint32_t yield_data_size;
} OSTask_t; // size = 0x40

typedef union {
    OSTask_t t;
    long long int force_structure_alignment;
} OSTask;

typedef uint32_t OSYieldResult;

#define osSpTaskStart(p) \
    osSpTaskLoad(p);     \
    osSpTaskStartGo(p);
