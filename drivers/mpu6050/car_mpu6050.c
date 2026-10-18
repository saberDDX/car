// SPDX-License-Identifier: GPL-2.0
/* Synchronous six-axis acquisition; Linux 6.1 I2C client + misc interface. */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/kref.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
#include "../../include/car_mpu6050_uapi.h"

#define MPU6050_SMPLRT_DIV 0x19
#define MPU6050_CONFIG 0x1a
#define MPU6050_GYRO_CONFIG 0x1b
#define MPU6050_ACCEL_CONFIG 0x1c
#define MPU6050_FIFO_EN 0x23
#define MPU6050_INT_ENABLE 0x38
#define MPU6050_ACCEL_XOUT_H 0x3b
#define MPU6050_USER_CTRL 0x6a
#define MPU6050_PWR_MGMT_1 0x6b
#define MPU6050_PWR_MGMT_2 0x6c
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_ID 0x68

struct car_mpu6050 {
	struct i2c_client *client; /* Only under lock; NULL after unbind. */
	struct mutex lock;
	struct kref refs; /* One binding reference, plus one per open fd. */
	struct miscdevice misc;
};

static char *run_id = "manual";
module_param(run_id, charp, 0444);
MODULE_PARM_DESC(run_id, "Identifier for ownership-safe module tests");

static int car_mpu6050_read_regs(struct i2c_client *client, u8 reg,
			       u8 *buffer, u16 length)
{
	struct i2c_msg messages[] = {
		{ .addr = client->addr, .len = 1, .buf = &reg },
		{ .addr = client->addr, .flags = I2C_M_RD,
		  .len = length, .buf = buffer },
	};
	int ret;

	/* Write register address, repeated START, read: one combined transfer. */
	ret = i2c_transfer(client->adapter, messages, ARRAY_SIZE(messages));
	return ret == ARRAY_SIZE(messages) ? 0 : (ret < 0 ? ret : -EIO);
}

static int car_mpu6050_read_reg(struct i2c_client *client, u8 reg)
{
	u8 value;
	int ret = car_mpu6050_read_regs(client, reg, &value, 1);

	return ret ? ret : value;
}

static int car_mpu6050_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	u8 bytes[] = { reg, value };
	int ret = i2c_master_send(client, bytes, sizeof(bytes));

	/* Returns bytes, unlike i2c_transfer's message count. */
	return ret == sizeof(bytes) ? 0 : (ret < 0 ? ret : -EIO);
}

static int car_mpu6050_configure(struct i2c_client *client)
{
	static const struct { u8 reg; u8 value; } settings[] = {
		{ MPU6050_PWR_MGMT_2, 0x00 }, /* Enable all six axes. */
		{ MPU6050_CONFIG, 0x03 },     /* DLPF: accel 44 Hz, gyro 42 Hz. */
		{ MPU6050_SMPLRT_DIV, 9 },    /* Internal: 1 kHz / (1 + 9) = 100 Hz. */
		{ MPU6050_GYRO_CONFIG, 0 },   /* +/-250 deg/s, self-test off. */
		{ MPU6050_ACCEL_CONFIG, 0 },  /* +/-2g, self-test off. */
		{ MPU6050_FIFO_EN, 0 },
		{ MPU6050_INT_ENABLE, 0 },
		{ MPU6050_USER_CTRL, 0 },     /* No FIFO/DMP/auxiliary I2C master. */
	};
	int i, ret;

	ret = car_mpu6050_write_reg(client, MPU6050_PWR_MGMT_1, 0x80);
	if (ret)
		return ret;
	msleep(100);
	ret = car_mpu6050_write_reg(client, MPU6050_PWR_MGMT_1, 0x01);
	if (ret)
		return ret;
	msleep(50); /* Wake, gyro startup; PLL uses X gyro reference. */
	ret = car_mpu6050_read_reg(client, MPU6050_PWR_MGMT_1);
	if (ret != 0x01)
		return ret < 0 ? ret : -EIO;
	for (i = 0; i < ARRAY_SIZE(settings); i++) {
		ret = car_mpu6050_write_reg(client, settings[i].reg, settings[i].value);
		if (ret)
			return ret;
		ret = car_mpu6050_read_reg(client, settings[i].reg);
		if (ret != settings[i].value)
			return ret < 0 ? ret : -EIO;
	}
	return 0;
}

static void car_mpu6050_free(struct kref *ref)
{
	struct car_mpu6050 *mpu = container_of(ref, struct car_mpu6050, refs);

	kfree(mpu);
}

static int car_mpu6050_open(struct inode *inode, struct file *file)
{
	struct car_mpu6050 *mpu = container_of(file->private_data,
					     struct car_mpu6050, misc);
	int ret = 0;

	if (file->f_mode & FMODE_WRITE)
		return -EACCES;
	/* misc_open serializes with misc_deregister; binding ref is still held. */
	mutex_lock(&mpu->lock);
	if (!mpu->client) {
		ret = -ENODEV;
	} else {
		kref_get(&mpu->refs);
		file->private_data = mpu;
	}
	mutex_unlock(&mpu->lock);
	return ret ? ret : nonseekable_open(inode, file);
}

static int car_mpu6050_release(struct inode *inode, struct file *file)
{
	struct car_mpu6050 *mpu = file->private_data;

	kref_put(&mpu->refs, car_mpu6050_free);
	return 0;
}

static ssize_t car_mpu6050_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *offset)
{
	struct car_mpu6050 *mpu = file->private_data;
	struct car_mpu6050_sample sample = { 0 };
	u8 bytes[14];
	int ret;

	if (!count)
		return 0;
	if (count < sizeof(sample))
		return -EINVAL;
	if (mutex_lock_interruptible(&mpu->lock))
		return -ERESTARTSYS;
	if (!mpu->client) {
		ret = -ENODEV;
		goto unlock;
	}
	/* Consecutive high/low pairs: AX AY AZ TEMP GX GY GZ, big endian. */
	ret = car_mpu6050_read_regs(mpu->client, MPU6050_ACCEL_XOUT_H,
				  bytes, sizeof(bytes));
	if (ret)
		goto unlock;
	sample.timestamp_ns = ktime_get_ns();
	sample.accel_x = (s16)get_unaligned_be16(bytes + 0);
	sample.accel_y = (s16)get_unaligned_be16(bytes + 2);
	sample.accel_z = (s16)get_unaligned_be16(bytes + 4);
	sample.temperature = (s16)get_unaligned_be16(bytes + 6);
	sample.gyro_x = (s16)get_unaligned_be16(bytes + 8);
	sample.gyro_y = (s16)get_unaligned_be16(bytes + 10);
	sample.gyro_z = (s16)get_unaligned_be16(bytes + 12);
unlock:
	mutex_unlock(&mpu->lock);
	if (ret)
		return ret;
	/* The open-file reference survives a concurrent unbind. */
	if (copy_to_user(buffer, &sample, sizeof(sample)))
		return -EFAULT;
	return sizeof(sample);
}

static const struct file_operations car_mpu6050_fops = {
	.owner = THIS_MODULE,
	.open = car_mpu6050_open,
	.release = car_mpu6050_release,
	.read = car_mpu6050_read,
	.llseek = no_llseek,
};

static ssize_t who_am_i_show(struct device *dev,
			    struct device_attribute *attribute, char *buffer)
{
	struct car_mpu6050 *mpu = i2c_get_clientdata(to_i2c_client(dev));
	int value;

	mutex_lock(&mpu->lock);
	value = mpu->client ? car_mpu6050_read_reg(mpu->client, MPU6050_WHO_AM_I) : -ENODEV;
	mutex_unlock(&mpu->lock);
	return value < 0 ? value : sysfs_emit(buffer, "0x%02x\n", value);
}
static DEVICE_ATTR_RO(who_am_i);

static struct attribute *car_mpu6050_attrs[] = {
	&dev_attr_who_am_i.attr,
	NULL,
};
ATTRIBUTE_GROUPS(car_mpu6050);

static int car_mpu6050_probe(struct i2c_client *client)
{
	struct car_mpu6050 *mpu;
	int value, ret;

	static_assert(sizeof(struct car_mpu6050_sample) == 24);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return dev_err_probe(&client->dev, -EOPNOTSUPP, "adapter lacks plain I2C\n");
	if (client->addr != 0x68 && client->addr != 0x69)
		return -EINVAL;
	value = car_mpu6050_read_reg(client, MPU6050_WHO_AM_I);
	if (value < 0)
		return dev_err_probe(&client->dev, value, "WHO_AM_I read failed\n");
	/* Identity stays 0x68 even when AD0 selects bus address 0x69. */
	if (value != MPU6050_ID)
		return dev_err_probe(&client->dev, -ENODEV, "unexpected WHO_AM_I=0x%02x\n", value);
	mpu = kzalloc(sizeof(*mpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;
	mutex_init(&mpu->lock);
	kref_init(&mpu->refs);
	mpu->client = client;
	i2c_set_clientdata(client, mpu);
	ret = car_mpu6050_configure(client);
	if (ret)
		goto fail;
	ret = sysfs_create_groups(&client->dev.kobj, car_mpu6050_groups);
	if (ret)
		goto fail;
	mpu->misc.minor = MISC_DYNAMIC_MINOR;
	mpu->misc.name = "car_mpu6050";
	mpu->misc.fops = &car_mpu6050_fops;
	mpu->misc.parent = &client->dev;
	mpu->misc.mode = 0444;
	ret = misc_register(&mpu->misc);
	if (ret) {
		sysfs_remove_groups(&client->dev.kobj, car_mpu6050_groups);
		goto fail;
	}
	dev_info(&client->dev, "WHO_AM_I=0x68 address=0x%02x run_id=%s\n", client->addr, run_id);
	dev_info(&client->dev, "six-axis ready: +/-2g +/-250dps internal=100Hz read=24 bytes\n");
	return 0;
fail:
	car_mpu6050_write_reg(client, MPU6050_PWR_MGMT_1, 0x40);
	i2c_set_clientdata(client, NULL);
	kref_put(&mpu->refs, car_mpu6050_free);
	return dev_err_probe(&client->dev, ret, "six-axis initialization failed\n");
}

static void car_mpu6050_remove(struct i2c_client *client)
{
	struct car_mpu6050 *mpu = i2c_get_clientdata(client);

	/* Stop new opens, drain sysfs, wait for current transfer, then detach. */
	misc_deregister(&mpu->misc);
	sysfs_remove_groups(&client->dev.kobj, car_mpu6050_groups);
	mutex_lock(&mpu->lock);
	car_mpu6050_write_reg(client, MPU6050_PWR_MGMT_1, 0x40);
	mpu->client = NULL;
	mutex_unlock(&mpu->lock);
	i2c_set_clientdata(client, NULL);
	kref_put(&mpu->refs, car_mpu6050_free);
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
	.remove = car_mpu6050_remove,
};
module_i2c_driver(car_mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Car MPU6050 six-axis I2C client and misc read interface (Linux 6.1)");
