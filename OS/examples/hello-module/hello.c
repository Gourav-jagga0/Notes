// Minimal Linux kernel module: prints a line to the kernel log on load and unload.
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GJ");
MODULE_DESCRIPTION("Hello world kernel module");

static int __init hello_init(void)
{
	pr_info("hello: module loaded\n");
	return 0; /* non-zero would make insmod fail */
}

static void __exit hello_exit(void)
{
	pr_info("hello: module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);
