#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Roberson");
MODULE_DESCRIPTION("Invoke cleanup_module from user-supplied address slide out.");
MODULE_VERSION("0.1");

static unsigned long cleanup_fn = 0;
module_param(cleanup_fn, ulong, 0000);
MODULE_PARM_DESC(cleanup_fn, "Address of the target cleanup_module() function");

static int __init cool_removal_init(void)
{
	if (!cleanup_fn) {
		pr_err("No cleanup_module() address provided.\n");
		return -EINVAL;
	}

	pr_info("Calling cleanup_module() at 0x%lx\n", cleanup_fn);

	// cast and call it
	((void (*)(void))cleanup_fn)();

	// fail init so we unload automatically
	return -ECANCELED;
}

static void __exit cool_removal_exit(void)
{
	pr_info("cool_removal_module exiting cleanly.\n");
}

module_init(cool_removal_init);
module_exit(cool_removal_exit);
