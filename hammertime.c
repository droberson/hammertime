#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Roberson");
MODULE_DESCRIPTION("Invoke cleanup_module from user-supplied address slide out.");
MODULE_VERSION("0.2");

static unsigned long cleanup_fn = 0;
module_param(cleanup_fn, ulong, 0000);
MODULE_PARM_DESC(cleanup_fn, "Address of the target cleanup_module() function");

static int __init hammertime_init(void)
{
	if (!cleanup_fn) {
		pr_err("No cleanup_module() address provided.\n");
		return -EINVAL;
	}

	pr_info("Calling cleanup_module() at 0x%lx\n", cleanup_fn);
	((void (*)(void))cleanup_fn)();

	return -ECANCELED; /* return -ECANCELED so module does not remain loaded */
}

static void __exit hammertime_exit(void)
{
	pr_info("hammertime exiting cleanly.\n");
}

module_init(hammertime_init);
module_exit(hammertime_exit);
