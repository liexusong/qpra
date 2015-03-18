/*
 * core/cpu/cpu.c -- Emulator CPU functions.
 *
 * The CPU functions, notably the instructions, are implemented here.
 *
 */

#include <string.h>
#include <stdlib.h>
#include "core/cpu/cpu.h"
#include "core/mmu/mmu.h"
#include "log.h"

static char *instrnam[NUM_INSTRS] = {
    "nop", /* 00 */
    "int", /* 01 */
    "rts", /* 02 */
    "rti", /* 03 */
    "jp",  /* 04 */
    "cl",  /* 05 */
    "jz",  /* 06 */
    "cz",  /* 07 */
    "jc",  /* 08 */
    "cc",  /* 09 */
    "jo",  /* 0a */
    "co",  /* 0b */
    "jn",  /* 0c */
    "cn",  /* 0d */
    "not", /* 0e */
    "inc", /* 0f */
    "dec", /* 10 */
    "ind", /* 11 */
    "ded", /* 12 */
    "mv",  /* 13 */
    "cmp", /* 14 */
    "tst", /* 15 */
    "add", /* 16 */
    "sub", /* 17 */
    "mul", /* 18 */
    "div", /* 19 */
    "lsl", /* 1a */
    "lsr", /* 1b */
    "asr", /* 1c */
    "and", /* 1d */
    "or",  /* 1e */
    "xor"  /* 1f */
};

int core_cpu_init(struct core_cpu **pcpu, struct core_mmu *mmu)
{
    struct core_cpu *cpu;
    
    *pcpu = NULL;
    *pcpu = malloc(sizeof(struct core_cpu));
    if(*pcpu == NULL) {
        LOGE("Could not allocate cpu core; exiting");
        return 0;
    }
    cpu = *pcpu;

    cpu->mmu = mmu;
    memset(cpu->r, 0, sizeof(cpu->r));
    cpu->i_cycles = 0;
    cpu->i = malloc(sizeof(struct core_instr));
    if(cpu->i == NULL) {
        LOGE("Could not allocate cpu instruction; exiting");
        return 0;
    }
    memset(cpu->i, 0, sizeof(*cpu->i));

    core_cpu_ops[0x04] = core_cpu_i_op_jp;
    core_cpu_ops[0x05] = core_cpu_i_op_cl;
    core_cpu_ops[0x06] = core_cpu_i_op_jz;
    core_cpu_ops[0x07] = core_cpu_i_op_cz;
    core_cpu_ops[0x08] = core_cpu_i_op_jc;
    core_cpu_ops[0x09] = core_cpu_i_op_cc;
    core_cpu_ops[0x0a] = core_cpu_i_op_jo;
    core_cpu_ops[0x0b] = core_cpu_i_op_co;
    core_cpu_ops[0x0c] = core_cpu_i_op_jn;
    core_cpu_ops[0x0d] = core_cpu_i_op_cn;
    core_cpu_ops[0x0e] = core_cpu_i_op_not;
    core_cpu_ops[0x0f] = core_cpu_i_op_inc;
    core_cpu_ops[0x10] = core_cpu_i_op_dec;
    core_cpu_ops[0x11] = core_cpu_i_op_ind;
    core_cpu_ops[0x12] = core_cpu_i_op_ded;
    core_cpu_ops[0x13] = core_cpu_i_op_mv;
    core_cpu_ops[0x14] = core_cpu_i_op_cmp;
    core_cpu_ops[0x15] = core_cpu_i_op_tst;
    core_cpu_ops[0x16] = core_cpu_i_op_add;
    core_cpu_ops[0x17] = core_cpu_i_op_sub;
    core_cpu_ops[0x18] = core_cpu_i_op_mul;
    core_cpu_ops[0x19] = core_cpu_i_op_div;
    core_cpu_ops[0x1a] = core_cpu_i_op_lsl;
    core_cpu_ops[0x1b] = core_cpu_i_op_lsr;
    core_cpu_ops[0x1c] = core_cpu_i_op_asr;
    core_cpu_ops[0x1d] = core_cpu_i_op_and;
    core_cpu_ops[0x1e] = core_cpu_i_op_or;
    core_cpu_ops[0x1f] = core_cpu_i_op_not;

    return 1;
}

void core_cpu_destroy(struct core_cpu *cpu)
{
    free(cpu);
    cpu = NULL;
}


/*
 * Execute one cycle of the current instruction.
 *
 * Each cycle step is a big state machine, with different addressing modes
 * governing the reading of operands (if any), the execution of the instruction,
 * and the writing of the result (if any).
 * 
 * Instructions are thus mostly shielded from these details, and can just focus
 * on the execution steps, rather than how to access the operands.
 */
void core_cpu_i_cycle(struct core_cpu *cpu)
{
    static struct core_instr_params p;
    static void (*i)(struct core_cpu *, struct core_instr_params *);
    int *c = &cpu->i_cycles;
    int done = 0;

    if(*c == 0) {
        memset(&p, 0, sizeof(p));
        p.p = cpu->r[R_P];
        p.s = cpu->r[R_S];
        p.f = cpu->r[R_F];

        cpu->i->ib0 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
        cpu->i->ib1 = core_mmu_readb(cpu->mmu, cpu->r[R_P] + 1);
        i = core_cpu_ops[INSTR_OP(cpu->i)];
        *c += 1;
        cpu->r[R_P] += 2;
    } else if(*c == 1) {
        /* Nothing else to fetch. */
        if(instr_is_void(cpu->i)) {
            cpu->r[R_P] -= 1;
            i(cpu, &p);
            if(!instr_has_spderef(cpu->i))
                done = 1;
        /* Nothing else to fetch. */
        } else if(instr_dr_only(cpu->i)) {
            p.op1 = cpu->r[INSTR_RX(cpu->i)];
            p.op2 = cpu->r[INSTR_RY(cpu->i)];
            i(cpu, &p);
            if(!instr_has_spderef(cpu->i))
                done = 1;
        /* Fetch data byte/word after instruction. */
        } else if(instr_has_data(cpu->i)) {
            cpu->i->db0 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
            cpu->r[R_P] += 1;
            if(instr_has_dw(cpu->i)) {
                cpu->i->db1 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
                cpu->r[R_P] += 1;
            }
            if(instr_is_op1data(cpu->i)) {
                p.op1 = (INSTR_AM(cpu->i) == AM_DB) ?
                    INSTR_D8(cpu->i) : INSTR_D16(cpu->i);
            } else if(instr_is_op2data(cpu->i)) {
                p.op1 = cpu->r[INSTR_RX(cpu->i)];
                p.op2 = (INSTR_AM(cpu->i) == AM_DR_DB) ?
                    INSTR_D8(cpu->i) : INSTR_D16(cpu->i);
            } /* else: data = ptr, fetch val from mem next cycle. */
        /* Fetch memory operand from source register. */
        } else if(instr_is_srcptr(cpu->i)) {
            if(instr_is_1op(cpu->i)) {
                p.op1 = (INSTR_OPSZ(cpu->i) == OP_16) ? 
                    core_mmu_readw(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]) :
                    core_mmu_readb(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]);
            } else {
                p.op1 = cpu->r[INSTR_RX(cpu->i)];
                p.op2 = (INSTR_OPSZ(cpu->i) == OP_16) ? 
                    core_mmu_readw(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]) :
                    core_mmu_readb(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]);
            }
        } else {
            LOGE("core.cpu: invalid state reached (cycle 2)");
        }
    } else if(*c == 2) {
        if(instr_is_void(cpu->i) || instr_dr_only(cpu->i)) {
            /* TODO: load memory operands when necessary. */.
            i(cpu, &p);
        } else if(instr_has_data(cpu->i)) {
            /* Fetch memory operand for pointer. */
            if(instr_is_srcptr(cpu->i)) {
                if(instr_is_1op(cpu->i))
                    p.op1 = (INSTR_OPSZ(cpu->i) == OP_16) ?
                        core_mmu_readw(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]) :
                        core_mmu_readb(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]);
                else
                    p.op1 = cpu->r[INSTR_RX(cpu->i)];
                    p.op2 = (INSTR_OPSZ(cpu->i) == OP_16) ?
                        core_mmu_readw(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]) :
                        core_mmu_readb(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]);
            /* Operate directly on data; nothing further to fetch. */
            } else {
                i(cpu, &p);
            }
        } else if(instr_is_srcptr(cpu->i)) {
            i(cpu, &p);
        } else {
            LOGE("core.cpu: invalid state reached (cycle 3)");
        }
    } else if(*c == 3) {
        if(instr_is_void(cpu->i) || instr_dr_only(cpu->i)) {
            /* TODO: load memory operands when necessary. */
            i(cpu, &p);
        } else if(instr_has_data(cpu->i)) {
            if(instr_is_srcptr(cpu->i)) {
                i(cpu, &p);
            } else if(instr_is_dstptr(cpu->i)) {
                (INSTR_OPSZ(cpu->i) == OP_16) ?
                    core_mmu_writew(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]) :
                    core_mmu_writeb(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]);
            } else {
                cpu->r[INSTR_RX(cpu->i)] = p.op1;
                done = 1;
            }
        } else if(instr_is_srcptr(cpu->i)) {
            if(instr_is_dstptr(cpu->i)) {
                (INSTR_OPSZ(cpu->i) == OP_16) ?
                    core_mmu_writew(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]) :
                    core_mmu_writeb(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]);
            } else {
                cpu->r[INSTR_RX(cpu->i)] = p.op1;
                done = 1;
            }
        } else {
            LOGE("core.cpu: reached error state (cycle 4)");
        }
    } else if(*c == 4) {
        if(instr_is_void(cpu->i) || instr_dr_only(cpu->i)) {
            /* TODO: load memory operands when necessary. */.
            i(cpu, &p);
        } else if(instr_has_data(cpu->i)) {
            if(instr_is_srcptr(cpu->i)) {
                if(instr_is_dstptr(cpu->i)) {
                    (INSTR_OP_SZ(cpu->i) == OP_16) ?
                        core_mmu_writew(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]) :
                        core_mmu_writeb(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]);
                } else {
                    cpu->r[INSTR_RX(cpu->i)] = p.op1;
                    done = 1;
                }
            } else if(instr_is_dstptr(cpu->i)) {
                done = 1;
            }
        } else if(instr_is_srcptr(cpu->i)) {
            if(instr_is_dstptr(cpu->i))
                done = 1;
        } else {
            LOGE("core.cpu: reached error state (cycle 5)");
        }
    } else if(*c == 5) {
        if(0) {
        } else if(instr_has_data(cpu->i)) {
            if(instr_is_srcptr(cpu->i) && instr_is_dstptr(cpu->i))
                done = 1;
        } else {
            LOGE("core.cpu: reached error state (cycle 6)");
        }
    }

    *c = done ? 0 : (*c + 1);
}

#if 0
/* Execute one cycle of the current instruction. */
void core_cpu_i_cycle(struct core_cpu *cpu)
{
    if(cpu->i_cycles == 0) {
        /* T1: opcode fetch */
        cpu->i->ib0 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
        cpu->i->ib1 = core_mmu_readb(cpu->mmu, cpu->r[R_P] + 1);
        ++cpu->i_cycles;
        cpu->r[R_P] += 2;
    } else {
        /* T2+: opcode decode + execute */
        /* Void-mode ops: no further work to do, can jump to execution. */
        if(INSTR_OP(cpu->i) < 4) {
            switch((enum core_instr_name) INSTR_OP(cpu->i)) {
                case OP_NOP:
                    core_cpu_i_op_nop(cpu);
                    break;
                case OP_INT:
                    core_cpu_i_op_int(cpu);
                    break;
                case OP_RTI:
                    core_cpu_i_op_rti(cpu);
                    break;
                case OP_RTS:
                    core_cpu_i_op_rts(cpu);
                    break;
            }
        /* Other ops: *maybe* fetch op1, execute, *maybe* store op2. */
        } else {
            enum core_mode_name mode = (enum core_mode_name) INSTR_AM(cpu->i);
            struct core_instr_params params;
            
            params.size = INSTR_OPSZ(cpu->i);
            params.start_cycle = 1;
            params.p = &cpu->r[R_P];
            params.s = &cpu->r[R_S];
            params.f = &cpu->r[R_F];

            switch(mode) {
                case AM_DR:
                case AM_IR:
                case AM_DR_DR:
                case AM_DR_IR:
                case AM_IR_DR:
                    /* Fetch operand 1 */
                    if(mode == AM_DR || mode == AM_DR_DR || mode == AM_DR_IR) {
                        params.op1.op16 = &cpu->r[INSTR_RX(cpu->i)];
                    } else if(mode == AM_IR || mode == AM_IR_DR) {
                        params.op1.op16 =
                            core_mmu_getwp(cpu->mmu, cpu->r[INSTR_RX(cpu->i)]);
                    }
                    /* Fetch operand 2 */
                    if(mode == AM_DR_DR || mode == AM_IR_DR) {
                        params.op2.op16 = &cpu->r[INSTR_RY(cpu->i)];
                    } else { /* AM_DR_IR */
                        params.op2.op16 =
                            core_mmu_getwp(cpu->mmu, cpu->r[INSTR_RY(cpu->i)]);
                    }
                    core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                    break;
                case AM_DB:
                case AM_IB:
                case AM_DR_DB:
                case AM_DR_IB:
                case AM_IB_DR:
                    if(cpu->i_cycles == 1) {
                        cpu->i->db0 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
                        cpu->r[R_P] += 1;
                        cpu->i_cycles += 1;
                    } else if(cpu->i_cycles == 2) {
                        /* Fetch operand 1 */
                        if(mode == AM_DR_DB || mode == AM_DR_IB) {
                            params.op1.op16 = &cpu->r[INSTR_RX(cpu->i)];
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        } else if(mode == AM_IB || mode == AM_IB_DR) {
                            params.op1.op16 =
                                core_mmu_getwp(cpu->mmu, cpu->r[R_P] + cpu->i->db0);
                            cpu->i_cycles += 1;
                        } else /* if(mode == AM_DB) */ {
                            params.op1.op8 = &cpu->i->db0;
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        }
                    } else if(cpu->i_cycles == 3) {
                        if(mode == AM_IB || mode == AM_IB_DR) {
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        } else if(mode == ) {
                        /* Fetch operand 2 */
                        } else if(mode == AM_DR_DB) {
                            params.op2.op8 = &cpu->i->db0;
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        } else if(mode == AM_DR_IB) {
                            params.op2.op16 =
                                core_mmu_getwp(cpu->mmu, cpu->r[R_P] + cpu->i->db0);
                            cpu->i_cycles += 1;
                        } else if(mode == AM_IB_DR) {
                            params.op2.op16 = &cpu->r[INSTR_RY(cpu->i)];
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        }
                    }
                    break;
                case AM_DW:
                case AM_IW:
                case AM_DR_DW:
                case AM_DR_IW:
                case AM_IW_DR:
                    if(cpu->i_cycles == 1) {
                        cpu->i->db0 = core_mmu_readb(cpu->mmu, cpu->r[R_P]);
                        cpu->i->db1 = core_mmu_readb(cpu->mmu, cpu->r[R_P] + 1);
                        cpu->r[R_P] += 2;
                        cpu->i_cycles += 1;
                        LOGD("data cycle");
                    } else {
                        /* Fetch operand 1 */
                        if(mode == AM_DR_DW || mode == AM_DR_IW) {
                            params.op1.op16 = &cpu->r[INSTR_RX(cpu->i)];
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        } else if(mode == AM_IW || mode == AM_IW_DR) {
                            params.op1.op16 =
                                core_mmu_getwp(cpu->mmu, *(uint16_t *)&cpu->i->db0);
                            cpu->i_cycles += 1;
                        } else /* if(mode == AM_IW_DR) */ {
                            params.op1.op8 = &cpu->i->db0;
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        }
                        /* Fetch operand 2 */
                        if(mode == AM_DR_DB) {
                            params.op2.op8 = &cpu->i->db0;
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        } else if(mode == AM_DR_IB) {
                            params.op2.op16 =
                                core_mmu_getwp(cpu->mmu, cpu->r[R_P] + cpu->i->db0);
                            cpu->i_cycles += 1;
                        } else if(mode == AM_IB_DR) {
                            params.op2.op16 = &cpu->r[INSTR_RY(cpu->i)];
                            core_cpu_ops[INSTR_OP(cpu->i)](cpu, &params);
                        }
                    }
                    break;
            }
        }
    }
}
#endif

/*
 * core_cpu_i_instr
 *
 * Execute all the cycles for the current instruction.
 */
void core_cpu_i_instr(struct core_cpu *cpu)
{
    /* Elapsed cycles are also recorded in the core_cpu structure, but when
     * we want to log the elapsed cycles after the instruction is complete,
     * that counter has been reset. So we record it here too.
     */
    int cycles = 0;
    uint16_t pc = cpu->r[R_P];

    do {
        core_cpu_i_cycle(cpu);
        ++cycles;
    } while(cpu->i_cycles > 0);
    LOGD("core.cpu: %04x: %s (%d cycles)", pc, instrnam[INSTR_OP(cpu->i)], cycles);
}

/*
 ******************************************************************************
 * Instruction implementations
 ******************************************************************************
 */

/*
 * core_cpu_i_op_nop
 *
 * NOP instruction implementation.
 */
void core_cpu_i_op_nop(struct core_cpu *cpu)
{
    --cpu->r[R_P];
    cpu->i_cycles = 0;
    LOGD("NOP cycle 1");
}

/*
 * core_cpu_i_op_int
 *
 * INT instruction implementation.
 */
void core_cpu_i_op_int(struct core_cpu *cpu)
{
    if(cpu->i_cycles == 1) {
        core_mmu_writew(cpu->mmu, cpu->r[R_S], cpu->r[R_P]);
        ++cpu->i_cycles;
    } else if(cpu->i_cycles == 2) {
        cpu->r[R_S] -= 2;
        cpu->r[R_F] |= FLAG_I;
        ++cpu->i_cycles;
    } else {
        cpu->r[R_P] = core_mmu_readw(cpu->mmu, 0xfffe);
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_rti
 *
 * RTI instruction implementation.
 */
void core_cpu_i_op_rti(struct core_cpu *cpu)
{
    if(cpu->i_cycles == 1) {
        cpu->r[R_F] = core_mmu_readw(cpu->mmu, cpu->r[R_S]);
        ++cpu->i_cycles;
    } else if(cpu->i_cycles == 2) {
        cpu->r[R_S] += 2;
        ++cpu->i_cycles;
    } else if(cpu->i_cycles == 3) {
        cpu->r[R_P] = core_mmu_readw(cpu->mmu, cpu->r[R_S]);
        ++cpu->i_cycles;
    } else {
        cpu->r[R_S] += 2;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_rts
 *
 * RTS instruction implementation.
 */
void core_cpu_i_op_rts(struct core_cpu *cpu)
{
    if(cpu->i_cycles == 1) {
        cpu->r[R_P] = core_mmu_readw(cpu->mmu, cpu->r[R_S]);
        ++cpu->i_cycles;
        LOGD("RTS cycle 1");
    } else {
        cpu->r[R_S] += 2;
        cpu->i_cycles = 0;
        LOGD("RTS cycle 2");
    }
}

void core_cpu_i__jump(struct core_cpu *cpu,
                      struct core_instr_params *params,
                      int flag)
{
    static uint16_t v;
    v = *params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle) {
        v = core_mmu_readw(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        switch(flag) {
        case FLAG_C:
        case FLAG_Z:
        case FLAG_O:
        case FLAG_N:
            if(*params->f & flag)
                *params->p = v;
            break;
        default:
            *params->p = v;
            break;
        }
        cpu->i_cycles = 0;
    }
}

void core_cpu_i__call(struct core_cpu *cpu,
                      struct core_instr_params *params,
                      int flag)
{
    static uint16_t v;
    v = *params->op1.op16;
    if(cpu->i_cycles == params->start_cycle) {
        core_mmu_writew(cpu->mmu, cpu->r[R_S], cpu->r[R_P]);
        ++cpu->i_cycles;
    } else if(cpu->i_cycles == 2) {
        cpu->r[R_S] -= 2;
        /* Indirect reg/byte/word: the memory access uses an extra cycle. */
        if(INSTR_AM(cpu->i) & 1) {
            v = core_mmu_readw(cpu->mmu, *params->op1.op16);
            ++cpu->i_cycles;
        } else {
            switch(flag) {
            case FLAG_Z:
            case FLAG_C:
            case FLAG_O:
            case FLAG_N:
                if(*params->f & flag)
                    *params->p = v;
                break;
            default:
                *params->p = v;
                break;
            }
            cpu->i_cycles = 0;
        }
    } else if(cpu->i_cycles == 3) {
        *params->p = v;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_jp
 *
 * JP instruction implementation.
 */
void core_cpu_i_op_jp(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__jump(cpu, params, 0);
}

/*
 * core_cpu_i_op_cl
 *
 * CL instruction implementation.
 */
void core_cpu_i_op_cl(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__call(cpu, params, 0);
}

/*
 * core_cpu_i_op_jz
 *
 * JZ instruction implementation.
 */
void core_cpu_i_op_jz(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__jump(cpu, params, FLAG_Z);
}

/*
 * core_cpu_i_op_cz
 *
 * CZ instruction implementation.
 */
void core_cpu_i_op_cz(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__call(cpu, params, FLAG_Z);
}

/*
 * core_cpu_i_op_jc
 *
 * JC instruction implementation.
 */
void core_cpu_i_op_jc(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__jump(cpu, params, FLAG_C);
}

/*
 * core_cpu_i_op_cc
 *
 * CC instruction implementation.
 */
void core_cpu_i_op_cc(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__call(cpu, params, FLAG_C);
}

/*
 * core_cpu_i_op_jo
 *
 * JO instruction implementation.
 */
void core_cpu_i_op_jo(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__jump(cpu, params, FLAG_O);
}

/*
 * core_cpu_i_op_co
 *
 * CO instruction implementation.
 */
void core_cpu_i_op_co(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__call(cpu, params, FLAG_O);
}

/*
 * core_cpu_i_op_jn
 *
 * JN instruction implementation.
 */
void core_cpu_i_op_jn(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__jump(cpu, params, FLAG_N);
}

/*
 * core_cpu_i_op_cn
 *
 * CN instruction implementation.
 */
void core_cpu_i_op_cn(struct core_cpu *cpu, struct core_instr_params *params)
{
    core_cpu_i__call(cpu, params, FLAG_N);
}

/*
 * core_cpu_i_op_not
 *
 * NOT instruction implementation.
 */
void core_cpu_i_op_not(struct core_cpu *cpu, struct core_instr_params *params)
{
    static uint16_t *v;
    v= params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle + 1) {
        v = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
        LOGD("NOT cycle 1");
    } else {
        *v = ~*v;
        cpu->i_cycles = 0;
        LOGD("NOT cycle 2");
    }
}

/*
 * core_cpu_i_op_inc
 *
 * INC instruction implementation.
 */
void core_cpu_i_op_inc(struct core_cpu *cpu, struct core_instr_params *params)
{
    static uint16_t *v;
    v= params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle) {
        v = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        *v += 1;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_dec
 *
 * DEC instruction implementation.
 */
void core_cpu_i_op_dec(struct core_cpu *cpu, struct core_instr_params *params)
{
    static uint16_t *v;
    v= params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle) {
        v = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        *v -= 1;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_ind
 *
 * IND instruction implementation.
 */
void core_cpu_i_op_ind(struct core_cpu *cpu, struct core_instr_params *params)
{
    static uint16_t *v;
    v= params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle) {
        v = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        *v += 2;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_ded
 *
 * DED instruction implementation.
 */
void core_cpu_i_op_ded(struct core_cpu *cpu, struct core_instr_params *params)
{
    static uint16_t *v;
    v= params->op1.op16;
    /* Indirect reg/byte/word: the memory access uses an extra cycle. */
    if((INSTR_AM(cpu->i) & 1) && cpu->i_cycles == params->start_cycle) {
        v = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        *v -= 2;
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_mv
 *
 * MV instruction implementation.
 */
void core_cpu_i_op_mv(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle + 1) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
        LOGD("MV cycle 1");
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle + 1) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
        LOGD("MV cycle 1");
    } else {
        *dst = *src;
        cpu->i_cycles = 0;
        LOGD("MV cycle 2");
    }
}

/*
 * core_cpu_i_op_cmp
 *
 * CMP instruction implementation.
 */
void core_cpu_i_op_cmp(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst - *src;
        int32_t itemp = (int32_t)*dst - (int32_t)*src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_tst
 *
 * TST instruction implementation.
 */
void core_cpu_i_op_tst(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst & *src;
        cpu->r[R_F] = !temp || ((temp < 0) << 3);
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_add
 *
 * ADD instruction implementation.
 */
void core_cpu_i_op_add(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst - *src;
        int32_t itemp = (int32_t)*dst - (int32_t)*src;
        *dst += *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

/*
 * core_cpu_i_op_sub
 *
 * SUB instruction implementation.
 */
void core_cpu_i_op_sub(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst - *src;
        int32_t itemp = (int32_t)*dst - (int32_t)*src;
        *dst -= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_mul(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst * *src;
        int32_t itemp = (int32_t)*dst * (int32_t)*src;
        *dst *= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_div(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst / *src;
        int32_t itemp = (int32_t)*dst / (int32_t)*src;
        *dst /= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_lsl(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst << *src;
        int32_t itemp = (int32_t)*dst << (int32_t)*src;
        *dst <<= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_lsr(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst >> *src;
        uint32_t utemp = (uint32_t)*dst - (uint32_t)*src;
        *dst >>= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((utemp > 0xffff) << 1) ||   /* C */
                ((utemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_asr(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        int16_t temp = *(int16_t *)dst >> *(int16_t *)src;
        int32_t itemp = *(int32_t *)dst >> *(int32_t *)src;
        *(int16_t *)dst >>= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_and(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst & *src;
        int32_t itemp = (int32_t)*dst & (int32_t)*src;
        *dst &= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_or(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst | *src;
        int32_t itemp = (int32_t)*dst | (int32_t)*src;
        *dst |= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

void core_cpu_i_op_xor(struct core_cpu *cpu, struct core_instr_params *params)
{
    int mode;
    static uint16_t *dst, *src;

    dst = params->op1.op16;
    src = params->op2.op16;
    mode = INSTR_AM(cpu->i);
    /* Indirect (memory) op2 */ 
    if((mode == AM_DR_IR || mode == AM_DR_IB || mode == AM_DR_IW) &&
            cpu->i_cycles == params->start_cycle) {
        src = core_mmu_getwp(cpu->mmu, *params->op2.op16);
        ++cpu->i_cycles;
    /* Indirect (memory) op1 */
    } else if((mode == AM_IR_DR || mode == AM_IB_DR || mode == AM_IW_DR) &&
            cpu->i_cycles == params->start_cycle) {
        dst = core_mmu_getwp(cpu->mmu, *params->op1.op16);
        ++cpu->i_cycles;
    } else {
        uint16_t temp = *dst ^ *src;
        int32_t itemp = (int32_t)*dst ^ (int32_t)*src;
        *dst ^= *src;
        cpu->r[R_F] = !temp ||              /* Z */
                ((itemp > 0xffff) << 1) ||   /* C */
                ((itemp > 0x7fff) << 2) ||   /* O */
                ((temp < 0) << 3);          /* N */
        cpu->i_cycles = 0;
    }
}

