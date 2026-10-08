/*
 * rastertoqpdl.c - CUPS filter: CUPS raster -> Samsung QPDL v5 / JBIG.
 *
 * Called by CUPS as:  rastertoqpdl job user title copies options [file]
 *
 * Accepts 1-bit or 8-bit single-channel raster (K or W color space).
 * 8-bit input is reduced to 1 bit with Floyd-Steinberg dithering, or with
 * a plain threshold when the "Halftone" PPD option asks for it.
 *
 * PPD options arrive through the raster page header:
 *   cupsInteger0  toner save (0 off, 1 on)
 *   cupsInteger1  toner density 1..5 (0 = printer default 3)
 *   cupsInteger2  halftone (0 dither, 1 threshold)
 *   MediaPosition paper source (1 auto, 2 manual feeder)
 */
#include "qpdl.h"

#include <cups/cups.h>
#include <cups/raster.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t canceled;

static void on_term(int sig)
{
    (void)sig;
    canceled = 1;
}

static void set_bit(unsigned char *row, unsigned x)
{
    row[x >> 3] |= 0x80 >> (x & 7);
}

/*
 * Convert one 8-bit line (0 = white, 255 = black) to 1 bit, writing pixel
 * x at bit x + xoff of out.
 * cur/next hold the diffused error for this and the next row; pixel x is
 * stored at index x + 1 so that x - 1 never goes out of range.
 */
static void dither_line(const unsigned char *in, unsigned char *out,
                        unsigned w, unsigned xoff, int *cur, int *next,
                        int threshold)
{
    memset(next, 0, (w + 2) * sizeof(int));
    for (unsigned x = 0; x < w; x++) {
        int v = in[x] + (threshold ? 0 : cur[x + 1]);
        int on = v >= 128;
        int e = v - (on ? 255 : 0);

        if (on)
            set_bit(out, x + xoff);
        if (threshold)
            continue;
        cur[x + 2]  += e * 7 / 16;
        next[x]     += e * 3 / 16;
        next[x + 1] += e * 5 / 16;
        next[x + 2] += e / 16;
    }
}

/*
 * Read one raster page into a whole-page 1-bit bitmap. macOS renders only
 * the imageable area, so the raster lands at (xoff, yoff) on the page.
 */
static int read_page(cups_raster_t *ras, const cups_page_header2_t *hdr,
                     unsigned char *bitmap, unsigned stride,
                     unsigned xoff, unsigned yoff)
{
    unsigned w = hdr->cupsWidth, bpl = hdr->cupsBytesPerLine;
    int white = hdr->cupsColorSpace != CUPS_CSPACE_K;
    int threshold = hdr->cupsInteger[2] == 1;
    unsigned char *line = malloc(bpl);
    int *err0 = calloc(w + 2, sizeof(int));
    int *err1 = calloc(w + 2, sizeof(int));
    int ret = -1;

    if (!line || !err0 || !err1)
        goto done;

    for (unsigned y = 0; y < hdr->cupsHeight; y++) {
        unsigned char *row = bitmap + (size_t)(y + yoff) * stride;

        if (canceled || cupsRasterReadPixels(ras, line, bpl) != bpl)
            goto done;

        if (hdr->cupsBitsPerColor == 1) {
            for (unsigned x = 0; x < w; x++) {
                int on = (line[x >> 3] >> (7 - (x & 7))) & 1;
                if (on != white)
                    set_bit(row, x + xoff);
            }
        } else {
            int *tmp;

            if (white)
                for (unsigned i = 0; i < w; i++)
                    line[i] = 255 - line[i];
            dither_line(line, row, w, xoff, err0, err1, threshold);
            tmp = err0;
            err0 = err1;
            err1 = tmp;
        }
    }
    ret = 0;

done:
    free(line);
    free(err0);
    free(err1);
    return ret;
}

int main(int argc, char *argv[])
{
    cups_raster_t *ras;
    cups_page_header2_t hdr;
    int fd = 0, page = 0, started = 0, ret = 0;

    if (argc < 6 || argc > 7) {
        fputs("Usage: rastertoqpdl job user title copies options [file]\n", stderr);
        return 1;
    }
    if (argc == 7 && (fd = open(argv[6], O_RDONLY)) < 0) {
        perror("ERROR: Unable to open raster file");
        return 1;
    }

    signal(SIGTERM, on_term);
    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

    while (!canceled && cupsRasterReadHeader2(ras, &hdr)) {
        unsigned xres = hdr.HWResolution[0], yres = hdr.HWResolution[1];
        unsigned w = hdr.PageSize[0] * xres / 72;
        unsigned h = hdr.PageSize[1] * yres / 72;
        unsigned stride = (w + 7) / 8, xoff = 0, yoff = 0;
        unsigned char *bitmap;
        qpdl_page_t p;

        /* Raster smaller than the page: offset it by the imaging box. */
        if (hdr.cupsWidth < w || hdr.cupsHeight < h) {
            xoff = hdr.ImagingBoundingBox[0] * xres / 72;
            yoff = (hdr.PageSize[1] - hdr.ImagingBoundingBox[3]) * yres / 72;
        }
        if (xoff + hdr.cupsWidth > w || yoff + hdr.cupsHeight > h) {
            /* Bigger than the page we computed: trust the raster size. */
            w = hdr.cupsWidth;
            h = hdr.cupsHeight;
            stride = (w + 7) / 8;
            xoff = yoff = 0;
        }

        if (hdr.cupsBitsPerPixel != hdr.cupsBitsPerColor ||
            (hdr.cupsBitsPerColor != 1 && hdr.cupsBitsPerColor != 8) ||
            (hdr.cupsColorSpace != CUPS_CSPACE_K &&
             hdr.cupsColorSpace != CUPS_CSPACE_W &&
             hdr.cupsColorSpace != CUPS_CSPACE_SW)) {
            fprintf(stderr, "ERROR: Unsupported raster format (%u bpp, "
                    "color space %d)\n", hdr.cupsBitsPerPixel,
                    hdr.cupsColorSpace);
            ret = 1;
            break;
        }

        page++;
        fprintf(stderr, "INFO: Printing page %d\n", page);

        if (!started) {
            qpdl_job_t job = {
                argv[2], argv[3], (int)hdr.HWResolution[1],
                hdr.cupsInteger[0] != 0,
                hdr.cupsInteger[1] ? (int)hdr.cupsInteger[1] : 3,
            };
            qpdl_job_begin(stdout, &job);
            started = 1;
        }

        bitmap = calloc((size_t)stride * h, 1);
        if (!bitmap) {
            fputs("ERROR: Out of memory\n", stderr);
            ret = 1;
            break;
        }
        if (read_page(ras, &hdr, bitmap, stride, xoff, yoff) < 0) {
            free(bitmap);
            if (!canceled) {
                fputs("ERROR: Unable to read raster data\n", stderr);
                ret = 1;
            }
            break;
        }

        p.xres = xres;
        p.yres = yres;
        p.paper_type = qpdl_paper_type(hdr.cupsPageSizeName);
        if (p.paper_type < 0)
            p.paper_type = qpdl_paper_type_for_size(hdr.PageSize[0],
                                                    hdr.PageSize[1]);
        if (p.paper_type < 0) {
            fprintf(stderr, "WARNING: Unknown paper size %ux%u pt, "
                    "using A4\n", hdr.PageSize[0], hdr.PageSize[1]);
            p.paper_type = 2;
        }
        p.paper_source = hdr.MediaPosition == 2 ? 2 : 1;
        p.page_w_pt = hdr.PageSize[0];
        p.page_h_pt = hdr.PageSize[1];
        p.copies = 1;               /* copies are generated by CUPS */

        if (qpdl_page(stdout, &p, bitmap, w, h, stride) < 0) {
            fprintf(stderr, "ERROR: Unable to encode page %d\n", page);
            ret = 1;
        }
        free(bitmap);
        fprintf(stderr, "PAGE: %d 1\n", page);
    }

    if (started)
        qpdl_job_end(stdout);
    cupsRasterClose(ras);
    if (fd)
        close(fd);
    return ret;
}
