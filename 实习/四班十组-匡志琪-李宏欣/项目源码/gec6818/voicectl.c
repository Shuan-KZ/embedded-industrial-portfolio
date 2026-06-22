#include "common.h"
#include "lcd.h"
#include "event.h"
#include "sensor.h"

/* ========== 老师 cmd.bnf 语音指令ID ========== */
#define CMD_OPEN_WINDOW    1  /* 开窗 */
#define CMD_OPEN_DOOR      2  /* 开门 */
#define CMD_LIGHT_ON       3  /* 开灯 */
#define CMD_CLOSE_WINDOW   4  /* 关窗 */
#define CMD_CLOSE_DOOR     5  /* 关门 */
#define CMD_LIGHT_OFF      6  /* 关灯 */
#define CMD_PREV_PIC       7  /* 上一张 */
#define CMD_SING           8  /* 唱首歌给我听 */
#define CMD_HELLO         20  /* 你好 */
#define CMD_XIAOLEI      100  /* 小雷 / 小雷小雷 */
#define CMD_EXIT         999  /* 退出 / 不玩了 */

/* ========== 录音 / 图片 / 传感器查询指令 ========== */
#define REC_CMD   "arecord -d3 -c1 -r16000 -traw -fS16_LE cmd.pcm"
#define PCM_FILE  "./cmd.pcm"
#define PIC_NUM   4

/* 屏幕显示模式 */
enum screen_mode {
	MODE_PHOTO  = 0,
	MODE_SENSOR = 1
};

/* ========== 应用上下文 ========== */
typedef struct app_context {
	int  sockfd_tcp;
	int  current_pic;
	int  running;
	int  gy39_fd;
	int  us100_fd;
	int  sr501_fd;
	int  mode;           /* MODE_PHOTO / MODE_SENSOR */
	int  auto_slide;     /* 1=自动轮播 */
	time_t last_touch;   /* 上次触摸/语音时间 */
	pthread_mutex_t lock;
	const char *bmps[PIC_NUM];
} app_context_t;

/* ========== SIGPIPE 处理 ========== */
static void catch_sigpipe(int sig)
{
	if (sig == SIGPIPE) {
		printf("killed by SIGPIPE\n");
		exit(0);
	}
}

/* ========== 欢迎界面 ========== */
static void show_welcome()
{
	lcd_draw_background(0xFFFFFF);

	/* 顶部蓝色标题栏 */
	for (int y = 30; y < 100; y++)
		for (int x = 50; x < 750; x++)
			lcd_draw_point(x, y, 0x1565C0);

	/* 中间装饰横条 */
	for (int y = 180; y < 200; y++)
		for (int x = 150; x < 650; x++)
			lcd_draw_point(x, y, 0x0D47A1);

	for (int y = 300; y < 320; y++)
		for (int x = 150; x < 650; x++)
			lcd_draw_point(x, y, 0x0D47A1);

	printf("\n========================================\n");
	printf("       智能语音交互系统\n");
	printf("  组员: 李宏欣  匡志琪\n");
	printf("  四班十组 | GEC6818 开发板\n");
	printf("========================================\n\n");

	sleep(2);
}

/* ========== 显示当前图片(调用者持有锁) ========== */
static void show_picture(app_context_t *ctx)
{
	lcd_draw_bmp(0, 0, (char *)ctx->bmps[ctx->current_pic]);
	printf("当前图片: %s\n", ctx->bmps[ctx->current_pic]);
}

/* 带锁的图片显示(外部调用) */
static void show_picture_safe(app_context_t *ctx)
{
	pthread_mutex_lock(&ctx->lock);
	show_picture(ctx);
	pthread_mutex_unlock(&ctx->lock);
}

/* 带锁的传感器面板显示(外部调用) */
static void show_sensor_panel_safe(app_context_t *ctx)
{
	pthread_mutex_lock(&ctx->lock);
	show_sensor_panel(ctx);
	pthread_mutex_unlock(&ctx->lock);
}

/* ========== 图片切换(线程安全) ========== */
static void switch_picture(app_context_t *ctx, int step)
{
	pthread_mutex_lock(&ctx->lock);
	ctx->current_pic = (ctx->current_pic + step + PIC_NUM) % PIC_NUM;
	ctx->auto_slide = 0;
	ctx->last_touch = time(NULL);
	ctx->mode = MODE_PHOTO;
	show_picture(ctx);
	pthread_mutex_unlock(&ctx->lock);
}

/* ========== LCD 显示传感器数据面板(调用者持有锁) ========== */
static void show_sensor_panel(app_context_t *ctx)
{
	lcd_draw_background(0xFFFFFF);
	ctx->mode = MODE_SENSOR;
	ctx->auto_slide = 0;
	ctx->last_touch = time(NULL);

	/* 顶部绿色标题栏 */
	for (int y = 10; y < 50; y++)
		for (int x = 200; x < 600; x++)
			lcd_draw_point(x, y, 0x00695C);

	printf("\n========== 传感器数据面板 ==========\n");

	/* GY39 光照 */
	if (ctx->gy39_fd != -1) {
		int light = gy39_get_light(ctx->gy39_fd);
		if (light >= 0) {
			printf("  光照: %d lux\n", light);
			lcd_draw_num(light, 750, 80, 16, 25, 0xFF6600);
		} else {
			printf("  光照: 读取失败\n");
		}
	}

	/* GY39 温湿度+气压+海拔 */
	if (ctx->gy39_fd != -1) {
		gy39_env_data_t env;
		if (gy39_get_env(ctx->gy39_fd, &env) == 0) {
			printf("  温度: %d C, 湿度: %d %%\n", env.temperature, env.humidity);
			printf("  气压: %d Pa, 海拔: %d m\n", env.pressure, env.altitude);
			/* LCD 显示温湿度 */
			lcd_draw_num(env.temperature, 300, 160, 16, 25, 0xFF0000);
			lcd_draw_num(env.humidity, 300, 220, 16, 25, 0x0000FF);
		}
	}

	/* US-100 超声波测距 */
	if (ctx->us100_fd != -1) {
		int dist = us100_get_distance(ctx->us100_fd);
		if (dist >= 0) {
			printf("  距离: %d mm\n", dist);
			lcd_draw_num(dist, 750, 160, 16, 25, 0x009688);
		}
	}

	/* SR501 红外人体感应 */
	if (ctx->sr501_fd != -1) {
		int ir = sr501_get_status(ctx->sr501_fd);
		printf("  红外: %s\n", ir == 1 ? "有人" : "无人");
		/* LCD 显示: 有人=红色圆 无人=灰色圆 */
		unsigned int ir_color = (ir == 1) ? 0xFF0000 : 0x888888;
		for (int dy = -20; dy <= 20; dy++)
			for (int dx = -20; dx <= 20; dx++)
				if (dx*dx + dy*dy <= 400)
					lcd_draw_point(700 + dx, 300 + dy, ir_color);
	}

	printf("======================================\n");
}

/* ========== 显示传感器数据面板(调用者持有锁) — 前向声明 ========== */
static void show_sensor_panel(app_context_t *ctx);

/* ========== 恢复到指令前的视图(调用者不持有锁) ========== */
static void restore_view(app_context_t *ctx, int force_photo)
{
	int saved;
	pthread_mutex_lock(&ctx->lock);
	ctx->last_touch = time(NULL);
	ctx->auto_slide = 0;
	if (force_photo)
		ctx->mode = MODE_PHOTO;
	saved = ctx->mode;
	pthread_mutex_unlock(&ctx->lock);
	if (saved == MODE_SENSOR)
		show_sensor_panel_safe(ctx);
	else
		show_picture_safe(ctx);
}

/* ========== 语音指令分发(核心) ========== */
static void dispatch_command(app_context_t *ctx, int cmd)
{
	switch (cmd) {

	case CMD_OPEN_WINDOW:   /* 开窗 */
		printf("  → [执行] 开窗\n");
		lcd_draw_background(0x87CEEB);
		usleep(800000);
		restore_view(ctx, 0);
		break;

	case CMD_OPEN_DOOR:     /* 开门 */
		printf("  → [执行] 开门\n");
		lcd_draw_background(0xFFD700);
		usleep(800000);
		restore_view(ctx, 0);
		break;

	case CMD_LIGHT_ON:      /* 开灯 */
		printf("  → [执行] 开灯 (屏幕全白)\n");
		lcd_draw_background(0xFFFFFF);
		usleep(600000);
		restore_view(ctx, 1);
		break;

	case CMD_CLOSE_WINDOW:  /* 关窗 */
		printf("  → [执行] 关窗\n");
		lcd_draw_background(0xB0C4DE);
		usleep(800000);
		restore_view(ctx, 0);
		break;

	case CMD_CLOSE_DOOR:    /* 关门 */
		printf("  → [执行] 关门\n");
		lcd_draw_background(0xDAA520);
		usleep(800000);
		restore_view(ctx, 0);
		break;

	case CMD_LIGHT_OFF:     /* 关灯 */
		printf("  → [执行] 关灯 (屏幕变暗)\n");
		lcd_draw_background(0x222222);
		usleep(500000);
		restore_view(ctx, 1);
		break;

	case CMD_PREV_PIC:      /* 上一张 */
		printf("  → [执行] 上一张\n");
		switch_picture(ctx, -1);
		break;

	case CMD_SING:          /* 唱首歌给我听 */
		printf("  → [执行] 播放音乐\n");
		{
			unsigned int clr[] = {0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF};
			for (int i = 0; i < 6; i++) {
				lcd_draw_background(clr[i]);
				usleep(350000);
			}
		}
		restore_view(ctx, 0);
		break;

	case CMD_HELLO:         /* 你好 */
		printf("  → [执行] 你好!\n");
		lcd_draw_background(0xE8F5E9);
		usleep(800000);
		restore_view(ctx, 0);
		break;

	case CMD_XIAOLEI:       /* 小雷 唤醒词 */
		printf("  → [唤醒] 小雷 → 我在!\n");
		lcd_draw_background(0xFFF9C4);
		usleep(250000);
		lcd_draw_background(0xFFFFFF);
		usleep(250000);
		lcd_draw_background(0xFFF9C4);
		usleep(250000);
		restore_view(ctx, 0);
		break;

	case CMD_EXIT:          /* 退出 / 不玩了 */
		printf("  → [执行] 退出程序\n");
		ctx->running = 0;
		break;

	default:
		printf("  → [忽略] 未支持的指令 ID=%d\n", cmd);
		break;
	}
}

/* ========== 触摸线程 ========== */
static void *touch_thread(void *arg)
{
	app_context_t *ctx = (app_context_t *)arg;
	int dir;

	while (ctx->running) {
		dir = get_event_direction();

		pthread_mutex_lock(&ctx->lock);

		switch (dir) {
		case LEFT:   /* 左滑 → 下一张 */
			ctx->current_pic = (ctx->current_pic + 1) % PIC_NUM;
			ctx->auto_slide = 0;
			ctx->last_touch = time(NULL);
			ctx->mode = MODE_PHOTO;
			show_picture(ctx);
			break;
		case RIGHT:  /* 右滑 → 上一张 */
			ctx->current_pic = (ctx->current_pic + PIC_NUM - 1) % PIC_NUM;
			ctx->auto_slide = 0;
			ctx->last_touch = time(NULL);
			ctx->mode = MODE_PHOTO;
			show_picture(ctx);
			break;
		case UP:     /* 上滑 → 传感器面板 */
			show_sensor_panel(ctx);
			break;
		case DOWN:   /* 下滑 → 回到图片模式 */
			ctx->mode = MODE_PHOTO;
			ctx->auto_slide = 0;
			ctx->last_touch = time(NULL);
			show_picture(ctx);
			break;
		default:
			break;
		}

		pthread_mutex_unlock(&ctx->lock);
	}

	return NULL;
}

/* ========== 语音线程 ========== */
static void *voice_thread(void *arg)
{
	app_context_t *ctx = (app_context_t *)arg;
	int id_num, ret;
	xmlChar *id;
	struct stat st;

	while (ctx->running) {
		printf("\n[语音] 等待 3 秒录音...\n");
		ret = system(REC_CMD);
		if (ret != 0) {
			printf("[语音] 录音失败 (arecord 返回 %d), 跳过\n", ret);
			continue;
		}

		if (stat(PCM_FILE, &st) == -1 || st.st_size < 100) {
			printf("[语音] PCM 文件无效, 跳过\n");
			continue;
		}

		send_pcm(ctx->sockfd_tcp, PCM_FILE);
		id = wait4id(ctx->sockfd_tcp);
		if (id == NULL) {
			printf("[语音] 未识别到有效指令\n");
			continue;
		}

		id_num = atoi((char *)id);
		xmlFree(id);

		printf("[语音] 识别 ID = %d\n", id_num);
		dispatch_command(ctx, id_num);
	}

	return NULL;
}

/* ========== 自动轮播检测 ========== */
static void check_auto_slide(app_context_t *ctx)
{
	pthread_mutex_lock(&ctx->lock);
	if (ctx->auto_slide && ctx->mode == MODE_PHOTO
	    && difftime(time(NULL), ctx->last_touch) >= 3) {
		ctx->current_pic = (ctx->current_pic + 1) % PIC_NUM;
		ctx->last_touch = time(NULL);
		show_picture(ctx);
	}
	pthread_mutex_unlock(&ctx->lock);
}

/* =====================================================================
   主函数
   ===================================================================== */
int main(int argc, char const *argv[])
{
	app_context_t ctx;
	pthread_t tid_touch;
	pthread_t tid_voice;

	if (argc != 2) {
		printf("用法: %s <Ubuntu-IP>\n", argv[0]);
		printf("示例: %s 192.168.2.10\n", argv[0]);
		exit(0);
	}

	printf("\n"
	       "========================================\n"
	       "     智能语音交互系统  v2.0\n"
	       "     组员: 李宏欣  匡志琪\n"
	       "     四班十组 | GEC6818\n"
	       "========================================\n\n");

	/* ---- 初始化 ---- */
	memset(&ctx, 0, sizeof(ctx));
	signal(SIGPIPE, catch_sigpipe);

	/* 连接语音识别服务器 */
	ctx.sockfd_tcp = init_tcp_socket(argv[1]);

	/* 传感器 */
	ctx.gy39_fd  = uart_open(GY39_UART, B9600);
	ctx.us100_fd = uart_open(US100_UART, B9600);
	ctx.sr501_fd = sr501_open();

	/* 图片列表 */
	ctx.bmps[0] = "1.bmp";
	ctx.bmps[1] = "2.bmp";
	ctx.bmps[2] = "3.bmp";
	ctx.bmps[3] = "4.bmp";
	ctx.current_pic = 0;

	/* 运行状态 */
	ctx.running    = 1;
	ctx.mode       = MODE_PHOTO;
	ctx.auto_slide = 1;           /* 启动后自动轮播 */
	ctx.last_touch = time(NULL);

	pthread_mutex_init(&ctx.lock, NULL);

	/* LCD 初始化 + 触摸屏 + 欢迎界面 + 第一张图 */
	lcd_init();
	event_init();
	show_welcome();

	pthread_mutex_lock(&ctx.lock);
	show_picture(&ctx);
	pthread_mutex_unlock(&ctx.lock);

	/* 启动双线程 */
	pthread_create(&tid_touch, NULL, touch_thread, &ctx);
	pthread_create(&tid_voice, NULL, voice_thread, &ctx);

	/* 主循环：自动轮播监测 */
	while (ctx.running) {
		check_auto_slide(&ctx);
		usleep(500000);   /* 每 0.5 秒检测一次 */
	}

	/* ---- 退出清理 ---- */
	printf("\n正在退出系统...\n");

	pthread_join(tid_voice, NULL);
	ctx.running = 0;
	pthread_cancel(tid_touch);
	pthread_join(tid_touch, NULL);

	if (ctx.gy39_fd  != -1) close(ctx.gy39_fd);
	if (ctx.us100_fd != -1) close(ctx.us100_fd);
	if (ctx.sr501_fd != -1) sr501_close(ctx.sr501_fd);
	close(ctx.sockfd_tcp);
	lcd_uninit();
	event_deinit();
	pthread_mutex_destroy(&ctx.lock);

	printf("再见!\n");
	return 0;
}
