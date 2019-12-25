#include <ctype.h>
#include "mt-sbuf.h"
#include "mt-parser.h"

/*
 * Move cursor:
 *
 * A - up
 * B - down
 * C - right
 * D - left
 */
static void csi_cursor(struct mt_parser *self, char csi)
{
	int inc = self->par_cnt ? self->pars[0] : 1;

	switch (csi) {
	case 'A':
		mt_sbuf_cursor_move(self->sbuf, 0, -inc);
	break;
	case 'B':
		mt_sbuf_cursor_move(self->sbuf, 0, inc);
	break;
	case 'C':
		mt_sbuf_cursor_move(self->sbuf, inc, 0);
	break;
	case 'D':
		mt_sbuf_cursor_move(self->sbuf, -inc, 0);
	break;
	}
}

/*
 * Clear line from cursor:
 *
 * 0 -- right (default)
 * 1 -- left
 * 2 -- whole line
 */
static void csi_K(struct mt_parser *self)
{
	int par = self->pars[0];

	if (self->par_cnt < 1)
		par = 0;

	switch (par) {
	case 0:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_CUR_RIGHT);
	break;
	case 1:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_CUR_LEFT);
	break;
	case 2:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_LINE);
	break;
	}
}

/*
 * Set cursor position.
 */
void csi_H(struct mt_parser *self)
{
	if (self->par_cnt == 2) {
		mt_sbuf_cursor_set(self->sbuf, self->pars[1] - 1, self->pars[0] - 1);
		return;
	}

	mt_sbuf_cursor_set(self->sbuf, 0, 0);
}

/*
 * Clear screen
 *
 * 0 -- clear from cursor to the end (default)
 * 1 -- clear from beginning to cursor
 * 2 -- clear whole display
 * 3 -- clears whole display + scrollback
 */
static void csi_J(struct mt_parser *self)
{
	int par = self->pars[0];

	if (self->par_cnt < 1)
		par = 0;

	switch (par) {
	case 0:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_END);
	break;
	case 1:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_START);
	break;
	case 2:
	case 3: //TODO: Scrollback
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_SCREEN);
	break;
	default:
		fprintf(stderr, "Unhandled CSI J %i\n", par);
	}
}

/*
 * Select graphic rendition
 *
 * 0 -- reset all text attributes
 * 1 -- bold
 * 2 -- faint (decreased intensity)
 * 3 -- italic
 * 4 -- underline
 * 5 -- slow blink
 * 6 -- rapid blink
 * 7 -- reverse
 * 8 -- conceal (mostly unsupported)
 * 9 -- cossed-out
 * 10 -- default font
 * 23 -- turn off italic
 * 24 -- turn off underline
 * ...
 * 27 -- turn off reverse
 * ...
 * 29 -- turn off crossed-out
 * 30 - 37 -- set fg color
 * 38 -- set RGB fg color
 * 39 -- set default fg color
 * 40 - 47 -- set bg color
 * 48 -- set RGB bg color
 * 49 -- set default bg color
 * ...
 */
void csi_m(struct mt_parser *self)
{
	int i;

	if (!self->par_cnt)
		self->pars[self->par_cnt++] = 0;

	for (i = 0; i < self->par_cnt; i++) {
		switch (self->pars[i]) {
		case 0:
			mt_sbuf_bold(self->sbuf, 0);
			mt_sbuf_reverse(self->sbuf, 0);
			mt_sbuf_fg_col(self->sbuf, self->fg_col);
			mt_sbuf_bg_col(self->sbuf, self->bg_col);
		break;
		case 1:
			mt_sbuf_bold(self->sbuf, 1);
		break;
		case 7:
			mt_sbuf_reverse(self->sbuf, 1);
		break;
		case 10:
			mt_sbuf_bold(self->sbuf, 0);
		break;
		case 27:
			mt_sbuf_reverse(self->sbuf, 0);
		break;
		case 30 ... 37:
			mt_sbuf_fg_col(self->sbuf, self->pars[i] - 30);
		break;
		case 39:
			mt_sbuf_fg_col(self->sbuf, self->fg_col);
		break;
		case 40 ... 47:
			mt_sbuf_bg_col(self->sbuf, self->pars[i] - 40);
		break;
		case 49:
			mt_sbuf_bg_col(self->sbuf, self->bg_col);
		break;
		default:
			fprintf(stderr, "Unhandled CSI %i m\n", self->pars[i]);
		}
	}
}

/*
 * Inserts blank spaces, line shifts to left.
 */
static void csi_insert_spaces(struct mt_parser *self)
{
	unsigned int i;

	if (!self->par_cnt)
		self->pars[0] = 1;

	mt_sbuf_insert_blank(self->sbuf, self->pars[0]);
}

/*
 * Delete characters from cursor to right, line shifts to left.
 */
static void csi_del_chars(struct mt_parser *self)
{
	unsigned int i;

	if (!self->par_cnt)
		self->pars[0] = 1;

	mt_sbuf_del_chars(self->sbuf, self->pars[0]);
}

static void csi_t(struct mt_parser *self)
{
	fprintf(stderr, "CSI t\n");
}

static void csi_r(struct mt_parser *self)
{
	fprintf(stderr, "SCROLL REGION\n");
}

/*
 * CSI .. l -> RM Reset Mode
 * CSI .. h -> SM Set Mode
 *
 * Modes:
 * .
 * 4 IRM -- Insertion Replacement Mode
 * .
 * .
 * 20 LNM -- Automatic Newline
 * .
 */
static void csi_lh(struct mt_parser *self, char csi)
{
	unsigned int i;

	for (i = 0; i < self->par_cnt; i++) {
		switch (self->pars[i]) {
		case 4:
			fprintf(stderr, "IRM %c\n", csi);
		break;
		default:
			fprintf(stderr, "Unhandled RM %i\n", self->pars[i]);
		}
	}
}

/*
 * DA - Device Attribute
 *
 * Sends device attributes to application.
 */
static void csi_c(struct mt_parser *self)
{
	if (!self->response)
		return;

	self->response(self->response_fd, "\e[?6c");
}

/*
 * HVP - Horizontal and Vertical Position
 *
 * Sets cursor position.
 */
static void csi_f(struct mt_parser *self)
{
	if (self->par_cnt != 2) {
		self->pars[0] = 1;
		self->pars[1] = 1;
	}

	if (self->pars[0] == 0 && self->pars[1] == 0) {
		self->pars[0] = 1;
		self->pars[1] = 1;
	}

	mt_sbuf_cursor_set(self->sbuf, self->pars[0] - 1, self->pars[1] - 1);
}

static void do_csi(struct mt_parser *self, char csi)
{
	//fprintf(stderr, "CSI %c %i\n", csi, pars[0]);

	switch (csi) {
	case '@':
		csi_insert_spaces(self);
	break;
	case 'P':
		csi_del_chars(self);
	break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
		csi_cursor(self, csi);
	break;
	case 'H':
		csi_H(self);
	break;
	case 'J':
		csi_J(self);
	break;
	case 'K':
		csi_K(self);
	break;
	case 'c':
		csi_c(self);
	break;
	case 'f':
		csi_f(self);
	break;
	case 'm':
		csi_m(self);
	break;
	case 'r':
		csi_r(self);
	break;
	case 'l':
	case 'h':
		csi_lh(self, csi);
	break;
	default:
		fprintf(stderr, "Unhandled CSI %c %02x\n", isprint(csi) ? csi : ' ', csi);
	}
}

static void set_charset(struct mt_parser *self, uint8_t pos, char c)
{
	switch (c) {
	case 'A':
	case 'B':
	case '0':
	case '1':
	case '2':
		mt_sbuf_set_charset(self->sbuf, pos, c);
	break;
	default:
		fprintf(stderr, "Invalid G%i charset '%c'", pos, c);
	break;
	}
}

static void do_csi_dec(struct mt_parser *self, char c)
{
	unsigned int i;

	if (c != 'l' && c != 'h')
		fprintf(stderr, "Invalid DECCSI priv %c\n", c);

	uint8_t val = c == 'l' ? 0 : 1;

	for (i = 0; i < self->par_cnt; i++) {
		switch (self->pars[i]) {
		case 25:
			mt_sbuf_cursor_visible(self->sbuf, val);
		break;
		case 12:
			fprintf(stderr, "TODO: Cursor blink %c\n", c);
		break;
		case 2004:
			fprintf(stderr, "TODO: Bracketed paste mode %c\n", c);
		break;
		default:
			fprintf(stderr, "Unhandled DECCSI %u %c\n",
				self->pars[i], c);
		}
	}
}

static int csi_parse_pars(struct mt_parser *self, char c, char dec)
{
	switch (c) {
	case '0' ... '9':
		self->pars[self->par_cnt] *= 10;
		self->pars[self->par_cnt] += c - '0';
		self->par_t = 1;
	break;
	case ';':
		self->par_cnt = (self->par_cnt + 1) % MT_MAX_CSI_PARS;
		self->pars[self->par_cnt] = 0;
		self->par_t = 0;
	break;
	default:
		self->par_cnt += !!self->par_t;

		if (dec)
			do_csi_dec(self, c);
		else
			do_csi(self, c);

		memset(self->pars, 0, sizeof(self->pars));
		self->par_cnt = 0;
		self->par_t = 0;
		return 1;
	break;
	}

	return 0;
}

static int csi(struct mt_parser *self, char c)
{
	return csi_parse_pars(self, c, 0);
}

static void tab(struct mt_parser *self)
{
	mt_coord c_col = mt_sbuf_cursor_col(self->sbuf);

	mt_sbuf_cursor_move(self->sbuf, 8 - (c_col % 8), 0);
}

static int csi_dec(struct mt_parser *self, char c)
{
	return csi_parse_pars(self, c, 1);
}

/*
 * Operating System Command
 *
 * OSC 0; [title] \a     - set window tittle
 * OSC 52;c; [base64] \a - set clipboard
 */
static int osc(struct mt_parser *self, char c)
{
	if (c == '\a')
		return 1;

	//fprintf(stderr, "OSC '%c'\n", c);
	return 0;
}

static void next_char(struct mt_parser *self, char c)
{
	//fprintf(stderr, "0x%02x %c\n", c, isprint(c) ? c : ' ');
	switch (self->state) {
	case VT_DEF:
		switch (c) {
		case '\e':
			self->state = VT_ESC;
		break;
		case '\f':
		case '\v':
		case '\n':
			mt_sbuf_newline(self->sbuf);
		break;
		case '\r':
			mt_sbuf_cursor_set(self->sbuf, 0, -1);
		break;
		case '\t':
			tab(self);
		break;
		case '\a':
			fprintf(stderr, "Bell!\n");
		break;
		case '\016': /* SO */
			mt_sbuf_shift_out(self->sbuf);
		break;
		case '\017': /* SI */
			mt_sbuf_shift_in(self->sbuf);
		break;
		case 0x08: /* backspace */
			mt_sbuf_cursor_move(self->sbuf, -1, 0);
		break;
		default:
			mt_sbuf_putc(self->sbuf, c);
		break;
		}
	break;
	case VT_ESC:
		switch (c) {
		case '[':
			self->state = VT_CSI;
		break;
		case ']':
			self->state = VT_OSC;
		break;
		case '(':
			self->state = VT_SCS_G0;
		break;
		case ')':
			self->state = VT_SCS_G1;
		break;
		case '7':
			fprintf(stderr, "TODO: Save cursor\n");
			self->state = VT_DEF;
		break;
		case '8':
			fprintf(stderr, "TODO: Restore cursor\n");
			self->state = VT_DEF;
		break;
		case 'D':
			/* IND - Index, moves cursor down - scrolls */
			mt_sbuf_cursor_down(self->sbuf);
			self->state = VT_DEF;
		break;
		case 'E':
			/* NEL - Next line */
			mt_sbuf_newline(self->sbuf);
			self->state = VT_DEF;
		break;
		case 'M':
			/* RI - Reverse Index, moves cursor up - scrolls */
			mt_sbuf_cursor_up(self->sbuf);
			self->state = VT_DEF;
		break;
		case 'c':
			/* RIS - Reset to Inital State */
			mt_sbuf_RIS(self->sbuf);
			self->state = VT_DEF;
		break;
		case '=': /* Set alternate keypad mode */
		case '>': /* Set numeric keypad mode */
			self->state = VT_DEF;
		break;
		case '#':
			self->state = VT_ESC_DEC;
		break;
		default:
			fprintf(stderr, "Unhandled ESC %c %02x\n", isprint(c) ? c : ' ', c);
			self->state = VT_DEF;
		break;
		}
	break;
	case VT_ESC_DEC:
		switch (c) {
		/* DECALN - Fills screen with test aligment pattern + cursor home */
		case '8':
			//TODO: Fill screen with 'E'
			mt_sbuf_cursor_set(self->sbuf, 0, 0);
		break;
		default:
			fprintf(stderr, "Unhandled DEC ESC %c %02x\n", isprint(c) ? c : ' ', c);
		}

		self->state = VT_DEF;
	break;
	case VT_CSI:
		switch (c) {
		case '?':
			self->state = VT_CSI_DEC;
		break;
		case '>':
			self->state = VT_CSI_DA2;
		break;
		case '!':
			self->state = VT_CSI_SOFT_RESET;
		break;
		default:
			if (csi(self, c))
				self->state = VT_DEF;
		break;
		}
	break;
	case VT_OSC:
		if (osc(self, c))
			self->state = VT_DEF;
	break;
	case VT_CSI_DEC:
		if (csi_dec(self, c))
			self->state = VT_DEF;
	break;
	case VT_CSI_DA2:
		if (c == 'c')
			self->state = VT_DEF;
		//TODO: response CSI61;1;1c
	break;
	case VT_CSI_SOFT_RESET:
		mt_sbuf_DECSTR(self->sbuf);
		self->state = VT_DEF;
	break;
	case VT_SCS_G0:
		set_charset(self, 0, c);
		self->state = VT_DEF;
	break;
	case VT_SCS_G1:
		set_charset(self, 1, c);
		self->state = VT_DEF;
	break;
	case VT52_ESC_Y:
		//if (vt52_esc_y(c))
			self->state = VT_DEF;
	break;
	default:
		fprintf(stderr, "INVALID STATE %i\n", self->state);
	}
}

void mt_parse(struct mt_parser *self, const char *buf, size_t buf_sz)
{
	size_t i;

	for (i = 0; i < buf_sz; i++)
		next_char(self, buf[i]);
}
