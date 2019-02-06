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
static void csi_cursor(struct mt_parser *self, int *pars, int par_cnt, char csi)
{
	int inc = par_cnt ? pars[0] : 1;

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
static void csi_K(struct mt_parser *self, int *pars, int par_cnt)
{
	int par = pars[0];

	if (par_cnt < 1)
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
void csi_H(struct mt_parser *self, int *pars, int par_cnt)
{
	if (par_cnt == 2) {
		mt_sbuf_cursor_set(self->sbuf, pars[1] - 1, pars[0] - 1);
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
static void csi_J(struct mt_parser *self, int *pars, int par_cnt)
{
	int par = pars[0];

	if (par_cnt < 1)
		par = 0;

	switch (par) {
	case 0:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_END);
	break;
	case 1:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_START);
	break;
	case 2:
		mt_sbuf_erase(self->sbuf, MT_SBUF_ERASE_SCREEN);
	break;
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
 * ...
 * 30 - 37 -- set fg color
 * 38 -- set RGB fg color
 * 39 -- set default fg color
 * 40 - 47 -- set bg color
 * 48 -- set RGB bg color
 * 49 -- set default bg color
 * ...
 */
void csi_m(struct mt_parser *self, int *pars, int par_cnt)
{
	int i;

	if (!par_cnt)
		pars[par_cnt++] = 0;

	for (i = 0; i < par_cnt; i++) {
		switch (pars[i]) {
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
		case 30 ... 37:
			mt_sbuf_fg_col(self->sbuf, pars[i] - 30);
		break;
		case 39:
			mt_sbuf_fg_col(self->sbuf, self->fg_col);
		break;
		case 40 ... 47:
			mt_sbuf_bg_col(self->sbuf, pars[i] - 40);
		break;
		case 49:
			mt_sbuf_bg_col(self->sbuf, self->bg_col);
		break;
		default:
			fprintf(stderr, "Unhandled CSI %i m\n", pars[i]);
		}
	}
}

static void csi_t(struct mt_parser *self, int *pars, int par_cnt)
{

}

static void do_csi(struct mt_parser *self, int *pars, int par_cnt, char csi)
{
	//fprintf(stderr, "CSI %c %i\n", csi, pars[0]);

	switch (csi) {
	case 'A':
	case 'B':
	case 'C':
	case 'D':
		csi_cursor(self, pars, par_cnt, csi);
	break;
	case 'H':
		csi_H(self, pars, par_cnt);
	break;
	case 'J':
		csi_J(self, pars, par_cnt);
	break;
	case 'K':
		csi_K(self, pars, par_cnt);
	break;
	case 'm':
		csi_m(self, pars, par_cnt);
	break;
	default:
		fprintf(stderr, "Unhandled CSI %c %02x\n", isprint(csi) ? csi : ' ', csi);
	}
}

#define MAX_CSI_PARS 10

static int csi(struct mt_parser *self, char c)
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
		do_csi(self, pars, par, c);
		par = 0;
		pars[0] = 0;
		t = 0;
		return 1;
	break;
	}

	return 0;
}

static void next_char(struct mt_parser *self, char c)
{
	//printf("0x%02x %c\n", c, isprint(c) ? c : ' ');
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
			//vt_tab();
		break;
		case '\a':
			//fprintf(stderr, "Bell!\n");
		break;
		case '\016': /* SO */
			//if (verbose)
			//	fprintf(stderr, "Charset G1\n");
			self->charset = 1;
		break;
		case '\017': /* SI */
			//if (verbose)
			//	fprintf(stderr, "Charset G0\n");
			self->charset = 0;
		break;
		case 0x08: /* backspace */
			mt_sbuf_backspace(self->sbuf);
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
		case '(':
			self->state = VT_SCS_G0;
		break;
		case ')':
			self->state = VT_SCS_G1;
		break;
		case '=': /* Set alternate keypad mode */
		case '>': /* Set numeric keypad mode */
			self->state = VT_DEF;
		break;
		default:
			fprintf(stderr, "Unhandled esc %c %02x\n", isprint(c) ? c : ' ', c);
			self->state = VT_DEF;
		break;
		}
	break;
	case VT_CSI:
		if (c == '?') {
			self->state = VT_CSI_PRIV;
		} else {
			if (csi(self, c))
				self->state = VT_DEF;
		}
	break;
	case VT_CSI_PRIV:
		//if (vt_csi_priv(c))
			self->state = VT_DEF;
	break;
	case VT_SCS_G0:
		//charset_G0 = c;
		//if (verbose)
		//	fprintf(stderr, "CSC G0=%c\n", c);
		self->state = VT_DEF;
	break;
	case VT_SCS_G1:
		//charset_G1 = c;
		//if (verbose)
		//	fprintf(stderr, "CSC G1=%c\n", c);
		self->state = VT_DEF;
	break;
	case VT52_ESC_Y:
		//if (vt52_esc_y(c))
			self->state = VT_DEF;
	break;
	}
}

void mt_parse(struct mt_parser *self, const char *buf, size_t buf_sz)
{
	size_t i;

	for (i = 0; i < buf_sz; i++)
		next_char(self, buf[i]);
}
