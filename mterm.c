#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <gfxprim.h>

#include "mt-common.h"
#include "mt-sbuf.h"
#include "mt-parser.h"
#include "mt-screen.h"

static struct {
	char r;
	char g;
	char b;
} RGB_colors[16] = {
	/* BLACK */
	{0x00, 0x00, 0x00},
	/* RED */
	{0xcd, 0x00, 0x00},
	/* GREEN */
	{0x00, 0xcd, 0x00},
	/* YELLOW */
	{0xcd, 0xcd, 0x00},
	/* BLUE */
	{0x00, 0x00, 0xee},
	/* MAGENTA */
	{0xcd, 0x00, 0xcd},
	/* CYAN */
	{0x00, 0xcd, 0xcd},
	/* GRAY */
	{0xe5, 0xe5, 0xe5},

	/* BRIGHT BLACK */
	{0x7f, 0x7f, 0x7f},
	/* BRIGHT RED */
	{0xff, 0x00, 0x00},
	/* BRIGHT GREEN */
	{0x00, 0xff, 0x00},
	/* BRIGHT YELLOW */
	{0xff, 0xff, 0x00},
	/* BRIGHT BLUE */
	{0x5c, 0x5c, 0xff},
	/* BRIGHT MAGENTA */
	{0xff, 0x00, 0xff},
	/* BRIGHT CYAN */
	{0x00, 0xff, 0xff},
	/* WHITE */
	{0xff, 0xff, 0xff},
};

static gp_pixel colors[16];

static gp_pixel bg_col(const struct mt_char *c)
{
	return colors[mt_char_bg_col(c)];
}

static gp_pixel fg_col(const struct mt_char *c)
{
	return colors[mt_char_fg_col(c) + (mt_char_bold(c) ? 8 : 0)];
}

static struct mt_parser parser;
static gp_size cell_w, cell_h;
static int cols = 80, rows = 25;
static gp_text_style style = GP_DEFAULT_TEXT_STYLE;
static gp_backend *win;

static struct mt_damage damage;
static struct mt_sbuf *sbuf;

#define FONT_DEF gp_font_haxor_narrow_17
#define FONT_BOLD gp_font_haxor_narrow_bold_17

static void draw_char(struct mt_char *c, gp_coord col, gp_coord row)
{
	gp_coord sx = col * cell_w;
	gp_coord sy = row * cell_h;

	if (mt_char_bold(c))
		style.font = FONT_BOLD;
	else
		style.font = FONT_DEF;

	gp_pixel bg = bg_col(c);
	gp_pixel fg = fg_col(c);

	if (c->reverse)
		MT_SWAP(bg, fg);

	gp_fill_rect_xyxy(win->pixmap, sx, sy, sx + cell_w-1, sy + cell_h-1, bg);
	gp_print(win->pixmap, &style, sx, sy, GP_VALIGN_BELOW | GP_ALIGN_RIGHT,
		 fg, 0, "%c", mt_char_c(c));
}

static void update_region(mt_coord s_col, mt_coord e_col,
                          mt_coord s_row, mt_coord e_row)
{
	gp_coord sx = s_col * cell_w;
	gp_coord sy = s_row * cell_h;
	gp_coord ex = e_col * cell_w - 1;
	gp_coord ey = e_row * cell_h - 1;

	gp_backend_update_rect_xyxy(win, sx, sy, ex, ey);
}

static void redraw_region(mt_coord s_row, mt_coord e_row,
                          mt_coord s_col, mt_coord e_col)
{
	mt_coord col, row;

	for (row = s_row; row < e_row; row++) {
		struct mt_char *c = mt_sbuf_row(parser.sbuf, row);
		for (col = s_col; col < e_col; col++)
			draw_char(&c[col], col, row);
	}
}

static void do_damage(void)
{
	redraw_region(damage.s_row, damage.e_row, damage.s_col, damage.e_col);
	update_region(damage.s_col, damage.e_col, damage.s_row, damage.e_row);
}

static void do_scroll(int lines)
{
	gp_coord mid_y = lines * cell_h;
	gp_coord end_y = gp_pixmap_h(win->pixmap) - 1;
	gp_coord end_x = gp_pixmap_w(win->pixmap) - 1;

	if (mid_y >= end_y)
		return;

	gp_pixel bg = bg_col(mt_sbuf_cur_char(sbuf));

	gp_blit_xyxy(win->pixmap, 0, mid_y, end_x, end_y, win->pixmap, 0, 0);
	gp_fill_rect_xyxy(win->pixmap, 0, end_y - mid_y, end_x, end_y, bg);
	gp_backend_flip(win);
}

static void cursor(mt_coord col, mt_coord row, uint8_t set)
{
	struct mt_char *c;

	if (col > 0 && row > 0) {
		c = mt_sbuf_char(parser.sbuf, col, row);
		draw_char(c, col, row);
		update_region(col, col+1, row, row+1);
	}
}

static void erase(mt_coord s_col, mt_coord s_row, mt_coord e_col, mt_coord e_row)
{
	gp_coord sx1 = s_col * cell_w;
	gp_coord sy1 = s_row * cell_h;
	gp_coord sx2 = (e_col+1) * cell_w;
	gp_coord sy2 = (e_row+1) * cell_h;
	gp_pixel bg = bg_col(mt_sbuf_cur_char(sbuf));

	gp_fill_rect_xyxy(win->pixmap, sx1, sy1, sx2-1, sy2-1, bg);
	gp_backend_update_rect(win, sx1, sy1, sx2-1, sy2-1);
}

static struct mt_screen screen = {
	.damage = mt_damage_merge,
	.cursor = cursor,
	.erase = erase,
	.scroll = mt_damage_scroll,
	.priv = &damage,
};

void init_mterm(void)
{
	char buf[1024];

	sbuf = mt_sbuf_alloc();
	if (!sbuf)
		MT_ERROR_MALLOC;

	if (mt_sbuf_resize(sbuf, cols, rows))
		MT_ERROR_MALLOC;

	sbuf->screen = &screen;

	mt_parser_init(&parser, sbuf, 7, 0);
}

static void vt_read(int fd)
{
	char buf[1024];
	int ret, i;

	mt_damage_reset(&damage);

	for (i = 0; i < 100; i++) {
		ret = read(fd, buf, sizeof(buf));

		if (ret < 0 && errno == EAGAIN)
			break;

		/* shell called exit() */
		if (ret < 0 && errno == EIO)
			exit(0);

		if (ret > 0)
			mt_parse(&parser, buf, ret);
	}

	if (damage.scroll)
		do_scroll(damage.scroll);

	if (damage.s_col >= 0)
		do_damage();
}

static void vt_write(int fd, char *buf, int buf_len)
{
	int ret;

	ret = write(fd, buf, buf_len);
	if (ret < 0)
		fprintf(stderr, "WRITE %s\n", strerror(errno));
}

static void vt_putc(int fd, char c)
{
	vt_write(fd, &c, 1);
}

static void event_to_vt(gp_event *ev, int fd)
{
	int ctrl = gp_event_get_key(ev, GP_KEY_RIGHT_CTRL) ||
		   gp_event_get_key(ev, GP_KEY_LEFT_CTRL);

	if (ctrl) {
		switch (ev->val.key.ascii) {
		case ' ': /* Ctrl + space -> NUL */
			vt_putc(fd, 0x00);
		break;
		case '\t': /* Ctrl + tab -> HT */
			vt_putc(fd, 0x09);
		break;
		case '\n': /* Ctrl + \n -> CR */
			vt_putc(fd, '\r');
		break;
		/* Ctrl + a-z are mapped to various control codes */
		case 'a' ... 'z':
			vt_putc(fd, ev->val.key.ascii - 'a' + 1);
		break;
		case '\\': /* Ctrl + \ -> FS */
			vt_putc(fd, 0x1c);
		break;
		case '`': /* Ctrl + ` -> RS */
			vt_putc(fd, 0x1e);
		break;
		case '/': /* Ctrl + / -> US */
			vt_putc(fd, 0x1f);
		break;
		}

		return;
	}

	if (ev->val.key.ascii) {
		vt_putc(fd, ev->val.key.ascii);
		return;
	}

	switch (ev->val.key.key) {
	case GP_KEY_UP:
		vt_write(fd, "\eOA", 3);
	break;
	case GP_KEY_DOWN:
		vt_write(fd, "\eOB", 3);
	break;
	case GP_KEY_RIGHT:
		vt_write(fd, "\eOC", 3);
	break;
	case GP_KEY_LEFT:
		vt_write(fd, "\eOD", 3);
	break;
	case GP_KEY_DELETE:
		vt_write(fd, "\e[3~", 4);
	break;
	case GP_KEY_PAGE_UP:
		vt_write(fd, "\e[5~", 4);
	break;
	case GP_KEY_PAGE_DOWN:
		vt_write(fd, "\e[6~", 4);
	break;
	case GP_KEY_HOME:
		vt_write(fd, "\e[7~", 4);
	break;
	case GP_KEY_END:
		vt_write(fd, "\e[8~", 4);
	break;
	case GP_KEY_F1:
		vt_write(fd, "\e[11~", 5);
	break;
	case GP_KEY_F2:
		vt_write(fd, "\e[12~", 5);
	break;
	case GP_KEY_F3:
		vt_write(fd, "\e[13~", 5);
	break;
	case GP_KEY_F4:
		vt_write(fd, "\e[14~", 5);
	break;
	case GP_KEY_F5:
		vt_write(fd, "\e[15~", 5);
	break;
	case GP_KEY_F6:
		vt_write(fd, "\e[17~", 5);
	break;
	case GP_KEY_F7:
		vt_write(fd, "\e[18~", 5);
	break;
	case GP_KEY_F8:
		vt_write(fd, "\e[19~", 5);
	break;
	case GP_KEY_F9:
		vt_write(fd, "\e[20~", 5);
	break;
	case GP_KEY_F10:
		vt_write(fd, "\e[21~", 5);
	break;
	case GP_KEY_F11:
		vt_write(fd, "\e[23~", 5);
	break;
	case GP_KEY_F12:
		vt_write(fd, "\e[24~", 5);
	break;
	}
}

#ifdef MT_RESIZE
static void resize(int fd, gp_event *ev)
{
	struct winsize w;

	cols = ev->val.sys.w/cell_w;
	rows = ev->val.sys.h/cell_h;

	w.ws_col = cols;
	w.ws_row = rows;

	if (ioctl(fd, TIOCSWINSZ, &w) < 0)
		fprintf(stderr, "ioctl(fd, TIOCSWINSZ, ...) failed\n");

	gp_backend_resize_ack(win);

	mt_sbuf_resize(parser.sbuf, cols, rows);

	gp_fill(win->pixmap, bg_col(mt_sbuf_cur_char(parser.sbuf)));
	redraw_region(0, rows, 0, cols);
	gp_backend_flip(win);
}
#endif

#define TERM "/bin/bash"

int run_vt_shell(void)
{
	int fd, pid, flags;

	pid = forkpty(&fd, NULL, NULL, NULL);
	if (pid < 0) {
		fprintf(stderr, "Fork failed!\n");
		exit(1);
	}

	if (pid == 0) {
		putenv("TERM=xterm");
		execl(TERM, TERM, NULL);
	}

	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	return fd;
}

void init_graphics(void)
{
	gp_size w, h;

	style.font = FONT_DEF;

	cell_h = gp_font_height(style.font);
	cell_w = gp_font_max_width(style.font);

	w = cell_w * 80;
	h = cell_h * 25;

	win = gp_x11_init(NULL, 0, 0, w, h, "term", 0);
	if (!win) {
		fprintf(stderr, "Can't initialize backend!\n");
		exit(1);
	}

	int i;
	for (i = 0; i < 16; i++) {
		colors[i] = gp_rgb_to_pixmap_pixel(RGB_colors[i].r,
		                                   RGB_colors[i].g,
		                                   RGB_colors[i].b,
		                                   win->pixmap);
	}

	gp_fill(win->pixmap, colors[0]);
	gp_backend_flip(win);
}

int main(void)
{
	gp_event ev;
	int fd;

	fd = run_vt_shell();
	init_mterm();
	init_graphics();

	for (;;) {
		while (gp_backend_poll_event(win, &ev)) {
			switch (ev.type) {
			case GP_EV_KEY:
				if (ev.code == GP_EV_KEY_DOWN)
					event_to_vt(&ev, fd);
			break;
#ifdef MT_RESIZE
			case GP_EV_SYS:
				if (ev.code == GP_EV_SYS_RESIZE)
					resize(fd, &ev);
			break;
#endif
			}
		}

		vt_read(fd);

		usleep(100);
	}

out:
	gp_backend_exit(win);
	return 0;
}
