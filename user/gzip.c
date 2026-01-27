/*
 * gzip/gunzip - compress/decompress files using miniz deflate
 * Usage: gzip file    -> creates file.gz
 *        gunzip file.gz -> extracts to file
 */

#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../third_party/miniz/miniz.h"
#include "../third_party/miniz/miniz.c"

/* Simple gzip header (10 bytes) */
static const unsigned char gzip_header[] = {
    0x1f, 0x8b,  /* magic */
    0x08,        /* deflate */
    0x00,        /* flags */
    0x00, 0x00, 0x00, 0x00,  /* mtime */
    0x00,        /* xfl */
    0xff         /* OS unknown */
};

static void usage_gzip(void) {
    fprintf(stderr, "Usage: gzip [-d] [-k] file\n");
    fprintf(stderr, "  -d    decompress (same as gunzip)\n");
    fprintf(stderr, "  -k    keep original file\n");
    fprintf(stderr, "  -h    show this help\n");
}

static void usage_gunzip(void) {
    fprintf(stderr, "Usage: gunzip [-k] file.gz\n");
    fprintf(stderr, "  -k    keep original file\n");
    fprintf(stderr, "  -h    show this help\n");
}

static unsigned int crc32_buf(const unsigned char *buf, size_t len) {
    return (unsigned int)mz_crc32(MZ_CRC32_INIT, buf, len);
}

static int do_compress(const char *infile, int keep) {
    FILE *fin = fopen(infile, "rb");
    if (!fin) {
        fprintf(stderr, "gzip: %s: %s\n", infile, strerror(errno));
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long insize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (insize < 0) {
        fprintf(stderr, "gzip: %s: cannot determine size\n", infile);
        fclose(fin);
        return 1;
    }

    unsigned char *inbuf = NULL;
    if (insize > 0) {
        inbuf = (unsigned char *)malloc((size_t)insize);
        if (!inbuf) {
            fprintf(stderr, "gzip: out of memory\n");
            fclose(fin);
            return 1;
        }
        if (fread(inbuf, 1, (size_t)insize, fin) != (size_t)insize) {
            fprintf(stderr, "gzip: %s: read error\n", infile);
            free(inbuf);
            fclose(fin);
            return 1;
        }
    }
    fclose(fin);

    /* Compress */
    mz_ulong compsize = mz_compressBound((mz_ulong)insize);
    unsigned char *compbuf = (unsigned char *)malloc(compsize);
    if (!compbuf) {
        fprintf(stderr, "gzip: out of memory\n");
        free(inbuf);
        return 1;
    }

    int status = mz_compress2(compbuf, &compsize, inbuf, (mz_ulong)insize, MZ_DEFAULT_COMPRESSION);
    if (status != MZ_OK) {
        fprintf(stderr, "gzip: compression failed\n");
        free(compbuf);
        free(inbuf);
        return 1;
    }

    /* Skip zlib header (2 bytes) and trailer (4 bytes) to get raw deflate */
    unsigned char *deflate_data = compbuf + 2;
    mz_ulong deflate_size = compsize - 6;

    /* Calculate CRC32 of original data */
    unsigned int crc = crc32_buf(inbuf, (size_t)insize);

    /* Write gzip file */
    char outfile[512];
    snprintf(outfile, sizeof(outfile), "%s.gz", infile);

    FILE *fout = fopen(outfile, "wb");
    if (!fout) {
        fprintf(stderr, "gzip: %s: %s\n", outfile, strerror(errno));
        free(compbuf);
        free(inbuf);
        return 1;
    }

    fwrite(gzip_header, 1, sizeof(gzip_header), fout);
    fwrite(deflate_data, 1, deflate_size, fout);

    /* Write CRC32 and original size (little endian) */
    unsigned char trailer[8];
    trailer[0] = crc & 0xff;
    trailer[1] = (crc >> 8) & 0xff;
    trailer[2] = (crc >> 16) & 0xff;
    trailer[3] = (crc >> 24) & 0xff;
    trailer[4] = insize & 0xff;
    trailer[5] = (insize >> 8) & 0xff;
    trailer[6] = (insize >> 16) & 0xff;
    trailer[7] = (insize >> 24) & 0xff;
    fwrite(trailer, 1, 8, fout);

    fclose(fout);
    free(compbuf);
    free(inbuf);

    printf("%s -> %s\n", infile, outfile);

    if (!keep) {
        remove(infile);
    }

    return 0;
}

static int do_decompress(const char *infile, int keep) {
    /* Check .gz extension */
    size_t len = strlen(infile);
    if (len < 4 || strcmp(infile + len - 3, ".gz") != 0) {
        fprintf(stderr, "gunzip: %s: unknown suffix -- ignored\n", infile);
        return 1;
    }

    FILE *fin = fopen(infile, "rb");
    if (!fin) {
        fprintf(stderr, "gunzip: %s: %s\n", infile, strerror(errno));
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long insize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (insize < 18) {
        fprintf(stderr, "gunzip: %s: file too small\n", infile);
        fclose(fin);
        return 1;
    }

    unsigned char *inbuf = (unsigned char *)malloc((size_t)insize);
    if (!inbuf) {
        fprintf(stderr, "gunzip: out of memory\n");
        fclose(fin);
        return 1;
    }

    if (fread(inbuf, 1, (size_t)insize, fin) != (size_t)insize) {
        fprintf(stderr, "gunzip: %s: read error\n", infile);
        free(inbuf);
        fclose(fin);
        return 1;
    }
    fclose(fin);

    /* Check gzip magic */
    if (inbuf[0] != 0x1f || inbuf[1] != 0x8b) {
        fprintf(stderr, "gunzip: %s: not in gzip format\n", infile);
        free(inbuf);
        return 1;
    }

    if (inbuf[2] != 0x08) {
        fprintf(stderr, "gunzip: %s: unknown compression method\n", infile);
        free(inbuf);
        return 1;
    }

    /* Parse header */
    unsigned char flags = inbuf[3];
    size_t pos = 10;

    if (flags & 0x04) { /* FEXTRA */
        if (pos + 2 > (size_t)insize) goto truncated;
        size_t xlen = inbuf[pos] | (inbuf[pos + 1] << 8);
        pos += 2 + xlen;
    }
    if (flags & 0x08) { /* FNAME */
        while (pos < (size_t)insize && inbuf[pos]) pos++;
        pos++;
    }
    if (flags & 0x10) { /* FCOMMENT */
        while (pos < (size_t)insize && inbuf[pos]) pos++;
        pos++;
    }
    if (flags & 0x02) { /* FHCRC */
        pos += 2;
    }

    if (pos >= (size_t)insize - 8) {
truncated:
        fprintf(stderr, "gunzip: %s: file truncated\n", infile);
        free(inbuf);
        return 1;
    }

    /* Get original size from trailer */
    size_t orig_size = inbuf[insize - 4] |
                       (inbuf[insize - 3] << 8) |
                       (inbuf[insize - 2] << 16) |
                       (inbuf[insize - 1] << 24);

    /* Decompress */
    unsigned char *outbuf = (unsigned char *)malloc(orig_size + 1);
    if (!outbuf) {
        fprintf(stderr, "gunzip: out of memory\n");
        free(inbuf);
        return 1;
    }

    mz_ulong dest_len = (mz_ulong)orig_size;
    int status = mz_uncompress(outbuf, &dest_len, inbuf + pos - 2, (mz_ulong)(insize - pos - 8 + 2));

    /* If zlib uncompress fails, try raw inflate */
    if (status != MZ_OK) {
        dest_len = (mz_ulong)orig_size;
        tinfl_decompressor decomp;
        tinfl_init(&decomp);

        size_t in_bytes = insize - pos - 8;
        size_t out_bytes = orig_size;

        tinfl_status tstat = tinfl_decompress(&decomp,
            inbuf + pos, &in_bytes,
            outbuf, outbuf, &out_bytes,
            TINFL_FLAG_PARSE_ZLIB_HEADER);

        if (tstat < 0) {
            /* Try without zlib header */
            tinfl_init(&decomp);
            in_bytes = insize - pos - 8;
            out_bytes = orig_size;
            tstat = tinfl_decompress(&decomp,
                inbuf + pos, &in_bytes,
                outbuf, outbuf, &out_bytes,
                0);
        }

        if (tstat < 0) {
            fprintf(stderr, "gunzip: %s: decompression failed\n", infile);
            free(outbuf);
            free(inbuf);
            return 1;
        }
        dest_len = (mz_ulong)out_bytes;
    }

    free(inbuf);

    /* Write output file */
    char outfile[512];
    snprintf(outfile, sizeof(outfile), "%.*s", (int)(len - 3), infile);

    FILE *fout = fopen(outfile, "wb");
    if (!fout) {
        fprintf(stderr, "gunzip: %s: %s\n", outfile, strerror(errno));
        free(outbuf);
        return 1;
    }

    if (dest_len > 0) {
        if (fwrite(outbuf, 1, dest_len, fout) != dest_len) {
            fprintf(stderr, "gunzip: %s: write error\n", outfile);
            fclose(fout);
            free(outbuf);
            return 1;
        }
    }

    fclose(fout);
    free(outbuf);

    printf("%s -> %s\n", infile, outfile);

    if (!keep) {
        remove(infile);
    }

    return 0;
}

int main(int argc, char **argv) {
    /* Check if called as gunzip */
    const char *prog = argv[0];
    const char *base = strrchr(prog, '/');
    if (base) base++; else base = prog;

    int decompress = (strcmp(base, "gunzip") == 0);
    int keep = 0;
    const char *file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            decompress = 1;
        } else if (strcmp(argv[i], "-k") == 0) {
            keep = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            if (decompress) usage_gunzip(); else usage_gzip();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "%s: unknown option '%s'\n", base, argv[i]);
            return 1;
        } else {
            file = argv[i];
        }
    }

    if (!file) {
        if (decompress) usage_gunzip(); else usage_gzip();
        return 1;
    }

    if (decompress) {
        return do_decompress(file, keep);
    } else {
        return do_compress(file, keep);
    }
}
