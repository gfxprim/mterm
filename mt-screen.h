#ifndef MT_SCREEN__
#define MT_SCREEN__

#include "mt-common.h"

struct mt_screen {
	void (*damage)(void *priv, mt_coord s_col, mt_coord s_row, mt_coord e_col, mt_coord e_row);
	void (*scroll)(void *priv, int lines);
	void (*cursor)(mt_coord o_col, mt_coord o_row, mt_coord n_col, mt_coord n_row);
	void (*erase)(mt_coord o_col, mt_coord o_row, mt_coord n_col, mt_coord n_row);

	void *priv;
};

struct mt_damage {
	mt_coord s_col, s_row;
	mt_coord e_col, e_row;
	int scroll;
};

static inline void mt_damage_reset(struct mt_damage *self)
{
	self->s_col = -1;
	self->e_col = -1;
	self->s_row = -1;
	self->e_row = -1;
	self->scroll = 0;
}

void mt_damage_merge(struct mt_damage *self,
                     mt_coord s_col, mt_coord s_row,
                     mt_coord e_col, mt_coord e_row);

void mt_damage_scroll(struct mt_damage *self, int lines);

#endif /* MT_SCREEN__ */
