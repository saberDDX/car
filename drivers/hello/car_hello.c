// SPDX-License-Identifier: GPL-2.0
/* First verify the module toolchain before connecting GPIO or I2C devices. */
#include <linux/init.h>
#include <linux/module.h>

static char *run_id = "manual";
module_param(run_id, charp, 0444);
MODULE_PARM_DESC(run_id, "Identifier used to associate logs with one test run");

static int __init car_hello_init(void)
{
	pr_info("car_hello: loaded (driver baseline) run_id=%s\n", run_id);
	return 0;
}

static void __exit car_hello_exit(void)
{
	pr_info("car_hello: unloaded run_id=%s\n", run_id);
}

module_init(car_hello_init);
module_exit(car_hello_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Car project module build/load/unload baseline");
