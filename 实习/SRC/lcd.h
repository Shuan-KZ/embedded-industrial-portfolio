#ifndef __LCD_H__
#define __LCD_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

//定义一个结构体来描述BMP图片的属性信息
typedef struct bmp_info
{
	int size;	//图片大小
	int width;	//图片宽度
	int height;	//图片高度
	short depth;//图片色深
	char* data; //保存像素数组的首地址
}BMP;

void open_lcd();
void close_lcd();
void write_lcd(unsigned int color);
void lcd_init();
void lcd_uninit();
void lcd_draw_point(int x, int y, unsigned int color);
void lcd_draw_background(unsigned int color);
BMP get_bmp_info(char* bmppath);
void lcd_drwa_bmp(int x, int y, char* bmppath);
void lcd_drwa_word(int x0, int y0, int w, int h, unsigned char word[], unsigned int color);
void lcd_drwa_num(int num, int x0, int y0, int w, int h, unsigned int color);

#endif