// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2019-2022 Cyril Hrubis <metan@ucw.cz>
 */
#include <gfxprim.h>
#include <pty.h>
#include <fcntl.h>
#include <ctype.h>

#include <string.h>
#include <errno.h>

#define TERM_W 80
#define TERM_H 25

static int verbose = 0;

static gp_text_style style = GP_DEFAULT_TEXT_STYLE;
static gp_backend *win;

#ifdef COLORS
static gp_pixel bg_color = 0, fg_color = 7;
static int bold = 0;
#else
static gp_pixel bg_color = 0, fg_color = 0xffffff;
#endif

static int charset_G0 = 'B';
static int charset_G1 = '0';
static int charset_sel = 0;

static struct vt {
	int cur_x;
	int cur_y;
	int cell_w;
	int cell_h;
	int w;
	int h;
} vt;

#ifdef COLORS
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
#endif

static gp_pixel bg_col(void)
{
#ifdef COLORS
	return colors[bg_color];
#else
	return bg_color;
#endif
}

static gp_pixel fg_col(void)
{
#ifdef COLORS
	return colors[fg_color + (bold ? 8 : 0)];
#else
	return fg_color;
#endif
}

void init_graphics(void)
{
	gp_size w, h;

	style.font = gp_font_haxor_narrow_17;

	vt.cell_h = gp_font_height(style.font);
	vt.cell_w = gp_font_max_width(style.font);
	vt.w = TERM_W;
	vt.h = TERM_H;

	w = vt.cell_w * vt.w;
	h = vt.cell_h * vt.h;

	win = gp_x11_init(NULL, 0, 0, w, h, "term", 0);
	if (!win) {
		fprintf(stderr, "Can't initialize backend!\n");
		exit(1);
	}

#ifdef COLORS
	int i;
	for (i = 0; i < 16; i++) {
		colors[i] = gp_rgb_to_pixmap_pixel(RGB_colors[i].r,
		                                   RGB_colors[i].g,
		                                   RGB_colors[i].b,
		                                   win->pixmap);
	}
#endif

	gp_fill(win->pixmap, bg_col());
	gp_backend_flip(win);
}

void draw_char(char c, gp_coord x, gp_coord y)
{
	gp_coord sx = x * vt.cell_w;
	gp_coord sy = y * vt.cell_h;

	gp_fill_rect_xyxy(win->pixmap, sx, sy, sx + vt.cell_w, sy + vt.cell_h, bg_col());
	gp_print(win->pixmap, &style, sx, sy, GP_VALIGN_BELOW | GP_ALIGN_RIGHT,
		 fg_col(), 0, "%c", c);

	gp_backend_update_rect_xyxy(win, sx, sy, sx + vt.cell_w-1, sy + vt.cell_h-1);
}

void erase(int x1, int y1, int x2, int y2)
{
	gp_coord sx1 = x1 * vt.cell_w;
	gp_coord sy1 = y1 * vt.cell_h;
	gp_coord sx2 = x2 * vt.cell_w;
	gp_coord sy2 = y2 * vt.cell_h;

	gp_fill_rect_xyxy(win->pixmap, sx1, sy1, sx2, sy2, bg_col());
	gp_backend_update_rect(win, sx1, sy1, sx2-1, sy2-1);
}

void scroll(int lines)
{
	gp_coord mid_y = lines * vt.cell_h;
	gp_coord end_y = vt.h * vt.cell_h - 1;
	gp_coord end_x = vt.w * vt.cell_w - 1;

	gp_blit_xyxy(win->pixmap, 0, mid_y, end_x, end_y, win->pixmap, 0, 0);
	gp_fill_rect_xyxy(win->pixmap, 0, end_y - mid_y, end_x, end_y, bg_col());
	gp_backend_flip(win);
}

void vt_cursor_move(int x, int y)
{
	if (verbose)
		fprintf(stderr, "Cursor move %i %i\n", x, y);

	vt.cur_x += x;
	vt.cur_y += y;

	if (vt.cur_x > vt.w) {
		vt.cur_x = 0;
		vt.cur_y++;
	}

	if (vt.cur_y >= vt.h) {
		scroll(vt.cur_y - vt.h + 1);
		vt.cur_y = vt.h - 1;
	}
}

void vt_cursor_set(int x, int y)
{
	if (verbose)
		fprintf(stderr, "Cursor set %i %i\n", x, y);

	if (x >= 0 && x < vt.w) {
		vt.cur_x = x;
	}

	if (y >= 0 && y < vt.h) {
		vt.cur_y = y;
	}
}

void vt_tab(void)
{
	int rem = vt.cur_x % 8;

	if (!rem)
		return;

	vt.cur_x += 8 - rem;

	if (vt.cur_x < vt.w)
		return;

	rem = vt.cur_x % vt.w;

	vt.cur_x = rem;
	vt.cur_y++;
}

void vt_bs(void)
{
	if (!vt.cur_x)
		return;

	vt_cursor_move(-1, 0);
	erase(vt.cur_x, vt.cur_y, vt.cur_x+1, vt.cur_y+1);
}

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

void vt_putchar(char c)
{
	int charset = charset_sel ? charset_G1 : charset_G0;

	/* ASCII */
	if (charset == 'B') {
		if (isprint(c)) {
			draw_char(c, vt.cur_x, vt.cur_y);
			vt_cursor_move(1, 0);
			return;
		}

		fprintf(stderr, "Unhandled char %c %02x\n", isprint(c) ? (int)c : ' ', c);
		return;
	}

	/* Graphics */
	if (charset == '0') {
		switch (c) {
		case 'x':
			c = '|';
		break;
		case 'q':
			c = '-';
		break;
		}
		draw_char(c, vt.cur_x, vt.cur_y);
		vt_cursor_move(1, 0);
	}
}

/*
 * Clear line from cursor:
 *
 * 0 -- right (default)
 * 1 -- left
 * 2 -- whole line
 */
void vt_csi_K(int val)
{
	switch (val) {
	case 0:
		erase(vt.cur_x, vt.cur_y, vt.w, vt.cur_y + 1);
	break;
	case 1:
		erase(0, vt.cur_y, vt.cur_x, vt.cur_y + 1);
	break;
	case 2:
		erase(0, vt.cur_y, vt.w, vt.cur_y + 1);
	break;
	}
}

/*
 * Set cursor position.
 */
void vt_csi_H(int *pars, int par_cnt)
{
	if (par_cnt == 2) {
		vt_cursor_set(pars[1] - 1, pars[0] - 1);
		return;
	}

	vt_cursor_set(0, 0);
}

static void char_attrs_reset(void)
{
	style.font = gp_font_haxor_narrow_16;
#ifdef COLORS
	bg_color = 0;
	fg_color = 7;
	bold = 0;
#else
	bg_color = 0x000000;
	fg_color = 0xffffff;
#endif
}

static void char_attrs_bold(void)
{
	style.font = gp_font_haxor_narrow_bold_16;
#ifdef COLORS
	bold = 1;
#endif
}

static void char_attrs_reverse(void)
{
	GP_SWAP(bg_color, fg_color);
}

/*
 * Select graphic rendition
 */
void vt_csi_m(int *pars, int par_cnt)
{
	int i;

	if (!par_cnt)
		pars[par_cnt++] = 0;

	for (i = 0; i < par_cnt; i++) {
		switch (pars[i]) {
		case 0:
			char_attrs_reset();
		break;
		case 1:
			char_attrs_bold();
		break;
		case 7:
			char_attrs_reverse();
		break;
#ifdef COLORS
		case 30 ... 37:
			fg_color = pars[i] - 30;
		break;
		case 40 ... 47:
			bg_color = pars[i] - 40;
		break;
#endif
		}
	}
}

static void vt_csi_cursor(int *pars, int par_cnt, char csi)
{
	int inc = par_cnt ? pars[0] : 1;

	switch (csi) {
	case 'A':
		vt_cursor_move(0, -inc);
	break;
	case 'B':
		vt_cursor_move(0, inc);
	break;
	case 'C':
		vt_cursor_move(inc, 0);
	break;
	case 'D':
		vt_cursor_move(-inc, 0);
	break;
	}
}

void vt_do_csi(int *pars, int par_cnt, char csi)
{
	if (verbose || 1) {
		int i;
		fprintf(stderr, "CSI");
		for (i = 0; i < par_cnt-1; i++)
			fprintf(stderr, "%i;", pars[i]);
		if (par_cnt)
			fprintf(stderr, "%i", pars[par_cnt-1]);
		fprintf(stderr, "%c\n", csi);
	}

	switch (csi) {
	case 'A':
	case 'B':
	case 'C':
	case 'D':
		vt_csi_cursor(pars, par_cnt, csi);
	break;
	case 'H':
		vt_csi_H(pars, par_cnt);
	break;
	case 'J':
		erase(vt.cur_x, vt.cur_y, vt.w, vt.h);
	break;
	case 'K':
		if (!par_cnt)
			pars[0] = 0;

		vt_csi_K(pars[0]);
	break;
	case 'm':
		vt_csi_m(pars, par_cnt);
	break;
	default:
		fprintf(stderr, "Unhandled CSI %c %02x\n", isprint(csi) ? csi : ' ', csi);
	}
}

#define MAX_CSI_PARS 5

int vt_csi(char c)
{
	static int pars[MAX_CSI_PARS];
	static int par;
	static int t;

	switch (c) {
	case '0' ... '9':
		pars[par] *= 10;
		pars[par] += c - '0';
		t = 1;
	break;
	case ';':
		par = (par + 1) % MAX_CSI_PARS;
		pars[par] = 0;
		t = 0;
	break;
	default:
		par += t;
		vt_do_csi(pars, par, c);
		par = 0;
		pars[0] = 0;
		t = 0;
		return 1;
	break;
	}

	return 0;
}

/*
 * Bit 1 == Cursor
 */
static void vt_do_csi_priv(int val, char c)
{
	switch (c) {
	case 'h':
		printf("Set bit %i\n", val);
	break;
	case 'l':
		printf("Clear bit %i\n", val);
	break;
	default:
		fprintf(stderr, "Unhandled private CSI %i%c\n", val, c);
	break;
	}
}

int vt_csi_priv(char c)
{
	static int val;

	switch (c) {
	case '0' ... '9':
		val *= 10;
		val += c - '0';
	break;
	default:
		vt_do_csi_priv(val, c);
		val = 0;
		return 1;
	break;
	}

	return 0;
}

enum vt_state {
	VT_DEF,
	VT_ESC,
	VT_CSI,
	VT_CSI_PRIV,
	VT_SCS_G0,
	VT_SCS_G1,
	VT52_ESC_Y,
};

static int vt52_esc_y(unsigned char c)
{
	static int y, flag;

	if (flag) {
		vt_cursor_set((int)c - 32, y - 32);
		flag = 0;
		return 1;
	}

	y = c;
	flag = 1;
	return 0;
}

void vt_char(char c)
{
	static enum vt_state state;

	//fprintf(stderr, "%c %02x\n", isprint(c) ? c : ' ', c);

	switch (state) {
	case VT_DEF:
		switch (c) {
		case '\e':
			state = VT_ESC;
		break;
		case '\f':
		case '\v':
		case '\n':
			vt_cursor_move(0, 1);
		break;
		case '\r':
			vt_cursor_set(0, -1);
		break;
		case '\t':
			vt_tab();
		break;
		case '\a':
			fprintf(stderr, "Bell!\n");
		break;
		case '\016': /* SO */
			//if (verbose)
				fprintf(stderr, "Charset G1\n");
			charset_sel = 1;
		break;
		case '\017': /* SI */
			//if (verbose)
				fprintf(stderr, "Charset G0\n");
			charset_sel = 0;
		break;
		case 0x08: /* backspace */
			vt_bs();
		break;
		default:
			vt_putchar(c);
		}
	break;
	case VT_ESC:
		switch (c) {
		case '[':
			state = VT_CSI;
		break;
		case '(':
			state = VT_SCS_G0;
		break;
		case ')':
			state = VT_SCS_G1;
		break;
		case '=': /* Set alternate keypad mode */
		case '>': /* Set numeric keypad mode */
			state = VT_DEF;
		break;
		default:
			fprintf(stderr, "Unhandled esc %c %02x\n", isprint(c) ? c : ' ', c);
			state = VT_DEF;
		break;
		}
	break;
	case VT_CSI:
		if (c == '?') {
			state = VT_CSI_PRIV;
		} else {
			if (vt_csi(c))
				state = VT_DEF;
		}
	break;
	case VT_CSI_PRIV:
		if (vt_csi_priv(c))
			state = VT_DEF;
	break;
	case VT_SCS_G0:
		charset_G0 = c;
		//if (verbose)
			fprintf(stderr, "CSC G0=%c\n", c);
		state = VT_DEF;
	break;
	case VT_SCS_G1:
		charset_G1 = c;
		//if (verbose)
			fprintf(stderr, "CSC G1=%c\n", c);
		state = VT_DEF;
	break;
	case VT52_ESC_Y:
		if (vt52_esc_y(c))
			state = VT_DEF;
	break;
	}
}

void vt_read(int fd)
{
	char buf[128];
	int ret, i;

	ret = read(fd, buf, sizeof(buf));

	for (i = 0; i < ret; i++)
		vt_char(buf[i]);
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

#ifdef RESIZE
static void resize(int fd, gp_event *ev)
{
	struct winsize w;

	vt.w = ev->val.sys.w/vt.cell_w;
	vt.h = ev->val.sys.w/vt.cell_h;

	w.ws_row = vt.h;
	w.ws_col = vt.w;

	if (ioctl(fd, TIOCSWINSZ, &w) < 0)
		fprintf(stderr, "ioctl(fd, TIOCSWINSZ, ...) failed\n");

	gp_backend_resize_ack(win);
	gp_fill(win->pixmap, bg_col());
	//TODO: Redraw buffer?
	gp_backend_flip(win);
}
#endif

int main(void)
{
	gp_event ev;
	int fd;

	fd = run_vt_shell();
	init_graphics();

	for (;;) {
		while (gp_backend_poll_event(win, &ev)) {
			switch (ev.type) {
			case GP_EV_KEY:
				if (ev.code == GP_EV_KEY_DOWN)
					event_to_vt(&ev, fd);
			break;
#ifdef RESIZE
			case GP_EV_SYS:
				if (ev.code == GP_EV_SYS_RESIZE) {
					resize(fd, &ev);
				}
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
