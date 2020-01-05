#ifndef MT_SBUF__
#define MT_SBUF__

#include <stdint.h>
#include <stdlib.h>
#include "mt-common.h"

struct mt_char {
	uint8_t c;

	uint8_t bold:1;
	uint8_t reverse:1;
	uint8_t fg_col:3;
	uint8_t bg_col:3;
};

static inline uint8_t mt_char_fg_col(const struct mt_char *c)
{
	return c->fg_col;
}

static inline uint8_t mt_char_bg_col(const struct mt_char *c)
{
	return c->bg_col;
}

static inline uint8_t mt_char_bold(const struct mt_char *c)
{
	return c->bold;
}

static inline uint8_t mt_char_reverse(const struct mt_char *c)
{
	return c->reverse;
}

static inline char mt_char_c(const struct mt_char *c)
{
	return c->c;
}

struct mt_screen;

struct mt_sbuf {
	mt_coord cols;
	mt_coord rows;

	mt_coord cur_col;
	mt_coord cur_row;

	/* G0 and G1 charsets */
	char charset[2];
	uint8_t sel_charset;

	uint8_t cursor_hidden:1;
	uint8_t autowrap:1;

	struct mt_char cur_char;

	struct mt_screen *screen;

	size_t sbuf_sz;
	size_t sbuf_off;
	struct mt_char *sbuf;
};

/*
 * Sets charset at G0 or G1 slot to 'A' 'B' or '0'.
 */
static inline void mt_sbuf_set_charset(struct mt_sbuf *self, unsigned int Gx, char charset)
{
	self->charset[Gx] = charset;
}

/*
 * Set currect charset to G0 slot.
 */
static inline void mt_sbuf_shift_in(struct mt_sbuf *self)
{
	self->sel_charset = 0;
}

/*
 * Set current charset to G1 slot.
 */
static inline void mt_sbuf_shift_out(struct mt_sbuf *self)
{
	self->sel_charset = 1;
}

static inline void mt_sbuf_autowrap(struct mt_sbuf *self, uint8_t autowrap)
{
	self->autowrap = !!autowrap;
}

/*
 * Returns current charset
 */
static inline char mt_sbuf_charset(struct mt_sbuf *self)
{
	return self->sel_charset ? self->charset[1] : self->charset[0];
}

static inline mt_coord mt_sbuf_cursor_col(struct mt_sbuf *self)
{
	return self->cur_col;
}

/*
 * DECSTR soft terminal reset.
 */
static inline void mt_sbuf_DECSTR(struct mt_sbuf *self)
{
	//TODO!
}

void mt_sbuf_insert_blank(struct mt_sbuf *self, uint16_t blanks);

void mt_sbuf_del_chars(struct mt_sbuf *self, uint16_t dels);

static inline struct mt_char *mt_sbuf_row(struct mt_sbuf *self, mt_coord row)
{
	return &self->sbuf[((row + self->sbuf_off) % self->rows) * self->cols];
}

static inline struct mt_char *mt_sbuf_char(struct mt_sbuf *self,
                                           mt_coord col, mt_coord row)
{
	struct mt_char *crow = mt_sbuf_row(self, row);

	return &crow[col];
}

static inline void mt_sbuf_bold(struct mt_sbuf *self, int bold)
{
	self->cur_char.bold = bold;
}

static inline void mt_sbuf_reverse(struct mt_sbuf *self, int reverse)
{
	self->cur_char.reverse = reverse;
}

static inline void mt_sbuf_bg_col(struct mt_sbuf *self, uint8_t bg_col)
{
	self->cur_char.bg_col = bg_col;
}

static inline void mt_sbuf_fg_col(struct mt_sbuf *self, uint8_t fg_col)
{
	self->cur_char.fg_col = fg_col;
}

static inline const struct mt_char *mt_sbuf_cur_char(struct mt_sbuf *self)
{
	return &self->cur_char;
}

struct mt_sbuf *mt_sbuf_alloc(void);

int mt_sbuf_resize(struct mt_sbuf *self, unsigned int n_cols, unsigned int n_rows);

int mt_sbuf_cursor_move(struct mt_sbuf *self, mt_coord col_inc, mt_coord row_inc);

void mt_sbuf_cursor_up(struct mt_sbuf *self);

void mt_sbuf_cursor_down(struct mt_sbuf *self);

void mt_sbuf_newline(struct mt_sbuf *self);

void mt_sbuf_cursor_set(struct mt_sbuf *self, mt_coord col, mt_coord row);

void mt_sbuf_cursor_visible(struct mt_sbuf *self, uint8_t visible);

enum mt_sbuf_erase_t {
	MT_SBUF_ERASE_CUR_LEFT,
	MT_SBUF_ERASE_CUR_RIGHT,
	MT_SBUF_ERASE_LINE,
	MT_SBUF_ERASE_END,
	MT_SBUF_ERASE_START,
	MT_SBUF_ERASE_SCREEN,
};

void mt_sbuf_erase(struct mt_sbuf *self, enum mt_sbuf_erase_t type);

void mt_sbuf_putc(struct mt_sbuf *self, const char c);

void mt_sbuf_dump_screen(struct mt_sbuf *self);

/*
 * RIS reset to initial state.
 */
static inline void mt_sbuf_RIS(struct mt_sbuf *self)
{
	self->sel_charset = 0;
	self->charset[0] = 'B';
	self->charset[1] = '0';

	mt_sbuf_erase(self, MT_SBUF_ERASE_SCREEN);
	mt_sbuf_cursor_set(self, 0, 0);
}

#endif /* MT_SBUF__ */
