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

static void car_key_sample(struct work_struct *work)//delayed work 回调，在工作队列的进程上下文执行，可以睡眠。
{
	struct car_gpio_key *key = container_of(to_delayed_work(work),
                                             struct car_gpio_key, debounce_work);//找回当前这个按键设备自己的私有数据 key
       int state;//用来保存按键状态

	/* Workqueue context may sleep; never perform this read in the hard IRQ. */
       state = gpiod_get_value_cansleep(key->button);//读取 GPIO 逻辑状态；描述符已处理低电平有效，1 表示按下。
	if (state < 0) {
		dev_err_ratelimited(key->dev, "GPIO read failed: %d\n", state);
		return;
	}

	/* Descriptor reads apply GPIO_ACTIVE_LOW: 1 means pressed. */
       state = !!state;//布尔化（把任意非0值转变成1）
	if (state == key->last_state)
               return;//防止重复事件

	key->last_state = state;
       input_report_key(key->input, key->keycode, state);//把按键状态上报给 Input Subsystem
       input_sync(key->input);//发送 SYN_REPORT，标记这一组输入事件结束。
}

static irqreturn_t car_key_irq(int irq, void *data)//每次按键边沿触发中断，就把“20ms 后再检查 GPIO”的任务重新延后
{
	struct car_gpio_key *key = data;

	/* Each edge moves the sample time, so bounce edges share one work item. */
	mod_delayed_work(system_wq, &key->debounce_work,
			 msecs_to_jiffies(key->debounce_ms));
       return IRQ_HANDLED;//已经正确响应了这个 IRQ，并安排了后续 delayed work
}

static void car_key_cancel_work(void *data)
{
	struct car_gpio_key *key = data;

       cancel_delayed_work_sync(&key->debounce_work);//取消 key->debounce_work 这个延迟任务，并且同步等待它彻底停止
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
       //先从设备树拿到这个按键 GPIO，然后把这个 GPIO 转换成一个可用于申请中断的 IRQ 编号。
       key->button = devm_gpiod_get(dev, "button", GPIOD_IN);//从当前设备 dev 对应的设备树节点中，找到 button-gpios，获取这个 GPIO，并且把它配置成输入方向。
	if (IS_ERR(key->button))
		return dev_err_probe(dev, PTR_ERR(key->button),
				     "cannot obtain button GPIO\n");

       irq = gpiod_to_irq(key->button);//查询这个 GPIO 对应的 IRQ 编号
	if (irq < 0)
		return dev_err_probe(dev, irq, "cannot map GPIO to IRQ\n");
//把你的 GPIO 按键注册成一个 Linux input 设备 GPIO 负责获取硬件电平，Input Subsystem 负责把它抽象成标准输入事件。
       key->input = devm_input_allocate_device(dev);//为当前设备申请一个 struct input_dev 输入设备对象。
	if (!key->input)
		return -ENOMEM;
	key->input->name = "car-gpio-key";
       key->input->id.bustype = BUS_HOST;//总线类型 这是一个直接挂在主机/SoC 上的输入设备，不是 USB 键盘、蓝牙键盘之类的外部总线设备。
       input_set_capability(key->input, EV_KEY, key->keycode);//声明支持 EV_KEY 和对应按键码；keycode 可由设备树 linux,code 配置。
       //212 对应 KEY_CAMERA。
	/* EV_REP is intentionally absent: holding the key does not auto-repeat. */
	ret = input_register_device(key->input);
	if (ret)
		return dev_err_probe(dev, ret, "cannot register input device\n");

       INIT_DELAYED_WORK(&key->debounce_work, car_key_sample);//将debounce_work和car_key_sample绑定起来，以后这个 delayed work 真正执行时，就调用 car_key_sample() 初始化+绑定
	/* devres unwinds in reverse: free IRQ, cancel work, unregister input. */
       ret = devm_add_action_or_reset(dev, car_key_cancel_work, key);//给当前设备 dev 注册一个自定义清理动作
	if (ret)
		return ret;
       ret = devm_request_irq(dev, irq, car_key_irq, //给这个 GPIO 对应的 IRQ 注册一个中断处理函数，让按键电平发生上升沿或下降沿时，内核自动调用 car_key_irq()。
                              IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,//表示这个中断在两种电平变化时都触发
			       dev_name(dev), key);
	if (ret)
		return dev_err_probe(dev, ret, "cannot request button IRQ\n");

	mod_delayed_work(system_wq, &key->debounce_work,
			 msecs_to_jiffies(key->debounce_ms));
	dev_info(dev, "ready: keycode=%u debounce=%u ms irq=%d run_id=%s\n",
		 key->keycode, key->debounce_ms, irq, run_id);
	return 0;
}

static const struct of_device_id car_key_of_match[] = {//这个数组列出“我的驱动支持哪些设备树 compatible”。
	{ .compatible = "saberddx,car-gpio-key" },
       { }//数组结束标志
};
MODULE_DEVICE_TABLE(of, car_key_of_match);//把这个设备匹配信息导出到模块元数据里。of 表示 OF/设备树匹配表。
//car_key_of_match 这张表，是一张 OF/设备树匹配表，请把它导出成模块设备别名信息。
static struct platform_driver car_key_driver = {//定义 platform 驱动，关联 probe 回调、驱动名称和 OF 匹配表。
       .probe = car_key_probe,//当这个驱动和某个设备匹配成功后，调用 car_key_probe()。
	.driver = {
		.name = "car-gpio-key",
		.of_match_table = car_key_of_match,
	},
};
module_platform_driver(car_key_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO camera key with IRQ debounce and input events");
