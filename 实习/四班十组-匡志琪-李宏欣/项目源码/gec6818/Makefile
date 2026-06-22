CC=arm-linux-gcc

CPPFLAGS+=-I ./inc
CPPFLAGS+= -I ./inc/libxml
LDFLAGS+=-L ./lib

LDFLAGS+=-lxml2
LDFLAGS+= -lz  -pthread

#后续把开发板相关的模块文件如: lcd.c lcd.h event.c event.h 放到当前目录来
#在目录文件后继续添加.c文件即可
#如： 
#  voicectl:voicectl.c common.c lcd.c event.c sensor.c
voicectl:voicectl.c common.c lcd.c event.c sensor.c
	$(CC) $^ -o $@ $(CPPFLAGS) $(LDFLAGS)

clean:
	rm voicectl -rf
