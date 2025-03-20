/*
 * Alibaba's ftrace_make_call bug fixing hotfix
 *
 * Copyright (C) 2023 Alibaba Ltd.
 *
 * Author: yongdezhang <zhangyongde.zyd@alibaba-inc.com>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#include <linux/cpu.h>
#include <linux/stacktrace.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/preempt.h>
#include <linux/delay.h>
#include <linux/tick.h>
#include <asm/module.h>
#include <asm/insn.h>
#include <linux/stop_machine.h>
#include <linux/sched.h>
#include <linux/livepatch.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/ftrace.h>
#include <linux/bitops.h>
#include <linux/elf.h>
#include <linux/ftrace.h>
#include <linux/gfp.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleloader.h>
#include <linux/random.h>
#include <linux/scs.h>
#include <linux/vmalloc.h>

#include <asm/alternative.h>
#include <asm/insn.h>
#include <asm/scs.h>
#include <asm/sections.h>
#include <asm/orc_lookup.h>

#include "hotfix_util.h"

enum aarch64_reloc_op {
        RELOC_OP_NONE,
        RELOC_OP_ABS,
        RELOC_OP_PREL,
        RELOC_OP_PAGE,
};

enum aarch64_insn_movw_imm_type {
        AARCH64_INSN_IMM_MOVNZ,
        AARCH64_INSN_IMM_MOVKZ,
};

DEFINE_SYSFS_BASE();
__maybe_unused static atomic_t ali_nr_running = ATOMIC_INIT(0);
int  (*orig_aarch64_insn_patch_text)(void **addr, u32 *insn, int cnt);
u64 (*orig_module_emit_veneer_for_adrp)(struct module *mod, Elf64_Shdr *sechdrs,
                                void *loc, u64 val);
u32 (*orig_aarch64_insn_encode_immediate)(enum aarch64_insn_imm_type type,
                                  u32 insn, u64 imm);
u64 (*orig_module_emit_plt_entry)(struct module *mod, Elf64_Shdr *sechdrs,
                          void *loc, const Elf64_Rela *rela,
                          Elf64_Sym *sym);
u32 (*orig_aarch64_insn_gen_branch_imm)(unsigned long pc, unsigned long addr,
                                enum aarch64_insn_branch_type type);
DEFINE_ORIG_FUNC(int, apply_relocate_add, 5, Elf64_Shdr *, sechdrs, const char *, strtab,
		unsigned int, symindex, unsigned int, relsec, struct module *, me);

#define MAX_STACK_ENTRIES  100
#define STACK_ERR_BUF_SIZE 128

static unsigned long old_func_addr;
static unsigned long old_func_size;

static u64 do_reloc(enum aarch64_reloc_op reloc_op, __le32 *place, u64 val)
{
        switch (reloc_op) {
        case RELOC_OP_ABS:
                return val;
        case RELOC_OP_PREL:
                return val - (u64)place;
        case RELOC_OP_PAGE:
                return (val & ~0xfff) - ((u64)place & ~0xfff);
        case RELOC_OP_NONE:
                return 0;
        }

        pr_err("do_reloc: unknown relocation operation %d\n", reloc_op);
        return 0;
}

static int reloc_insn_imm(enum aarch64_reloc_op op, __le32 *place, u64 val,
                          int lsb, int len, enum aarch64_insn_imm_type imm_type)
{
        u64 imm, imm_mask;
        s64 sval;
        u32 insn = le32_to_cpu(*place);

        /* Calculate the relocation value. */
        sval = do_reloc(op, place, val);
        sval >>= lsb;

        /* Extract the value bits and shift them to bit 0. */
        imm_mask = (BIT(lsb + len) - 1) >> lsb;
        imm = sval & imm_mask;

        /* Update the instruction's immediate field. */
        insn = orig_aarch64_insn_encode_immediate(imm_type, insn, imm);
	
        *place = cpu_to_le32(insn);

        /*
         * Extract the upper value bits (including the sign bit) and
         * shift them to bit 0.
         */
        sval = (s64)(sval & ~(imm_mask >> 1)) >> (len - 1);

        /*
         * Overflow has occurred if the upper bits are not all equal to
         * the sign bit of the value.
         */
        if ((u64)(sval + 1) >= 2)
                return -ERANGE;

        return 0;
}

static int reloc_insn_adrp(struct module *mod, Elf64_Shdr *sechdrs,
                           __le32 *place, u64 val)
{
        u32 insn;

        if (!is_forbidden_offset_for_adrp(place))
                return reloc_insn_imm(RELOC_OP_PAGE, place, val, 12, 21,
                                      AARCH64_INSN_IMM_ADR);

        /* patch ADRP to ADR if it is in range */
        if (!reloc_insn_imm(RELOC_OP_PREL, place, val & ~0xfff, 0, 21,
                            AARCH64_INSN_IMM_ADR)) {
                insn = le32_to_cpu(*place);
                insn &= ~BIT(31);
        } else {
                /* out of range for ADR -> emit a veneer */
                val = orig_module_emit_veneer_for_adrp(mod, sechdrs, place, val & ~0xfff);
                if (!val)
                        return -ENOEXEC;
                insn = orig_aarch64_insn_gen_branch_imm((u64)place, val,
                                                   AARCH64_INSN_BRANCH_NOLINK);
        }

        *place = cpu_to_le32(insn);
        return 0;
}

static int reloc_insn_movw(enum aarch64_reloc_op op, __le32 *place, u64 val,
                           int lsb, enum aarch64_insn_movw_imm_type imm_type)
{
        u64 imm;
        s64 sval;
        u32 insn = le32_to_cpu(*place);

        sval = do_reloc(op, place, val);
        imm = sval >> lsb;

        if (imm_type == AARCH64_INSN_IMM_MOVNZ) {
                /*
                 * For signed MOVW relocations, we have to manipulate the
                 * instruction encoding depending on whether or not the
                 * immediate is less than zero.
                 */
                insn &= ~(3 << 29);
                if (sval >= 0) {
                        /* >=0: Set the instruction to MOVZ (opcode 10b). */
                        insn |= 2 << 29;
                } else {
                        /*
                         * <0: Set the instruction to MOVN (opcode 00b).
                         *     Since we've masked the opcode already, we
                         *     don't need to do anything other than
                         *     inverting the new immediate field.
                         */
                        imm = ~imm;
                }
        }

        /* Update the instruction with the new encoding. */
        insn = orig_aarch64_insn_encode_immediate(AARCH64_INSN_IMM_16, insn, imm);
        *place = cpu_to_le32(insn);

        if (imm > U16_MAX)
                return -ERANGE;

        return 0;
}

static int reloc_data(enum aarch64_reloc_op op, void *place, u64 val, int len)
{
        s64 sval = do_reloc(op, place, val);

        /*
         * The ELF psABI for AArch64 documents the 16-bit and 32-bit place
         * relative and absolute relocations as having a range of [-2^15, 2^16)
         * or [-2^31, 2^32), respectively. However, in order to be able to
         * detect overflows reliably, we have to choose whether we interpret
         * such quantities as signed or as unsigned, and stick with it.
         * The way we organize our address space requires a signed
         * interpretation of 32-bit relative references, so let's use that
         * for all R_AARCH64_PRELxx relocations. This means our upper
         * bound for overflow detection should be Sxx_MAX rather than Uxx_MAX.
         */

        switch (len) {
        case 16:
                *(s16 *)place = sval;
                switch (op) {
                case RELOC_OP_ABS:
                        if (sval < 0 || sval > U16_MAX)
                                return -ERANGE;
                        break;
                case RELOC_OP_PREL:
                        if (sval < S16_MIN || sval > S16_MAX)
                                return -ERANGE;
                        break;
                default:
                        pr_err("Invalid 16-bit data relocation (%d)\n", op);
                        return 0;
                }
                break;
        case 32:
                *(s32 *)place = sval;
                switch (op) {
                case RELOC_OP_ABS:
                        if (sval < 0 || sval > U32_MAX)
                                return -ERANGE;
                        break;
                case RELOC_OP_PREL:
                        if (sval < S32_MIN || sval > S32_MAX)
				if (sval < S32_MIN || sval > S32_MAX)
                                return -ERANGE;
                        break;
                default:
                        pr_err("Invalid 32-bit data relocation (%d)\n", op);
                        return 0;
                }
                break;
        case 64:
                *(s64 *)place = sval;
                break;
        default:
                pr_err("Invalid length (%d) for data relocation\n", len);
                return 0;
        }
        return 0;
}



static int try_enable_patch(void *data){
    int ret = 0;
    
    JUMP_INSTALL(apply_relocate_add);
out:
    return ret;
}

int new_apply_relocate_add(Elf64_Shdr *sechdrs,
               const char *strtab,
               unsigned int symindex,
               unsigned int relsec,
               struct module *me)
{
    atomic_inc(&ali_nr_running);
    
    unsigned int i;
        int ovf;
        bool overflow_check;
        Elf64_Sym *sym;
        void *loc;
        u64 val;
        Elf64_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	printk("new_apply_relocate_add\n");
        for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* loc corresponds to P in the AArch64 ELF document. */
                loc = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
                        + rel[i].r_offset;

                /* sym is the ELF symbol we're referring to. */
                sym = (Elf64_Sym *)sechdrs[symindex].sh_addr
                        + ELF64_R_SYM(rel[i].r_info);

                /* val corresponds to (S + A) in the AArch64 ELF document. */
                val = sym->st_value + rel[i].r_addend;

                /* Check for overflow by default. */
                overflow_check = true;

                /* Perform the static relocation. */
                switch (ELF64_R_TYPE(rel[i].r_info)) {
                /* Null relocations. */
                case R_ARM_NONE:
                case R_AARCH64_NONE:
                        ovf = 0;
                        break;

                /* Data relocations. */
                case R_AARCH64_ABS64:
                        overflow_check = false;
                        ovf = reloc_data(RELOC_OP_ABS, loc, val, 64);
                        break;
                case R_AARCH64_ABS32:
			ovf = reloc_data(RELOC_OP_ABS, loc, val, 32);
                        break;
                case R_AARCH64_ABS16:
                        ovf = reloc_data(RELOC_OP_ABS, loc, val, 16);
                        break;
                case R_AARCH64_PREL64:
                        overflow_check = false;
                        ovf = reloc_data(RELOC_OP_PREL, loc, val, 64);
                        break;
                case R_AARCH64_PREL32:
                        ovf = reloc_data(RELOC_OP_PREL, loc, val, 32);
                        break;
                case R_AARCH64_PREL16:
                        ovf = reloc_data(RELOC_OP_PREL, loc, val, 16);
                        break;

		/* MOVW instruction relocations. */
                case R_AARCH64_MOVW_UABS_G0_NC:
                        overflow_check = false;
                        fallthrough;
                case R_AARCH64_MOVW_UABS_G0:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_UABS_G1_NC:
                        overflow_check = false;
                        fallthrough;
                case R_AARCH64_MOVW_UABS_G1:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 16,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_UABS_G2_NC:
                        overflow_check = false;
                        fallthrough;
                case R_AARCH64_MOVW_UABS_G2:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 32,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_UABS_G3:
                        /* We're using the top bits so we can't overflow. */
                        overflow_check = false;
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 48,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_SABS_G0:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 0,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
                case R_AARCH64_MOVW_SABS_G1:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 16,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
                case R_AARCH64_MOVW_SABS_G2:
                        ovf = reloc_insn_movw(RELOC_OP_ABS, loc, val, 32,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
		case R_AARCH64_MOVW_PREL_G0_NC:
                        overflow_check = false;
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 0,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_PREL_G0:
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 0,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
                case R_AARCH64_MOVW_PREL_G1_NC:
                        overflow_check = false;
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_PREL_G1:
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 16,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
                case R_AARCH64_MOVW_PREL_G2_NC:
                        overflow_check = false;
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
                                              AARCH64_INSN_IMM_MOVKZ);
                        break;
                case R_AARCH64_MOVW_PREL_G2:
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 32,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;
                case R_AARCH64_MOVW_PREL_G3:
                        /* We're using the top bits so we can't overflow. */
                        overflow_check = false;
                        ovf = reloc_insn_movw(RELOC_OP_PREL, loc, val, 48,
                                              AARCH64_INSN_IMM_MOVNZ);
                        break;

                /* Immediate instruction relocations. */
                case R_AARCH64_LD_PREL_LO19:
                        ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 19,
                                             AARCH64_INSN_IMM_19);
                        break;
		case R_AARCH64_ADR_PREL_LO21:
                        ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 0, 21,
                                             AARCH64_INSN_IMM_ADR);
                        break;
                case R_AARCH64_ADR_PREL_PG_HI21_NC:
                        overflow_check = false;
                        fallthrough;
                case R_AARCH64_ADR_PREL_PG_HI21:
                        ovf = reloc_insn_adrp(me, sechdrs, loc, val);
                        if (ovf && ovf != -ERANGE)
                                return ovf;
                        break;
                case R_AARCH64_ADD_ABS_LO12_NC:
                case R_AARCH64_LDST8_ABS_LO12_NC:
                        overflow_check = false;
                        ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 0, 12,
                                             AARCH64_INSN_IMM_12);
                        break;
                case R_AARCH64_LDST16_ABS_LO12_NC:
                        overflow_check = false;
                        ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 1, 11,
                                             AARCH64_INSN_IMM_12);
                        break;
                case R_AARCH64_LDST32_ABS_LO12_NC:
                        overflow_check = false;
                        ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 2, 10,
                                             AARCH64_INSN_IMM_12);
                        break;
                case R_AARCH64_LDST64_ABS_LO12_NC:
                        overflow_check = false;
                        ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 3, 9,
                                             AARCH64_INSN_IMM_12);
                        break;
		case R_AARCH64_LDST128_ABS_LO12_NC:
                        overflow_check = false;
                        ovf = reloc_insn_imm(RELOC_OP_ABS, loc, val, 4, 8,
                                             AARCH64_INSN_IMM_12);
                        break;
		case R_AARCH64_TSTBR14:
                        ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 14,
                                             AARCH64_INSN_IMM_14);
                        break;
                case R_AARCH64_CONDBR19:
                        ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 19,
                                             AARCH64_INSN_IMM_19);
                        break;
                case R_AARCH64_JUMP26:
                case R_AARCH64_CALL26:
                        ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 26,
                                             AARCH64_INSN_IMM_26);
                        if (ovf == -ERANGE) {
                                val = orig_module_emit_plt_entry(me, sechdrs, loc, &rel[i], sym);
                                if (!val)
                                        return -ENOEXEC;
                                ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2,
                                                     26, AARCH64_INSN_IMM_26);
                        }
                        break;

                default:
                        pr_err("module %s: unsupported RELA relocation: %llu\n",
                               me->name, ELF64_R_TYPE(rel[i].r_info));
                        return -ENOEXEC;
                }

                if (overflow_check && ovf == -ERANGE) {
			if (overflow_check)
				printk("overflow_check!!\n");
			if (ovf == -ERANGE)
				printk("OVF!!\n");
                        goto overflow;
		}

        }
    atomic_dec(&ali_nr_running);
    return 0;
overflow:
    	atomic_dec(&ali_nr_running);
        pr_err("module %s: overflow in relocation type %d val %Lx\n",
               me->name, (int)ELF64_R_TYPE(rel[i].r_info), val);
        return -ENOEXEC;
}

static int lookup_syms(void)
{
	SYM_SET_ORIG_FUNCTION_ADDRESS(apply_relocate_add, 0xffff800080029278);
	SYM_SET_ORIG_FUNCTION_ADDRESS(aarch64_insn_patch_text,0xffff800080d15908);
	SYM_SET_ORIG_FUNCTION_ADDRESS(aarch64_insn_encode_immediate,0xffff800080d16f60);
	SYM_SET_ORIG_FUNCTION_ADDRESS(module_emit_veneer_for_adrp, 0xffff80008002a0e8);
	SYM_SET_ORIG_FUNCTION_ADDRESS(aarch64_insn_gen_branch_imm,0xffff800080d17030);
	SYM_SET_ORIG_FUNCTION_ADDRESS(module_emit_plt_entry, 0xffff800080029f38);
	return 0;
}

static int ftrace_plt_init(void)
{
	int ret = 0;
	if (lookup_syms())
		return -EINVAL;
	JUMP_INIT(apply_relocate_add);
	ret = try_enable_patch(NULL);
	if (ret){
		pr_err("manal_hotfix for ftrace_location_range execute failed ... \n");
		JUMP_REMOVE_SYM(apply_relocate_add);
		return ret;
	}
	pr_warn("ftrace_location_range load.\n");

	return 0;
}

static void ftrace_plt_exit(void)
{
	JUMP_REMOVE(apply_relocate_add);
    do{
        msleep(10);
    } while(atomic_read(&ali_nr_running) > 0);
	pr_err("arm ftrace plt  unload.\n");
}

module_init(ftrace_plt_init);
module_exit(ftrace_plt_exit);

MODULE_DESCRIPTION("Example of dynamically jump to replace the unexported symbol");
MODULE_AUTHOR("yongdezhang <zhangyongde.zyd@alibaba-inc.com>");
MODULE_LICENSE("GPL v2");

