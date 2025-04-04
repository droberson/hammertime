#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Roberson");
MODULE_DESCRIPTION("Minimal LKM that cannot be easily unloaded with rmmod");
MODULE_VERSION("0.1");

static int __init canttouchthis_init(void) {
    pr_info("canttouchthis: loaded\n");

    // Prevent unloading
    if (!try_module_get(THIS_MODULE)) {
        pr_err("canttouchthis: try_module_get failed\n");
        return -1;
    }

    return 0;
}

static void __exit canttouchthis_exit(void) {
    pr_info("canttouchthis: unloading\n");
    module_put(THIS_MODULE);
}

module_init(canttouchthis_init);
module_exit(canttouchthis_exit);
