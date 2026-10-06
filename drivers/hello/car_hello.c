// SPDX-License-Identifier: GPL-2.0
/* First verify the module toolchain before connecting GPIO or I2C devices. */
#include <linux/init.h>
#include <linux/module.h>

static int __init car_hello_init(void)
{
	pr_info("car_hello: loaded (driver baseline)\n");
	return 0;
}

static void __exit car_hello_exit(void)
{
	pr_info("car_hello: unloaded\n");
}

module_init(car_hello_init);
module_exit(car_hello_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Car project module build/load/unload baseline");
