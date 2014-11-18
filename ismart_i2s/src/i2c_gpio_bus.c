/******************************************************************
** 文件名: i2c_gpio_bus.c
** Copyright (c) 2014 深圳果谷智能科技有限公司
** 创建人:邓俊勇
** 日  期:2014.11.06
** 描  述:gpio模拟i2c总线,用于wm8918控制
** 版  本: 1.0
**-----------------------------------------------------------------------------
** 函  数:
**修改记录:
******************************************************************/

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

//#include <asm/mach-atheros/atheros.h>
//#include <asm/mach-atheros/gpio.h>

#include "ar7240.h"

/* SDA连接到GPIO17*/
#define GPIO_PIN_SDA 17
/* SCL连接到GPIO1*/
#define GPIO_PIN_SCL 1

static struct i2c_pins {
	int sda_pin;
	int scl_pin;
} gpio_data;

void ar7240_gpio_out_val(int gpio, int val)
{
	if (val & 0x1) {
		ar7240_reg_rmw_set(AR7240_GPIO_OUT, (1 << gpio));
	} else {
		ar7240_reg_rmw_clear(AR7240_GPIO_OUT, (1 << gpio));
	}
}

int ar7240_gpio_in_val(int gpio)
{
	return ((1 << gpio) & (ar7240_reg_rd(AR7240_GPIO_IN)));
} 

void ar7240_gpio_config_output(int gpio)
{
	ar7240_reg_rmw_set(AR7240_GPIO_OE, (1 << gpio));
}

static inline int scl_pin(void *data)
{
	return ((struct i2c_pins*)data)->scl_pin;
}

static inline int sda_pin(void *data)
{
	return ((struct i2c_pins*)data)->sda_pin;
}

static void bit_setscl(void *data, int val)
{
	ar7240_gpio_out_val(scl_pin(data), val);
}

static void bit_setsda(void *data, int val)
{
	ar7240_gpio_out_val(sda_pin(data), val);
}

static int bit_getscl(void *data)
{
	return ar7240_gpio_in_val(scl_pin(data)) != 0;
}

static int bit_getsda(void *data)
{
	return ar7240_gpio_in_val(sda_pin(data)) != 0;
}

struct i2c_algo_bit_data bit_data = {
	.data           = &gpio_data,
	.setsda         = bit_setsda,
	.setscl         = bit_setscl,
	.getsda         = bit_getsda,
	.getscl         = bit_getscl,
	.udelay         = 50,
	.timeout        = HZ
//	.timeout        = 10
};
         
struct i2c_adapter i2c_gpio_adapter = {
	.name           = "AR9331 I2C Adapter",
	.class          = I2C_CLASS_HWMON | I2C_CLASS_SPD,
	.algo_data      = &bit_data, //算法algorithm
	.nr             = 5,
};

static int __init i2c_init(void)
{
//	int ret;
	int res = 0;
//	unsigned int gpio_oe_val = 0, gpio_out_val = 0, gpio_func4_val = 0;
	unsigned int gpio_oe_val = 0, gpio_out_val = 0;
	
	/*保存配置前GPIO register value */
	gpio_oe_val = ar7240_reg_rd(AR7240_GPIO_OE);
	gpio_out_val = ar7240_reg_rd(AR7240_GPIO_OUT);
//	gpio_func4_val = ar7240_reg_rd(ar7240_gpio_OUT_FUNCTION4);

	gpio_data.sda_pin = GPIO_PIN_SDA;
	gpio_data.scl_pin = GPIO_PIN_SCL;

//    ar7240_reg_wr(ar7240_gpio_OUT_FUNCTION4, (ar7240_reg_rd(ATH_GPIO_OUT_FUNCTION4) & 0xffff0000)); /*reset GPIO 16/17*/
    ar7240_gpio_config_output(gpio_data.scl_pin);
    ar7240_gpio_config_output(gpio_data.sda_pin);

	//Disables the Ethernet Switch LED data on GPIO_17
//	ret = ar7240_reg_rd(0x18040028);
//	printk("GPIO_FUN1:%08x\n", ret);
	ar7240_reg_rmw_clear(0x18040028, 0x80);
//	ret = ar7240_reg_rd(0x18040028);
//	printk("GPIO_FUN1:%08x\n", ret);

	//初始化为高电平
//	ret = ar7240_reg_rd(AR7240_GPIO_IN);
//	printk("GPIO_IN  :%08x\n", ret);
    ar7240_gpio_out_val(gpio_data.scl_pin, 1);
    ar7240_gpio_out_val(gpio_data.sda_pin, 1);
//	ret = ar7240_reg_rd(AR7240_GPIO_IN);
//	printk("GPIO_IN  :%08x\n", ret);

	if ((res = i2c_bit_add_numbered_bus(&i2c_gpio_adapter) != 0)) {
		/*恢复配置前GPIO register value */
		printk(KERN_INFO "TMP75 temprature sensor not exist.\n");
		ar7240_reg_wr(AR7240_GPIO_OE, gpio_oe_val);
		ar7240_reg_wr(AR7240_GPIO_OUT, gpio_out_val);
//		ar7240_reg_wr(ar7240_gpio_OUT_FUNCTION4, gpio_func4_val);
		return res;
	}

	return 0;
}

static void __exit i2c_exit(void)
{
	i2c_del_adapter(&i2c_gpio_adapter);
	return;
}

module_init(i2c_init);
module_exit(i2c_exit);

MODULE_AUTHOR ("Shaojun Xie<xiesj@zoomnetcom.com>");
MODULE_DESCRIPTION("AR9344 GPIO-based I2C bus driver");
MODULE_LICENSE("GPL");

