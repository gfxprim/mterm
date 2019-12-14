#include <string.h>
#include <ctype.h>
#include "mt-sbuf.h"
#include "mt-screen.h"

struct mt_sbuf *mt_sbuf_alloc(void)
{
	struct mt_sbuf *self;

	self = malloc(sizeof(struct mt_sbuf));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	return self;
}

int mt_sbuf_resize(struct mt_sbuf *self, unsigned int n_cols, unsigned int n_rows)
{
	struct mt_char *new_buf;
	size_t sbuf_sz = n_cols * (2 * n_rows) * sizeof(struct mt_char);

	new_buf = malloc(sbuf_sz);
	if (!new_buf)
		return 1;

	memset(new_buf, 0, sbuf_sz);

	if (self->sbuf) {
		mt_coord row;
		size_t min_cols = MT_MIN(self->cols, (int)n_cols);

		for (row = 0; row < self->rows; row++) {
			struct mt_char *o_row = &self->sbuf[row * self->cols];
			struct mt_char *n_row = &new_buf[row * n_cols];
			memcpy(n_row, o_row, min_cols * sizeof(struct mt_char));
		}

		free(self->sbuf);
	}

	self->sbuf = new_buf;
	self->sbuf_sz = sbuf_sz;

	if (self->screen) {
		self->screen->damage(self->screen->priv, 0, 0, n_cols, n_rows);
		//TODO: Redraw!
	}

	self->cols = n_cols;
	self->rows = n_rows;

	return 0;
}

static void mt_sbuf_scroll(struct mt_sbuf *self, mt_coord inc)
{
	struct mt_char *row;

	self->sbuf_off = (self->sbuf_off + 1) % (self->sbuf_sz / self->cols);

	row = mt_sbuf_row(self, self->rows - 1);

	memset(row, 0, sizeof(struct mt_char) * self->cols);

	if (self->screen)
		self->screen->scroll(self->screen->priv, 1);
}

static void unset_cursor(struct mt_sbuf *self)
{
	struct mt_char *cur = mt_sbuf_char(self, self->cur_col, self->cur_row);

	cur->reverse = 0;

	if (self->screen)
		self->screen->cursor(self->cur_col, self->cur_row);
}

static void set_cursor(struct mt_sbuf *self)
{
	struct mt_char *cur = mt_sbuf_char(self, self->cur_col, self->cur_row);

	cur->reverse = 1;

	if (cur->c == 0) {
		cur->fg_col = self->cur_char.fg_col;
		cur->bg_col = self->cur_char.bg_col;
	}

	if (self->screen)
		self->screen->cursor(self->cur_col, self->cur_row);
}

void mt_sbuf_cursor_move(struct mt_sbuf *self, mt_coord col_inc, mt_coord row_inc)
{
	unset_cursor(self);

	self->cur_col += col_inc;
	self->cur_row += row_inc;

	if (self->cur_col < 0)
		self->cur_col = 0;

	if (self->cur_row < 0)
		self->cur_row = 0;

	if (self->cur_col >= self->cols)
		self->cur_col = self->cols - 1;

	if (self->cur_row >= self->rows)
		self->cur_row = self->rows - 1;

	set_cursor(self);
}

void mt_sbuf_newline(struct mt_sbuf *self)
{
	unset_cursor(self);

	self->cur_col = 0;
	self->cur_row++;

	if (self->cur_row >= self->rows) {
		mt_sbuf_scroll(self, 1);
		self->cur_row--;
	}

	set_cursor(self);
}

void mt_sbuf_cursor_inc(struct mt_sbuf *self)
{
	unset_cursor(self);

	self->cur_col++;

	if (self->cur_col >= self->cols) {
		self->cur_col = 0;
		self->cur_row++;
	}

	if (self->cur_row >= self->rows) {
		mt_sbuf_scroll(self, 1);
		self->cur_row--;
	}

	set_cursor(self);
}

void mt_sbuf_cursor_set(struct mt_sbuf *self, mt_coord col, mt_coord row)
{
	unset_cursor(self);

	if (col >= 0 && col < self->cols)
		self->cur_col = col;

	if (row >= 0 && row < self->rows)
		self->cur_row = row;

	set_cursor(self);
}

void mt_sbuf_putc(struct mt_sbuf *self, const char c)
{
	struct mt_char *mc;

	mc = mt_sbuf_char(self, self->cur_col, self->cur_row);

	self->cur_char.c = c;

	*mc = self->cur_char;

	if (mc->reverse) {
		uint8_t bg = mc->bg_col;
		mc->bg_col = mc->fg_col;
		mc->fg_col = bg;
		mc->reverse = 0;
	}

	if (self->screen)
		self->screen->damage(self->screen->priv, self->cur_col, self->cur_row, self->cur_col+1, self->cur_row+1);

	mt_sbuf_cursor_inc(self);
}

static void erase(struct mt_sbuf *self, mt_coord s_col, mt_coord s_row,
                  mt_coord e_col, mt_coord e_row)
{
	struct mt_char *row;
	mt_coord r;

	if (self->screen)
		self->screen->erase(s_col, s_row, e_col, e_row);

	for (r = s_row; r <= e_row; r++) {
		row = mt_sbuf_row(self, r);
		memset(row + s_col, 0, (e_col - s_col + 1) * sizeof(*row));
	}
}

void mt_sbuf_erase(struct mt_sbuf *self, enum mt_sbuf_erase_t type)
{
	switch (type) {
	case MT_SBUF_ERASE_CUR_LEFT:
		erase(self, 0, self->cur_row, self->cur_col, self->cur_row);
	break;
	case MT_SBUF_ERASE_CUR_RIGHT:
		erase(self, self->cur_col, self->cur_row, self->cols - 1, self->cur_row);
	break;
	case MT_SBUF_ERASE_LINE:
		erase(self, 0, self->cur_row, self->cols - 1, self->cur_row);
	break;
	case MT_SBUF_ERASE_END:
		mt_sbuf_erase(self, MT_SBUF_ERASE_CUR_RIGHT);
		if (self->cur_row + 1 < self->rows)
			erase(self, 0, self->cur_row + 1, self->cols - 1, self->rows - 1);
	break;
	case MT_SBUF_ERASE_START:
		mt_sbuf_erase(self, MT_SBUF_ERASE_CUR_LEFT);
		if (self->cur_row - 1 >= 0)
			erase(self, 0, 0, self->cols - 1, self->cur_row - 1);
	break;
	case MT_SBUF_ERASE_SCREEN:
		erase(self, 0, 0, self->cols - 1, self->rows - 1);
	break;
	}
}

void mt_sbuf_backspace(struct mt_sbuf *self)
{
	if (self->cur_col <= 0)
		return;

	erase(self, self->cur_col-1, self->cur_row, self->cur_col, self->cur_row+1);
	mt_sbuf_cursor_move(self, -1, 0);
}

void mt_sbuf_dump_screen(struct mt_sbuf *self)
{
	int col, row;

	printf(" ");

	for (col = 0; col < self->cols; col++)
		printf("-");

	printf("\n");

	for (row = 0; row < self->rows; row++) {
		printf("|");
		struct mt_char *c = mt_sbuf_row(self, row);
		for (col = 0; col < self->cols; col++) {
			if (isprint(mt_char_c(&c[col])))
				printf("%c", c[col].c);
			else
				printf(" ");
		}
		printf("|\n");
	}

	printf(" ");

	for (col = 0; col < self->cols; col++)
		printf("-");

	printf("\n");
	printf("size %ux%u cursor %ux%u\n", self->rows, self->cols, self->cur_row, self->cur_col);
}
