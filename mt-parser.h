#ifndef MT_PARSER__
#define MT_PARSER__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mt-sbuf.h"
#include "mt-common.h"

struct mt_sbuf;

enum mt_state {
	VT_DEF,
	VT_ESC,
	VT_CSI,
	VT_CSI_PRIV,
	VT_SCS_G0,
	VT_SCS_G1,
	VT52_ESC_Y,
};

struct mt_parser {
	struct mt_sbuf *sbuf;
	enum mt_state state;
	int charset;

	uint8_t fg_col:3;
	uint8_t bg_col:3;
};

static inline void mt_parser_init(struct mt_parser *parser, struct mt_sbuf *sbuf,
                                  uint8_t fg_col, uint8_t bg_col)
{
	memset(parser, 0, sizeof(parser));
	parser->sbuf = sbuf;
	parser->bg_col = bg_col;
	parser->fg_col = fg_col;

	mt_sbuf_bg_col(sbuf, bg_col);
	mt_sbuf_fg_col(sbuf, fg_col);
}

void mt_parse(struct mt_parser *self, const char *buf, size_t buf_sz);

#endif /* MT_PARSER__ */
