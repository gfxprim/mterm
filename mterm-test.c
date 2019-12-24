#include <string.h>
#include <errno.h>
#include "mt-common.h"
#include "mt-screen.h"
#include "mt-sbuf.h"
#include "mt-parser.h"

static int verbose;

static void make_buf(char *buf)
{
	unsigned int in = 0;
	unsigned int out = 0;

	while (buf[in]) {
		switch (buf[in]) {
		case '\\':
			switch (buf[in+1]) {
			case 'n':
				buf[out++] = '\n';
				in+=2;
			break;
			case 'r':
				buf[out++] = '\r';
				in+=2;
			break;
			case 'e':
				buf[out++] = '\e';
				in+=2;
			break;
			case 'b':
				buf[out++] = 0x08;
				in+=2;
			break;
			/* Two digit octal e.g. \016 */
			case '0':
				buf[out++] = 8*(buf[in+2] - '0') + buf[in+3] - '0';
				in+=4;
			break;
			default:
				goto next;
			}
		break;
		default:
next:
			buf[out++] = buf[in++];
		break;
		}
	}

	buf[out-1] = 0;
}

static int cursor_fail;

static void track_cursor(mt_coord col, mt_coord row, uint8_t set)
{
	static mt_coord scol, srow;
	static int saved;
	static int first;

	if (!first) {
		first = 1;
		return;
	}

	if (verbose)
		fprintf(stderr, "Cursor set %u %u %i\n", col, row, set);

	if (set && saved) {
		fprintf(stderr, "Cursor set twice!\n");
		cursor_fail++;
		return;
	}

	if (!set && !saved) {
		fprintf(stderr, "Cursor unset twice!\n");
		cursor_fail++;
		return;
	}

	if (set) {
		scol = col;
		srow = row;
		saved = 1;
	} else {
		if (scol != col || srow != row)
			cursor_fail++;
		saved = 0;
	}
}

static struct mt_screen screen = {
	.cursor = track_cursor,
};

int main(int argc, char *argv[])
{
	struct mt_sbuf *sbuf;
	unsigned int cols, rows;
	char buf[1024];
	const char *fname = argv[1];

	sbuf = mt_sbuf_alloc();
	if (!sbuf)
		MT_ERROR_MALLOC;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s [-v] input_file.txt\n", argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "-v")) {
		fname = argv[2];
		verbose = 1;
	}

	FILE *f = fopen(fname, "r");

	if (!f) {
		fprintf(stderr, "Can't open '%s': %s\n",
			fname, strerror(errno));
		return 1;
	}

	fscanf(f, "%u\n%u%*c", &cols, &rows);

	sbuf->screen = &screen;

	if (mt_sbuf_resize(sbuf, cols, rows))
		MT_ERROR_MALLOC;

	struct mt_parser parser;

	mt_parser_init(&parser, sbuf, 0, 0);

	while (fgets(buf, sizeof(buf), f)) {
		make_buf(buf);
		mt_parse(&parser, buf, strlen(buf));
	}

	fclose(f);

	mt_sbuf_dump_screen(sbuf);

	free(sbuf->sbuf);
	free(sbuf);

	if (cursor_fail) {
		fprintf(stderr, "Cursor not unset!\n");
		return 1;
	}

	return 0;
}
