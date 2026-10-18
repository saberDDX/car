// SPDX-License-Identifier: GPL-2.0
/* Stage 1: identify an MPU6050 through a repeated-START I2C transaction. */
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sysfs.h>

#define MPU6050_WHO_AM_I 0x75
#define MPU6050_ID 0x68

static char *run_id = "manual";
module_param(run_id, charp, 0444);
MODULE_PARM_DESC(run_id, "Identifier for ownership-safe module tests");

static int car_mpu6050_read_reg(struct i2c_client *client, u8 reg)
{
	u8 value;
	struct i2c_msg messages[] = {
		{ .addr = client->addr, .len = 1, .buf = &reg },
		{ .addr = client->addr, .flags = I2C_M_RD,
		  .len = 1, .buf = &value },
	};
	int ret;

	/* One transfer: write register address, repeated START, read its value. */
	ret = i2c_transfer(client->adapter, messages, ARRAY_SIZE(messages));
	if (ret != ARRAY_SIZE(messages))
		return ret < 0 ? ret : -EIO;

	return value;
}

static ssize_t who_am_i_show(struct device *dev,
			    struct device_attribute *attribute, char *buffer)
{
	struct i2c_client *client = to_i2c_client(dev);
	int value = car_mpu6050_read_reg(client, MPU6050_WHO_AM_I);

	if (value < 0)
		return value;
	return sysfs_emit(buffer, "0x%02x\n", value);
}
static DEVICE_ATTR_RO(who_am_i);

static struct attribute *car_mpu6050_attrs[] = {
	&dev_attr_who_am_i.attr,
	NULL,
};
ATTRIBUTE_GROUPS(car_mpu6050);

static int car_mpu6050_probe(struct i2c_client *client)
{
	int value;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "adapter lacks plain I2C transfers\n");
	if (client->addr != 0x68 && client->addr != 0x69)
		return dev_err_probe(&client->dev, -EINVAL,
				     "expected a 7-bit address of 0x68 or 0x69\n");

	value = car_mpu6050_read_reg(client, MPU6050_WHO_AM_I);
	if (value < 0)
		return dev_err_probe(&client->dev, value,
				     "WHO_AM_I read failed; check power, wiring and AD0\n");
	/* WHO_AM_I stays 0x68 even when AD0 selects bus address 0x69. */
	if (value != MPU6050_ID)
		return dev_err_probe(&client->dev, -ENODEV,
				     "unexpected WHO_AM_I=0x%02x (expected 0x68)\n",
				     value);

	ret = devm_device_add_groups(&client->dev, car_mpu6050_groups);
	if (ret)
		return dev_err_probe(&client->dev, ret,
				     "cannot add identity diagnostic attribute\n");
	dev_info(&client->dev, "WHO_AM_I=0x68 address=0x%02x run_id=%s\n",
		 client->addr, run_id);
	return 0;
}

static const struct of_device_id car_mpu6050_of_match[] = {
	{ .compatible = "saberddx,car-mpu6050" },
	{ }
};
MODULE_DEVICE_TABLE(of, car_mpu6050_of_match);

static struct i2c_driver car_mpu6050_driver = {
	.driver = {
		.name = "car-mpu6050",
		.of_match_table = car_mpu6050_of_match,
	},
	.probe_new = car_mpu6050_probe,
};
module_i2c_driver(car_mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Car project MPU6050 I2C identity bring-up (Linux 6.1)");
