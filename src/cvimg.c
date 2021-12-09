// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * cvimg.c - generate firmware for Realtek based devices
 *           with a header and checksum
 *
 * based on cvimg.c in bootcode of NEC Aterm WG1200HS4
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <endian.h>
#include <byteswap.h>
#include <errno.h>
#include <limits.h>

/* FW_SIGNATURE */
#define DEF_SIGNATURE		"cs6c"
#define SIGNATURE_LEN		4
#define HEADER_LEN		16

#define BUF_LEN			0x10000

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#  define HOST_TO_BE16(x)	bswap_16(x)
#  define HOST_TO_BE32(x)	bswap_32(x)
#else
#  define HOST_TO_BE16(x)	(x)
#  define HOST_TO_BE32(x)	(x)
#endif

struct cvimg_header {
	unsigned char signature[SIGNATURE_LEN];
	unsigned int startAddr;
	unsigned int burnAddr;
	unsigned int len;
} __attribute__((packed));

static unsigned long calcSum(char *buf, long len)
{
	unsigned long sum = 0;

	while (len > 1) {
		sum += HOST_TO_BE16(*(unsigned short *)buf);
		buf += 2;

		len -= 2;
		/* for left-over byte */
		if (len == 1)
			sum += *buf;
	}

	return sum;
}

static int strtou32(char *str, unsigned int *val)
{
	char *endptr = NULL;
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(str, &endptr, 0);

	if (errno || endptr == str || (*endptr && (endptr != NULL)))
		return -1;
	if (tmp > UINT_MAX)
		return -1;

	*val = (unsigned int)tmp;

	return 0;
}

static void __attribute__((noreturn)) print_usage(void)
{
	printf("Usage: cvimg -i <input> -o <output> -s <startaddr> -b <burnaddr> [-S <signature>]\n");
	printf("<signature>: user-specified signature (4 characters), default is \"%s\"\n",
	       DEF_SIGNATURE);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int c, ret = EXIT_SUCCESS;
	size_t size, n;
	struct stat status;
	unsigned int startAddr, burnAddr;
	unsigned long sum = 0;
	unsigned short checksum;
	char *ifn = NULL, *ofn = NULL;
	FILE *in, *out;
	char buf[BUF_LEN];
	struct cvimg_header header = { DEF_SIGNATURE };

	errno = 0;
	while ((c = getopt(argc, argv, "i:o:b:s:S:")) != -1) {
		switch (c) {
		case 'i':
			ifn = optarg;
			break;
		case 'o':
			ofn = optarg;
			break;
		case 'b':
			if (strtou32(optarg, &burnAddr)) {
				fprintf(stderr, "invalid burn-addr specified\n");
				print_usage();
			}
			break;
		case 's':
			if (strtou32(optarg, &startAddr)) {
				fprintf(stderr, "invalid start-addr specified\n");
				print_usage();
			}
			break;
		case 'S':
			if (strlen(optarg) != 4) {
				fprintf(stderr, "signature must be 4 characters long\n");
				print_usage();
			}
			memcpy(header.signature, optarg, 4);
			break;
		default:
			print_usage();
			break;
		}
	}

	if (!ifn || !ofn) {
		fprintf(stderr, "no input or output file specified\n");
		print_usage();
	}

	/* check input file */
	if (stat(ifn, &status) < 0) {
		perror("cannot stat file");
		print_usage();
	}

	size = status.st_size + sizeof(checksum);
	/* padding */
	if (size % 2)
		size++;

	if (size > UINT_MAX) {
		fprintf(stderr, "input file is too large\n");
		return EXIT_FAILURE;
	}

	header.burnAddr = HOST_TO_BE32(burnAddr);
	header.startAddr = HOST_TO_BE32(startAddr);
	header.len = HOST_TO_BE32(status.st_size + sizeof(checksum));

	memcpy(buf, &header, HEADER_LEN);

	in = fopen(ifn, "r");
	if (!in) {
		perror("failed to open input file");
		ret = EXIT_FAILURE;
		goto out;
	}

	out = fopen(ofn, "w");
	if (!out) {
		perror("failed to open output file");
		ret = EXIT_FAILURE;
		goto out;
	}

	/* write header to output file */
	if (fwrite(buf, 1, HEADER_LEN, out) != HEADER_LEN) {
		fprintf(stderr, "failed to write header\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	/* write data to output file */
	while ((n = fread(buf, 1, BUF_LEN, in)) > 0) {
		sum += calcSum(buf, n);

		/* padding */
		if (n % 2) {
			n++;
			buf[n] = 0;
		}

		if (fwrite(buf, 1, n, out) != n) {
			fprintf(stderr, "failed to write data\n");
			ret = EXIT_FAILURE;
			goto out;
		}
	}

	checksum = (unsigned short)((~sum & 0xFFFF) + 1);

	checksum = HOST_TO_BE16(checksum);
	memcpy(buf, &checksum, sizeof(checksum));

	/* write checksum to output file */
	if (fwrite(buf, 1, sizeof(checksum), out) != sizeof(checksum)) {
		fprintf(stderr, "failed to write checksum\n");
		ret = EXIT_FAILURE;
		goto out;
	}

	printf("data len : %ld bytes\n"
	       "total len: %ld bytes\n"
	       "checksum : 0x%04X\n",
	       size - sizeof(checksum),
	       size + HEADER_LEN,
	       HOST_TO_BE16(checksum));

out:
	if (in)
		fclose(in);
	if (out)
		fclose(out);

	return ret;
}

