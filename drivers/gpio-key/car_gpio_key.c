// SPDX-License-Identifier: GPL-2.0
/* One GPIO key: device tree -> IRQ -> debounce work -> Linux input. */
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

/* A test script only unloads the instance whose read-only token it supplied. */
static char *run_id = "manual";
module_param(run_id, charp, 0444);
MODULE_PARM_DESC(run_id, "Identifier for ownership-safe load/unload testing");

struct car_gpio_key {
	struct device *dev;
	struct gpio_desc *button;
	struct input_dev *input;
	struct delayed_work debounce_work;
	unsigned int keycode;
	unsigned int debounce_ms;
	int last_state;
};

static void car_key_sample(struct work_struct *work)
{
	struct car_gpio_key *key = container_of(to_delayed_work(work),
					      struct car_gpio_key, debounce_work);
	int state;

	/* Workqueue context may sleep; never perform this read in the hard IRQ. */
	state = gpiod_get_value_cansleep(key->button);
	if (state < 0) {
		dev_err_ratelimited(key->dev, "GPIO read failed: %d\n", state);
		return;
	}

	/* Descriptor reads apply GPIO_ACTIVE_LOW: 1 means pressed. */
	state = !!state;
	if (state == key->last_state)
		return;

	key->last_state = state;
	input_report_key(key->input, key->keycode, state);
	input_sync(key->input);
}

static irqreturn_t car_key_irq(int irq, void *data)
{
	struct car_gpio_key *key = data;

	/* Each edge moves the sample time, so bounce edges share one work item. */
	mod_delayed_work(system_wq, &key->debounce_work,
			 msecs_to_jiffies(key->debounce_ms));
	return IRQ_HANDLED;
}

static void car_key_cancel_work(void *data)
{
	struct car_gpio_key *key = data;

	cancel_delayed_work_sync(&key->debounce_work);
}

static int car_key_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct car_gpio_key *key;
	int irq;
	int ret;

	key = devm_kzalloc(dev, sizeof(*key), GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	key->dev = dev;
	key->last_state = -1;
	key->keycode = KEY_CAMERA;
	key->debounce_ms = 20;

	if (device_property_present(dev, "linux,code")) {
		ret = device_property_read_u32(dev, "linux,code", &key->keycode);
		if (ret)
			return dev_err_probe(dev, ret, "invalid linux,code\n");
	}
	if (key->keycode == KEY_RESERVED || key->keycode > KEY_MAX)
		return dev_err_probe(dev, -EINVAL, "keycode out of range\n");

	if (device_property_present(dev, "debounce-interval")) {
		ret = device_property_read_u32(dev, "debounce-interval",
					      &key->debounce_ms);
		if (ret)
			return dev_err_probe(dev, ret, "invalid debounce-interval\n");
	}
	if (!key->debounce_ms || key->debounce_ms > 1000)
		return dev_err_probe(dev, -EINVAL,
				     "debounce-interval must be 1..1000 ms\n");

	/* "button" maps to the button-gpios property in our device tree node. */
	key->button = devm_gpiod_get(dev, "button", GPIOD_IN);
	if (IS_ERR(key->button))
		return dev_err_probe(dev, PTR_ERR(key->button),
				     "cannot obtain button GPIO\n");

	irq = gpiod_to_irq(key->button);
	if (irq < 0)
		return dev_err_probe(dev, irq, "cannot map GPIO to IRQ\n");

	key->input = devm_input_allocate_device(dev);
	if (!key->input)
		return -ENOMEM;
	key->input->name = "car-gpio-key";
	key->input->id.bustype = BUS_HOST;
	input_set_capability(key->input, EV_KEY, key->keycode);
	/* EV_REP is intentionally absent: holding the key does not auto-repeat. */
	ret = input_register_device(key->input);
	if (ret)
		return dev_err_probe(dev, ret, "cannot register input device\n");

	INIT_DELAYED_WORK(&key->debounce_work, car_key_sample);
	/* devres unwinds in reverse: free IRQ, cancel work, unregister input. */
	ret = devm_add_action_or_reset(dev, car_key_cancel_work, key);
	if (ret)
		return ret;
	ret = devm_request_irq(dev, irq, car_key_irq,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			       dev_name(dev), key);
	if (ret)
		return dev_err_probe(dev, ret, "cannot request button IRQ\n");

	mod_delayed_work(system_wq, &key->debounce_work,
			 msecs_to_jiffies(key->debounce_ms));
	dev_info(dev, "ready: keycode=%u debounce=%u ms irq=%d run_id=%s\n",
		 key->keycode, key->debounce_ms, irq, run_id);
	return 0;
}

static const struct of_device_id car_key_of_match[] = {
	{ .compatible = "saberddx,car-gpio-key" },
	{ }
};
MODULE_DEVICE_TABLE(of, car_key_of_match);

static struct platform_driver car_key_driver = {
	.probe = car_key_probe,
	.driver = {
		.name = "car-gpio-key",
		.of_match_table = car_key_of_match,
	},
};
module_platform_driver(car_key_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO camera key with IRQ debounce and input events");
