// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * nosimg-enc.c - encode/decode "nos.img" image for XikeStor SKS8300 series
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define ENCODE_BLKLEN	0x100
#define ENCODE_BLOCKS	2

static uint32_t patterns[] = {
	0xeeddcc21,  0x5355eecc,  0xdd55807e,  0x00000000,
	0xcdbddfae,  0xbb9b8901,  0x70e5ccdd,  0xf6fc8364,
	0xecddcef1,  0xe354fed0,  0xbdabdde1,  0xe4b4d583,
	0xedfed0cd,  0xb655cca3,  0xedd5c67e,  0xddcc2153,
	0xec4ddc00,  0x5355cdc3,  0x2201807e,  0xefbc7566,
	0xa6c0cc2f,  0xfed0eecc,  0xdd550101,  0x0101c564,
	0x9945ab32,  0x55807eef,  0x55807eef,  0xbc756689,
	0xe31d83dd,  0xfe558eab,  0x7d55807e,  0xff01ac66,
	0x0ec992d9,  0x73e50101,  0xbde510ce,  0x0101bae8,
	0x3edd81a1,  0x53330101,  0x9ac510aa,  0x01ce8ae1,
	0xb1fb0080,  0x53770000,  0x70dc0001,  0x0000cbb1,
	0xa0300000,  0x55a60000,  0xcabd0101,  0x0000c9b2,
	0x81900100,  0x5a210001,  0x79bc0100,  0x78007bb3,
	0xd4970100,  0x5355a9fc,  0xdda501be,  0xafc175c5,
	0x8ed77700,  0x55d00dac,  0x0155807e,  0xefbc7ee6,
	0xf16c5200,  0x331698cc,  0x01010101,  0x00007988,
};

static void __attribute__((noreturn)) usage(void)
{
	fprintf(stderr, "Usage: nosimg-enc -i infile -o outfile [-d]\n");
	exit(EXIT_FAILURE);
}

static void EncodeBlock100(uint8_t *data, bool decode)
{
	int i;

	for (i = 0; i < ENCODE_BLKLEN; i++)
	{
		uint32_t tmp;
		tmp = patterns[i / sizeof(uint32_t)];
		tmp >>= (sizeof(uint32_t) - i % sizeof(uint32_t) - 1) * 8;
		data[i] -= (tmp & 0xff) * (decode ? -1 : 1);
	}
}

int main(int argc, char **argv)
{
	int i, c, n, ret = EXIT_SUCCESS;
	char *ifn = NULL, *ofn = NULL;
	bool decode = false;
	uint8_t buf[0x10000];
	FILE *out, *in;

	while ((c = getopt(argc, argv, "i:o:dh")) != -1) {
		switch (c) {
		case 'i':
			ifn = optarg;
			break;
		case 'o':
			ofn = optarg;
			break;
		case 'd':
			decode = true;
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (optind != argc || optind == 1) {
		fprintf(stderr, "illegal arg \"%s\"\n", argv[optind]);
		usage();
	}

	in = fopen(ifn, "r");
	if (!in) {
		perror("can not open input file");
		usage();
	}

	out = fopen(ofn, "w");
	if (!out) {
		perror("can not open output file");
		usage();
	}

	/* encode/decode the first 512 bytes (0x100 x2) */
	for (i = 0; i < ENCODE_BLOCKS; i++) {
		n = fread(buf, 1, ENCODE_BLKLEN, in);
		if (n < ENCODE_BLKLEN) {
			perror("failed to read data for encoding/decoding\n");
			ret = EXIT_FAILURE;
			goto out;
		}

		EncodeBlock100(buf, decode);
		fwrite(buf, 1, n, out);
	}

	/* copy the remaining data */
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			perror("failed to write");
			ret = EXIT_FAILURE;
			goto out;
		}
	}

	if (ferror(in)) {
		perror("failed to read");
		ret = EXIT_FAILURE;
	}

out:
	fclose(in);
	fclose(out);
	return ret;
}
