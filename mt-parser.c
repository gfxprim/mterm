#include <ctype.h>
#include "mt-sbuf.h"
#include "mt-parser.h"

/*
 * Move cursor:
 *
 * A - CUU Cursor Up
 * B - CUD Cursor Down
 * C - CUF Cursor Forward
 * D - CUB Cursor Backward
 */
static void csi_cursor(struct mt_parser *self, char csi)
{
	int inc = self->pars[0] ? self->pars[0] : 1;

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

/*
 * DECSTBM - Set Top and Bottom Margins
 *
 *
 */
static void csi_r(struct mt_parser *self)
{
	if (self->par_cnt != 2) {
		self->pars[0] = 0;
		self->pars[1] = self->sbuf->rows - 1;
	}

	fprintf(stderr, "SCROLL REGION %i %i\n", self->pars[0], self->pars[1]);
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
 * REP - Repeat preceding graphic char.
 */
static void csi_b(struct mt_parser *self)
{
	unsigned int i;

	if (!self->last_gchar)
		return;

	for (i = 0; i < self->pars[0]; i++)
		mt_sbuf_putc(self->sbuf, self->last_gchar);
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
 * VPA -- Vertical Line Position Absolute
 *
 * Sets column.
 */
static void csi_d(struct mt_parser *self)
{
	if (!self->pars[0])
		self->pars[0] = 1;

	mt_sbuf_cursor_set(self->sbuf, -1, self->pars[0] - 1);
}

/*
 * VPR -- Vertical Line Position Relative
 *
 * Sets column.
 */
static void csi_e(struct mt_parser *self)
{
	if (!self->pars[0])
		self->pars[0] = 1;

	mt_sbuf_cursor_move(self->sbuf, 0, self->pars[0]);
}

/*
 * CUP -- Cursor Position
 */
void csi_H(struct mt_parser *self)
{
	if (self->par_cnt == 2) {
		if (self->pars[0] == 0)
			self->pars[0] = 1;

		if (self->pars[1] == 0)
			self->pars[1] = 1;

		mt_sbuf_cursor_set(self->sbuf, self->pars[1] - 1, self->pars[0] - 1);
		return;
	}

	mt_sbuf_cursor_set(self->sbuf, 0, 0);
}

/*
 * HVP - Horizontal and Vertical Position
 *
 * Sets cursor position.
 */
static void csi_f(struct mt_parser *self)
{
	csi_H(self);
}

static void do_csi(struct mt_parser *self, char csi)
{
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
	case 'b':
		csi_b(self);
	break;
	case 'c':
		csi_c(self);
	break;
	case 'd':
		csi_d(self);
	break;
	case 'e':
		csi_e(self);
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
	/* xterm window manipulation */
	case 't':
		fprintf(stderr, "TODO: Xterm window manipulation!\n");
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
		fprintf(stderr, "Invalid G%i charset '%c'\n", pos, c);
	break;
	}
}

/*
 * DEC private modes
 *
 * 1  -> Normal Cursor Keys (DECCKM)
 * 7  -> No Wraparound Mode (DECAWM)
 * 25 -> ide Cursor (DECTCEM)
 *
 * s == save
 * r == restore
 * l == disable
 * h == enable
 */
static void do_csi_dec(struct mt_parser *self, char c)
{
	unsigned int i;

	switch (c) {
	case 's':
	case 'r':
		fprintf(stderr, "DEC CSI save/restore\n");
		return;
	case 'l':
	case 'h':
	break;
	default:
		fprintf(stderr, "Invalid DECCSI priv %c\n", c);
	}

	uint8_t val = c == 'l' ? 0 : 1;

	for (i = 0; i < self->par_cnt; i++) {
		switch (self->pars[i]) {
		case 1:
			fprintf(stderr, "TODO: Normal Cursor Keys %c\n", c);
		break;
		/* DECAWM -- Autowrap Mode */
		case 7:
			mt_sbuf_autowrap(self->sbuf, val);
		break;
		case 12:
			fprintf(stderr, "TODO: Cursor blink %c\n", c);
		break;
		case 25:
			mt_sbuf_cursor_visible(self->sbuf, val);
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

static void csi_dispatch(struct mt_parser *self, char c)
{
	switch (self->csi_intermediate) {
	case 0:
		do_csi(self, c);
	break;
	case '?':
		do_csi_dec(self, c);
	break;
	/* CSI DECSTR - Soft Terminal Reset */
	case '!':
		if (c == 'p')
			mt_sbuf_DECSTR(self->sbuf);
		else
			fprintf(stderr, "Unhandled CSI!%c\n", c);
	break;
	/* CSI DA2 - Secondary Device Attributes */
	case '<':
		if (c == 'c')
			self->response(self->response_fd, "\e[32;1;1c");
		else
			fprintf(stderr, "Unhandled CSI<%c\n", c);
	break;
	}
}

static void csi_parse_param(struct mt_parser *self, char c)
{
	switch (c) {
	/* Parse number param */
	case '0' ... '9':
		self->pars[self->par_cnt] *= 10;
		self->pars[self->par_cnt] += c - '0';
		self->par_t = 1;
	break;
	/* Next CSI parameter */
	case ';':
		self->par_cnt = (self->par_cnt + 1) % MT_MAX_CSI_PARS;
		self->pars[self->par_cnt] = 0;
		self->par_t = 0;
	break;
	}
}


/*
 * CSI state machine, control characters are handled in the main loop.
 */
static void csi(struct mt_parser *self, char c)
{
	/* 0x40 - 0x7E is the only way how to get out of ignore state */
	if (self->state == VT_CSI_IGNORE) {
		switch (c) {
		case '@' ... '~':
			self->state = VT_GROUND;
			return;
		}
	}

	switch (c) {
	/* 0x20 - 0x2F enter intermedate */
	case ' ' ... '/':
		self->csi_intermediate = c;
		self->state = VT_CSI_INTERMEDIATE;
		return;
	/* 0x3A - simply invalid */
	case ':':
		self->state = VT_CSI_IGNORE;
		return;
	/* Dispatch CSI 0x40 ... 0x7E */
	case '@' ... '~':
		self->par_cnt += !!self->par_t;

		csi_dispatch(self, c);

		self->state = VT_GROUND;
		return;
	/* DEL - ignored during CSI */
	case 0x7F:
		return;
	}

	switch (self->state) {
	case VT_CSI_ENTRY:
		switch (c) {
		case '0' ... '9':
		case ';':
			csi_parse_param(self, c);
			self->state = VT_CSI_PARAM;
		break;
		case '<' ... '?':
			self->csi_intermediate = c;
			self->state = VT_CSI_PARAM;
		break;
		}
	break;
	case VT_CSI_PARAM:
		switch (c) {
		case '<' ... '?':
			self->state = VT_CSI_IGNORE;
		break;
		case '0' ... '9':
		case ';':
			csi_parse_param(self, c);
		break;
		}
	break;
	case VT_CSI_INTERMEDIATE:
		switch (c) {
		case '0' ... '?':
			self->state = VT_CSI_IGNORE;
		break;
		}
	break;
	}
}

/*
 * Device Control String
 */
static void dcs(struct mt_parser *self, char c)
{
	switch (c) {
	case '@' ... '~':
		self->state = VT_GROUND;
		return;
	}
}

static void tab(struct mt_parser *self)
{
	mt_coord c_col = mt_sbuf_cursor_col(self->sbuf);

	mt_sbuf_cursor_move(self->sbuf, 8 - (c_col % 8), 0);
}

/*
 * Operating System Command
 *
 * OSC 0; [title] \a     - set window tittle
 * OSC 52;c; [base64] \a - set clipboard
 */
static void osc(struct mt_parser *self, char c)
{
	if (c == '\a')
		self->state = VT_GROUND;

	//fprintf(stderr, "OSC '%c'\n", c);
}

static void param_reset(struct mt_parser *self)
{
	memset(self->pars, 0, sizeof(self->pars));
	self->csi_intermediate = 0;
	self->par_cnt = 0;
	self->par_t = 0;
}

static void csi_entry(struct mt_parser *self)
{
	self->state = VT_CSI_ENTRY;
	param_reset(self);
}

static void dcs_entry(struct mt_parser *self)
{
	fprintf(stderr, "DCS!\n");
	self->state = VT_DCS_ENTRY;
	param_reset(self);
}

static void esc_dec(struct mt_parser *self, char c)
{
	switch (c) {
	/* DECALN - Fills screen with test aligment pattern + cursor home */
	case '8':
		//TODO: Fill screen with 'E'
		mt_sbuf_cursor_set(self->sbuf, 0, 0);
	break;
	default:
		fprintf(stderr, "Unhandled DEC ESC %c %02x\n", isprint(c) ? c : ' ', c);
	}
}

static void esc_dispatch(struct mt_parser *self, char c)
{
	if (self->csi_intermediate) {
		switch (self->csi_intermediate) {
		/* SCS G0 */
		case '(':
			set_charset(self, 0, c);
		break;
		/* SCS G1 */
		case ')':
			set_charset(self, 1, c);
		break;
		/* DEC ESC */
		case '#':
			esc_dec(self, c);
		break;
		default:
			fprintf(stderr, "Unhandled ESC %c %c\n", self->csi_intermediate, c);
		}

		self->state = VT_GROUND;
		return;
	}

	switch (c) {
	case '[':
		csi_entry(self);
		return;
	case ']':
		self->state = VT_OSC;
		return;
	case '7':
		fprintf(stderr, "TODO: Save cursor\n");
	break;
	case '8':
		fprintf(stderr, "TODO: Restore cursor\n");
	break;
	case 'D':
		/* IND - Index, moves cursor down - scrolls */
		mt_sbuf_cursor_down(self->sbuf);
	break;
	case 'E':
		/* NEL - Next line */
		mt_sbuf_newline(self->sbuf);
	break;
	case 'M':
		/* RI - Reverse Index, moves cursor up - scrolls */
		mt_sbuf_cursor_up(self->sbuf);
	break;
	case 'P':
		dcs_entry(self);
		return;
	case 'c':
		/* RIS - Reset to Inital State */
		mt_sbuf_RIS(self->sbuf);
	break;
	case '=': /* Set alternate keypad mode */
	case '>': /* Set numeric keypad mode */
	break;
	default:
		fprintf(stderr, "Unhandled ESC %c %02x\n", isprint(c) ? c : ' ', c);
	break;
	}

	self->state = VT_GROUND;
}

static void esc(struct mt_parser *self, unsigned char c)
{
	switch (c) {
	/* 0x20 - 0x2F */
	case ' ' ... '/':
		self->csi_intermediate = c;
	break;
	/* 0x30 - 0x7E */
	case '0' ... '~':
		esc_dispatch(self, c);
	break;
	case 0x7F:
	break;
	}
}

static void esc_entry(struct mt_parser *self)
{
	self->state = VT_ESC_ENTRY;
	self->csi_intermediate = 0;
}

/*
 * Some control chanracters may be interleaved with CSIs
 */
static void parser_ctrl_char(struct mt_parser *self, unsigned char c)
{
	switch (c) {
	/* BEL 0x07 */
	case '\a':
		if (self->bell)
			self->bell();
	break;
	/* BS 0x08 */
	case '\b':
		mt_sbuf_cursor_move(self->sbuf, -1, 0);
	break;
	/* TAB 0x09 */
	case '\t':
		tab(self);
	break;
	/* CR 0x0D */
	case '\r':
		mt_sbuf_cursor_set(self->sbuf, 0, -1);
	break;
	/* SO 0x0E*/
	case '\016':
		mt_sbuf_shift_out(self->sbuf);
	break;
	/* SI 0x0F */
	case '\017':
		mt_sbuf_shift_in(self->sbuf);
	break;
	/* CAN 0x18 - Cancel ESC, CSI, DCS */
	case 0x18:
		self->state = VT_GROUND;
	break;
	/* ESC 0x1B */
	case '\e':
		esc_entry(self);
	break;
	case '\f':
	case '\v':
	case '\n':
		mt_sbuf_newline(self->sbuf);
	break;
	/* DCS */
	case 0x90:
		dcs_entry(self);
	break;
	/* CSI */
	case 0x9B:
		csi_entry(self);
	break;
	/* OSC */
	case 0x9D:
		self->state = VT_OSC;
	break;
	default:
		fprintf(stderr, "Unhandled control char 0x%02x\n", c);
	}
}

#define CONTROL_C0 0x00 ... 0x1F
#define CONTROL_C1 0x80 ... 0x9F

static void next_char(struct mt_parser *self, unsigned char c)
{
	//fprintf(stderr, "0x%02x %c\n", c, isprint(c) ? c : ' ');

	/*
	 * Control characters are processed immediately unless in OSC or DCS.
	 */
	switch (self->state & VT_STATE_MASK) {
	case VT_OSC:
	case VT_DCS:
	break;
	default:
		switch (c) {
			case CONTROL_C0:
			case CONTROL_C1:
				parser_ctrl_char(self, c);
			return;
		}
	break;
	}

	switch ((self->state & VT_STATE_MASK)) {
	case VT_GROUND:
		switch (c) {
		case ' ' ... 0x7F:
			mt_sbuf_putc(self->sbuf, c);
			self->last_gchar = c;
		break;
		default:
			fprintf(stderr, "Unhandled char 0x%02x\n", c);
		break;
		}
	break;
	case VT_ESC:
		esc(self, c);
	break;
	case VT_CSI:
		csi(self, c);
	break;
	case VT_DCS:
		dcs(self, c);
	break;
	case VT_OSC:
		osc(self, c);
	break;
	case VT52_ESC_Y:
		//if (vt52_esc_y(c))
			self->state = VT_GROUND;
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
