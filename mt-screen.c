#include "mt-screen.h"

void mt_damage_merge(struct mt_damage *self,
                     mt_coord s_col, mt_coord s_row,
                     mt_coord e_col, mt_coord e_row)
{
	if (self->s_col < 0) {
		self->s_col = s_col;
		self->s_row = s_row;
		self->e_col = e_col;
		self->e_row = e_row;
		return;
	}

	//printf("Merging damage %ix%i-%ix%i %ix%i-%ix%i\n",
	//	self->s_col, self->s_row, self->e_col, self->e_row,
	//	s_col, s_row, e_col, e_row);

	self->s_col = MT_MIN(self->s_col, s_col);
	self->s_row = MT_MIN(self->s_row, s_row);
	self->e_col = MT_MAX(self->e_col, e_col);
	self->e_row = MT_MAX(self->e_row, e_row);
}

void mt_damage_scroll(struct mt_damage *self, int lines)
{
	self->scroll += lines;

	if (self->e_row <= lines) {
		self->s_col = -1;
		return;
	}

	if (self->s_row <= lines)
		self->s_row = 0;
	else
		self->s_row -= lines;

	self->e_row -= lines;
}
