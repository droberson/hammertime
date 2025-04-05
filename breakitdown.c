#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/uaccess.h>

/* compatability  stuff :/ */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
#define safe_read_kernel(dst, src, len) probe_kernel_read((dst), (src), (len))
#else
#define safe_read_kernel(dst, src, len) copy_from_kernel_nofault((dst), (src), (len))
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Roberson");
MODULE_DESCRIPTION("Scan kernel memory for struct module entries");
MODULE_VERSION("0.5");

/* TODO make sure scan_start % MODULE_SCAN_STEP is zero for proper alignment */
/* TODO do this programatically instead of relying on input or scanning the entire range */
static unsigned long scan_start = 0xffffffffc0000000UL;
module_param(scan_start, ulong, 0);
MODULE_PARM_DESC(scan_start, "Start address of scan range");

static unsigned long scan_end = 0xffffffffffe00000UL;
module_param(scan_end, ulong, 0);
MODULE_PARM_DESC(scan_end, "End address of scan range");

/* may need to edit this. take 64 byte steps. */
#define MODULE_SCAN_STEP    64

/* TODO review 'struct module' to see if any other fields are worth extracting */
/* get offsets with 'pahole' (dwarves package on Debian):
   pahole -C module /usr/lib/debug/boot/vmlinux-$(uname -r)
 */

// struct module {
//     enum module_state          state;                /*     0     4 */
//     char                       name[56];             /*    24    56 */
//     ...
//     void                       (*exit)(void);        /*   776     8 */
//     atomic_t                   refcnt;               /*   784     4 */
//
#define MODULE_NAME_OFFSET    24
#define MODULE_STATE_OFFSET    0
#define MODULE_EXIT_OFFSET   776
#define MODULE_REFCNT_OFFSET 784

/* filter out garbage name results */
static bool valid_name(const char *buf, size_t len)
{
	size_t i;
	bool null_seen = false;

	/* TODO investigate if some rootkits zero out name[] member */
	for (i = 0; i < len; i++) {
		if (buf[i] == '\0') {
			null_seen = true;
			break;
		}
		if (!isprint(buf[i]))
			return false;
	}

	return null_seen && i >= 4;
}

static int __init breakitdown_init(void)
{
	unsigned long addr;
	int hits = 0;

	pr_info("breakitdown: scanning for 'struct module' entries from %lx to %lx...\n",
			scan_start, scan_end);

	/* iterate provided scan ranges to find 'struct module' entries */
	for (addr = scan_start; addr < scan_end; addr += MODULE_SCAN_STEP) {
		char name[56] = {0};
		unsigned int state = 0;
		void *exit_func = NULL;
		atomic_t refcnt;

		/*
		  sanity checks; make sure current memory range populates sane
		  values into a 'struct module' entry
		*/
		if (safe_read_kernel(&state, (void *)(addr + MODULE_STATE_OFFSET), sizeof(state))) {
			continue;
		}

		if (state > 5) {
			continue;
		}

		if (safe_read_kernel(&name, (void *)(addr + MODULE_NAME_OFFSET), sizeof(name))) {
			continue;
		}

		if (!valid_name(name, sizeof(name))) {
			continue;
		}

		if (safe_read_kernel(&exit_func, (void *)(addr + MODULE_EXIT_OFFSET), sizeof(exit_func))) {
			continue;
		}

		if (!safe_read_kernel(&refcnt, (void *)(addr + MODULE_REFCNT_OFFSET), sizeof(refcnt))) {
			int refcount_res = atomic_read(&refcnt);
			if (refcount_res < 0 || refcount_res > 10000) {
				continue;
			}
		}

		/* if we get here, its likely that this is a 'struct module' entry */
		pr_info("breakitdown: found module '%s' at %lx, state=%d, refcnt=%d, cleanup=%px\n",
				name,
				addr,
				state,
				atomic_read(&refcnt),
				exit_func);

		hits++;
	}

	pr_info("breakitdown: done - %d likely 'struct module' entries found\n", hits);
	return -ECANCELED; /* return -ECANCELED so the module doesn't remain loaded */
}

static void __exit breakitdown_exit(void)
{
	pr_info("breakitdown: module exit\n");
}

module_init(breakitdown_init);
module_exit(breakitdown_exit);
