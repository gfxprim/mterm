#ifndef MT_PARSER__
#define MT_PARSER__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mt-sbuf.h"
#include "mt-common.h"

struct mt_sbuf;

enum mt_state {
	VT_DEF = 0,
	VT_ESC = 0x01,
	VT_ESC_DEC = 0x02,
	VT_OSC = 0x03,

	/* CSI */
	VT_CSI_ENTRY = 0x04,
	VT_CSI_PARAM = 0x14,
	VT_CSI_IGNORE = 0x24,
	VT_CSI_INTERMEDIATE = 0x34,

	VT_SCS_G0 = 0x08,
	VT_SCS_G1 = 0x09,
	VT52_ESC_Y = 0x0A,
};

#define MT_MAX_CSI_PARS 10

struct mt_parser {
	struct mt_sbuf *sbuf;
	enum mt_state state;

	uint8_t fg_col:3;
	uint8_t bg_col:3;
	uint8_t par_t:1;

	/*
	 * Callback to optionally send response to application.
	 */
	void (*response)(int fd, const char *string);
	int response_fd;

	char csi_intermediate;
	uint16_t pars[MT_MAX_CSI_PARS];
	uint8_t par_cnt;
};

static inline void mt_parser_init(struct mt_parser *parser, struct mt_sbuf *sbuf,
                                  uint8_t fg_col, uint8_t bg_col)
{
	memset(parser, 0, sizeof(struct mt_parser));

	parser->sbuf = sbuf;
	parser->bg_col = bg_col;
	parser->fg_col = fg_col;

	sbuf->charset[0] = 'B';
	sbuf->charset[1] = '0';
	sbuf->sel_charset = 0;

	mt_sbuf_bg_col(sbuf, bg_col);
	mt_sbuf_fg_col(sbuf, fg_col);
}

void mt_parse(struct mt_parser *self, const char *buf, size_t buf_sz);

#endif /* MT_PARSER__ */
