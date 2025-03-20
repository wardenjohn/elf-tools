#ifndef DYNAMIC_JUMP_H
#define DYNAMIC_JUMP_H

#include <linux/version.h>
#include <linux/tracepoint.h>
#include <linux/kprobes.h>
#include <linux/stacktrace.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>

#include "symbol.h"

#define MAX_FUNC_COUNT 16
#define FUNC_NAME_LENGTH 255

/*
 * DEFINE_SYSFS_BASE
 * define the base sysfs kobject
 * for each patch function should have a 
 * node record in the sysfs
 */
#define DEFINE_SYSFS_BASE() \
	static struct kobject *patch_root_kobj; \
	static struct kobject *patch_funcs_kobj[MAX_FUNC_COUNT]; \
	static int patch_funcs_kobj_cnt\
	
/*
 * INIT_SYSFS_BASE
 * before excatually patch a function,
 * create the patch function's kobject node
 */
#define INIT_SYSFS_BASE() do { \
	char name[FUNC_NAME_LENGTH]; \
	snprintf(name, FUNC_NAME_LENGTH, "manual_%s", THIS_MODULE->name); \
	if (patch_root_kobj) { printk("patch object root kobject exist.\n"); break; } \
	patch_root_kobj = kobject_create_and_add(name, kernel_kobj);    \
	if (!patch_root_kobj)    return -ENOMEM; \
	patch_funcs_kobj_cnt = 0; \
	memset(patch_funcs_kobj, 0, sizeof(patch_funcs_kobj)); \
} while(0)

/*
 *
 * LOOKUP_SYM_CONV_TO_NAME
 * lookup a symbol by kallsyms_lookup_name
 * and use as orig_{func_name} as conversion
 */
#define LOOKUP_SYM_CONV_TO_NAME(name, sym) do { \
	orig_##name = (void *)kallsyms_lookup_name(sym); \
	if (!orig_##name) { \
		pr_err("kallsyms_lookup_name: %s\n", #name);        \
		return -EINVAL;                     \
	} \
} while(0)

/*
 * LOOKUP_SYMS
 *
 * lookup symbol
 */
#define LOOKUP_SYMS(name) LOOKUP_SYM_CONV_TO_NAME(name, #name)
#define LOOKUP_SYMS_RETURN LOOKUP_SYM_CONV_TO_NAME(name, #name)

/*
 * ORIG_FUNC
 *
 * define the original function 
 * before calling or using it in the file
 */
#define ORIG_FUNC(rt, name, x , ...) \
	rt (*orig_##name)(__MAP(x, __SC_DECL, __VA_ARGS__)); 

/*
 * ORIG_NOINPUT_FUNC
 *
 * define the original function with no 
 * input parameter, befor calling or
 * using it in the file
 */
#define ORIG_NOINPU_FUNC(rt, name) \
	rt (*orig_##name)(void);

/*
 * SYM_SET_ORIG_FUNCTION_ADDRESS
 *
 * to handle the situation of patching
 * the unexported function. Force to
 * jump by setting the function address
 * manually.
 */
#define SYM_SET_ORIG_FUNCTION_ADDRESS(func_name, addr) \
	orig_##func_name = (void *)addr

#if CONFIG_ARM64

#define RELATIVEJUMP_SIZE 8

#define JUMP_INIT(func) do { \
	INIT_SYSFS_BASE(); \
	unsigned long long addr = (unsigned long long)&new_##func;      \
	unsigned long long orig_addr = (unsigned long long)orig_##func;      \
	int size = 0;      \
	/* stp x29, x30, [sp,#-16]! */              \
            e9_##func[0] = 0xa9bf7bfdu;                 \
            /* mov x29, #0x0 */                         \
            e9_##func[1] = 0xd280001du | ((addr & 0xffff) << 5);        \
            /* movk    x29, #0x0, lsl #16 */                \
            e9_##func[2] = 0xf2a0001du | (((addr & 0xffff0000) >> 16) << 5);        \
            /* movk    x29, #0x0, lsl #32 */                \
            e9_##func[3] = 0xf2c0001du | (((addr & 0xffff00000000) >> 32) << 5);    \
            /* movk    x29, #0x0, lsl #48 */                \
            e9_##func[4] = 0xf2e0001du | (((addr & 0xffff000000000000) >> 48) << 5);   \
            /* blr x29 */                                   \
            e9_##func[5] = 0xd63f03a0u;                     \
            /* ldp x29, x30, [sp],#16 */                    \
            e9_##func[6] = 0xa8c17bfdu;                     \
            /* ret */                                       \
            e9_##func[7] = 0xd65f03c0u;                     \
            for (; size <  RELATIVEJUMP_SIZE; size++) {\
                    addr_##func[size] = orig_addr + size*4;\
            } \
        } while (0)

#define JUMP_INSTALL(func) do { \
	memcpy(inst_##func, orig_##func, sizeof(inst_##func));    \
	orig_aarch64_insn_patch_text((void **)addr_##func, (u32 *)e9_##func, RELATIVEJUMP_SIZE);    \
	patch_funcs_kobj_##func = kobject_create_and_add(#func, patch_root_kobj);       \
	if (!patch_funcs_kobj_##func) {  \
		int i = 0;                                                              \
		for (i = 0; i < patch_funcs_kobj_cnt; i++) {                            \
			kobject_put(patch_funcs_kobj[i]);                               \
		}                                                                       \
		kobject_put(patch_root_kobj);                                           \
		printk(KERN_ERR "create patch_funcs_kobj_" #func "failed\n");           \
	}                                                                               \
	patch_funcs_kobj[patch_funcs_kobj_cnt++] = patch_funcs_kobj_##func;             \
} while (0)

#define JUMP_REMOVE_SYM(func) do {      \
	if (patch_funcs_kobj_cnt > 0 )    \
		patch_funcs_kobj[patch_funcs_kobj_cnt--] = NULL;   \
	if (0 == patch_funcs_kobj_cnt)                             \
		kobject_put(patch_root_kobj);  \
}while(0)

#define JUMP_REMOVE(func) do{                       \
        orig_aarch64_insn_patch_text((void **)addr_##func, (u32 *)inst_##func, RELATIVEJUMP_SIZE);           \   
        kobject_put(patch_funcs_kobj_##func);             \
        patch_funcs_kobj[patch_funcs_kobj_cnt--] = NULL;                                                     \
        if (0 == patch_funcs_kobj_cnt)                                                                       \
            kobject_put(patch_root_kobj);  \
}while(0)

#define DEFINE_ORIG_FUNC(rt, name, x, ...)                  \
    static unsigned int e9_##name[RELATIVEJUMP_SIZE];               \
    static void *addr_##name[RELATIVEJUMP_SIZE];               \
    static unsigned int inst_##name[RELATIVEJUMP_SIZE];             \
    static rt new_##name(__MAP(x, __SC_DECL, __VA_ARGS__));         \
    static rt (*orig_##name)(__MAP(x, __SC_DECL, __VA_ARGS__));         \
    static struct kobject *patch_funcs_kobj_##name

#else

/* X86_64 or other arch*/
#define RELATIVEJUMP_SIZE   5

#define JUMP_INIT(func) do {                                                    \
	INIT_SYSFS_BASE();                                                              \
	e9_##func[0] = 0xe9;                                            \
	(*(int *)(&e9_##func[1])) = (long)new_##func - (long) orig_##func - RELATIVEJUMP_SIZE;                         \
} while (0)


#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
extern void *(*orig_text_poke_smp)(void *, const void *, size_t);

#define JUMP_INSTALL(func) do {                                         \
	memcpy(inst_##func, orig_##func, RELATIVEJUMP_SIZE);    \
	orig_text_poke_smp(orig_##func, e9_##func, RELATIVEJUMP_SIZE);                  \
	patch_funcs_kobj_##func = kobject_create_and_add(#func, patch_root_kobj);       \
	if (!patch_funcs_kobj_##func) {                                                 \
		int i = 0;                                                              \
		for (i = 0; i < patch_funcs_kobj_cnt; i++) {                            \
			kobject_put(patch_funcs_kobj[i]);                               \
		}                                                                       \
		kobject_put(patch_root_kobj);                                           \
		printk(KERN_ERR "create patch_funcs_kobj_" #func "failed\n");           \
	}                                                                               \
	patch_funcs_kobj[patch_funcs_kobj_cnt++] = patch_funcs_kobj_##func;             \
} while (0)

#define JUMP_REMOVE(func) do {                                    \
	orig_text_poke_smp(orig_##func, inst_##func, RELATIVEJUMP_SIZE);        \
	kobject_put(patch_funcs_kobj_##func);                   \
	patch_funcs_kobj[patch_funcs_kobj_cnt--] = NULL;        \
	if (0 == patch_funcs_kobj_cnt)                          \
	kobject_put(patch_root_kobj);                           \
}while(0);

#else
/*LINUX_VERSION_CODE > 3.12*/
extern void *(*orig_text_poke_bp)(void *addr, const void *opcode, size_t len, void *handler);

#define JUMP_INSTALL(func) do { \
	memcpy(inst_##func, orig_##func, RELATIVEJUMP_SIZE);    \
	orig_text_poke_bp(orig_##func, e9_##func,               \
			RELATIVEJUMP_SIZE, new_##func);         \
	patch_funcs_kobj_##func = kobject_create_and_add(#func, patch_root_kobj);               \
	if (!patch_funcs_kobj_##func) { \
		int i = 0; \
		for (i = 0; i < patch_funcs_kobj_cnt; i++) {            \
			kobject_put(patch_funcs_kobj[i]);               \
		}                                                       \
		kobject_put(patch_root_kobj);                                   \
		printk(KERN_ERR "create patch_funcs_kobj_" #func "failed\n");           \
	}                       						\
	patch_funcs_kobj[patch_funcs_kobj_cnt++] = patch_funcs_kobj_##func;     \
} while (0)

#define JUMP_REMOVE(func) do { \
	orig_text_poke_bp(orig_##func, inst_##func, RELATIVEJUMP_SIZE, new_##func); \
	kobject_put(patch_funcs_kobj_##func); \
	patch_funcs_kobj[patch_funcs_kobj_cnt--] = NULL; \
	if (0 == patch_funcs_kobj_cnt)  \
		kobject_put(patch_root_kobj); \
}while(0)

#endif

#define DEFINE_ORIG_FUNC(rt, name, x, ...)                                      \
        unsigned char e9_##name[RELATIVEJUMP_SIZE];                             \
        unsigned char inst_##name[RELATIVEJUMP_SIZE];                           \
        extern rt new_##name(__MAP(x, __SC_DECL, __VA_ARGS__));                 \
        rt (*orig_##name)(__MAP(x, __SC_DECL, __VA_ARGS__));                    \
        static struct kobject *patch_funcs_kobj_##name
#endif //CONFIG_ARM64

#define DEFINE_ORIG_FUNC0(rt, name)                                             \
        unsigned char e9_##name[RELATIVEJUMP_SIZE];                             \
        unsigned char inst_##name[RELATIVEJUMP_SIZE];                           \
        extern rt new_##name(void);                                             \
        rt (*orig_##name)(void);                                                \
        static struct kobject *patch_funcs_kobj_##name

#define DEFINE_ORIG_NOINPUT_FUNC(rt, name)                                      \
        unsigned char e9_##name[RELATIVEJUMP_SIZE];                             \
        unsigned char inst_##name[RELATIVEJUMP_SIZE];                           \
        extern rt new_##name(void);                 \
        rt (*orig_##name)(void);                                        \
        static struct kobject *patch_funcs_kobj_##name
#endif

#define BACKTRACE_DEPTH 50

#define TRACE_DUMP_STACK()      \
        do {                    \
                unsigned long trace_buf[BACKTRACE_DEPTH];       \
                                                                \
                ali_print_stack_trace(current, trace_buf);      \
        } while (0)

struct proc_dir_entry;
static inline struct proc_dir_entry *ali_proc_mkdir(const char *name,
                struct proc_dir_entry *parent)
{
        struct proc_dir_entry *ret = NULL;
        struct file *file;
        char full_name[FUNC_NAME_LENGTH];

        snprintf(full_name, FUNC_NAME_LENGTH, "/proc/%s", name);
        file = filp_open(full_name, O_RDONLY, 0);
        if (IS_ERR(file)) {
                ret = proc_mkdir(name, parent);
        } else {
                fput(file);
        }

        return ret;
}
