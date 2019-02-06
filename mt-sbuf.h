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

	struct mt_char cur_char;

	struct mt_screen *screen;

	size_t sbuf_sz;
	size_t sbuf_off;
	struct mt_char *sbuf;
};

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

void mt_sbuf_cursor_move(struct mt_sbuf *self, mt_coord col_inc, mt_coord row_inc);

void mt_sbuf_newline(struct mt_sbuf *self);

void mt_sbuf_cursor_set(struct mt_sbuf *self, mt_coord col, mt_coord row);

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

void mt_sbuf_backspace(struct mt_sbuf *self);

void mt_sbuf_dump_screen(struct mt_sbuf *self);

#endif /* MT_SBUF__ */
