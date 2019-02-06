#include <string.h>
#include <errno.h>
#include "mt-common.h"
#include "mt-sbuf.h"
#include "mt-parser.h"

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

int main(int argc, char *argv[])
{
	struct mt_sbuf *sbuf;
	unsigned int cols, rows;
	char buf[1024];

	sbuf = mt_sbuf_alloc();
	if (!sbuf)
		MT_ERROR_MALLOC;

	if (argc != 2) {
		fprintf(stderr, "usage: %s input_file.txt\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "r");

	if (!f) {
		fprintf(stderr, "Can't open '%s': %s\n",
			argv[1], strerror(errno));
		return 1;
	}

	fscanf(f, "%u\n%u%*c", &cols, &rows);

	if (mt_sbuf_resize(sbuf, cols, rows))
		MT_ERROR_MALLOC;

	struct mt_parser parser;

	memset(&parser, 0, sizeof(parser));

	parser.sbuf = sbuf;

	while (fgets(buf, sizeof(buf), f)) {
		make_buf(buf);
		mt_parse(&parser, buf, strlen(buf));
	}

	fclose(f);

	mt_sbuf_dump_screen(sbuf);

	return 0;
}
