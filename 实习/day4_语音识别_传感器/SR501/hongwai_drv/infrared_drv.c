#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/gpio.h>		//函数声明
#include "cfg_type.h" 	 	//端口宏定义
#include <linux/ioport.h>	//IO相关
#include <linux/delay.h> 	//延时函数
#include <linux/time.h>

#define INFRARED_GPIO  (PAD_GPIO_D+17)//GPIOD17(RXD3)


//1、定义一个cdev
static struct cdev gec6818_infrared_cdev;

static unsigned int infrared_major=0; //自动分配设备号
static unsigned int infrared_minor=0;
static dev_t infrared_num;

static struct class *gec6818_infrared_class;
static struct device *gec6818_infrared_device;

//3、定义file_operations,并做初始化
//给应用程序的read()提供系统调用的接口
static ssize_t gec6818_infrared_read(struct file *filp, char __user *buf, 
                       size_t len, loff_t *off)
{
	int ret;
	int value;
	if(len != 4 ){
		return -EINVAL;		
	}

	value = !gpio_get_value(INFRARED_GPIO);
	ret = copy_to_user(buf, &value, len);
	if(ret != 0){
		printk("copy to user error\n");
		return -EFAULT;
	}

	value = 0;
	
	return len;
}

static const struct file_operations gec6818_infrared_fops = {
	.owner = THIS_MODULE,
	.read  = gec6818_infrared_read,
};

//驱动的入口函数--->驱动的安装函数
static int __init gec6818_infrared_init(void)
{
	int ret;
	//2、给cdev申请设备号（静态注册 or 动态分配）
	if(infrared_major == 0)
		ret = alloc_chrdev_region(&infrared_num, infrared_minor, 1, "gec6818_infrareds");
	else{
		infrared_num = MKDEV(infrared_major,infrared_minor);
		ret = register_chrdev_region(infrared_num, 1, "gec6818_infrareds");
	} 
	if(ret < 0){
		printk("can not register device number\n");
		return ret;
	}
	//4、cdev的初始化
	cdev_init(&gec6818_infrared_cdev, &gec6818_infrared_fops);

	//5、将cdev加入内核
	ret = cdev_add(&gec6818_infrared_cdev, infrared_num, 1);
	if(ret < 0){
		printk("can not add cdev\n");
		goto failed_cdev_add;
	}
	
	//6、创建class
	gec6818_infrared_class = class_create(THIS_MODULE, "gec6818_infrared_class");
	if (IS_ERR(gec6818_infrared_class)){
		printk("class create is error\n");
		ret = PTR_ERR(gec6818_infrared_class);
		goto failed_class_create;		
	}

	//7、创建device
	gec6818_infrared_device = device_create(gec6818_infrared_class, NULL,
				    infrared_num, NULL,"infrared_drv"); // /dev/infrared_drv
		if (IS_ERR(gec6818_infrared_device)) {
			printk("device create is error\n");
			ret = PTR_ERR(gec6818_infrared_device);
		goto failed_device_create;
	}
	
	gpio_free(INFRARED_GPIO);
	ret = gpio_request(INFRARED_GPIO, "infrared_gpio");
	if(ret < 0){
		printk("request infrared gpio error\n");
		goto failed_request_gpio;
	}
	gpio_direction_input(INFRARED_GPIO);
	printk("infrared driver on gec210\n");
	
	return 0;
	
failed_request_gpio:
	device_destroy(gec6818_infrared_class, infrared_num);
failed_device_create:
	class_destroy(gec6818_infrared_class);
failed_class_create:
	cdev_del(&gec6818_infrared_cdev);
failed_cdev_add:
	return ret;	
}

//驱动的出口函数--->驱动的卸载函数
static void __exit gec6818_infrared_exit(void)
{
	//相当于入口函数的反函数
	unregister_chrdev_region(infrared_num, 1);
	cdev_del(&gec6818_infrared_cdev);
	
	device_destroy(gec6818_infrared_class, infrared_num);
	class_destroy(gec6818_infrared_class);
	
	gpio_free(INFRARED_GPIO);
	
	printk("good bye\n");
}

module_init(gec6818_infrared_init);
module_exit(gec6818_infrared_exit);

//module的描述，可有可无。#modinfo infrared_drv.ko
MODULE_AUTHOR("bobeyfeng@163.com");
MODULE_DESCRIPTION("S5P6818 infrared driver");
MODULE_VERSION("V1.0");
MODULE_LICENSE("GPL"); //源码符合GPL协议