/*
 * qpdl.c - Encoder for Samsung QPDL v5 / JBIG (algorithm 0x15) printers.
 *
 * Stream layout (one job):
 *
 *   PJL header ... "@PJL ENTER LANGUAGE = QPDL"
 *   for each page:
 *     page header   17 bytes
 *     aux records   16 bytes + 20-byte JBIG BIH + 4 bytes  (if any band)
 *     for each non-empty 128-line band:
 *       band header 12 bytes, JBIG data (without BIH), 4-byte checksum
 *     page footer   3 bytes
 *   PJL footer
 *
 * All multi-byte values are big-endian.
 */
#include "qpdl.h"

#include "jbig85.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define BAND_HEIGHT   128           /* QPDL BandSize */
#define MARGIN_PT     12            /* hardware margins (HWMargins) */
#define QPDL_VERSION  5
#define MAX_PACKET    (512 * 1024)  /* QPDL PacketSize, in bytes */
#define COLOR_BLACK   4
#define ALGO_JBIG     0x15

typedef struct {
    unsigned char *data;
    size_t         len, cap;
    int            error;
} jbig_buf_t;

typedef struct {
    int            nr;
    unsigned char *data;            /* JBIG data without the 20-byte BIH */
    size_t         len;
} band_t;

static const struct { const char *name; int code; } paper_codes[] = {
    {"Letter", 0}, {"Legal", 1}, {"A4", 2}, {"Executive", 3}, {"Ledger", 4},
    {"A3", 5}, {"Env10", 6}, {"Monarch", 7}, {"C5", 8}, {"DL", 9},
    {"EnvMonarch", 7}, {"EnvC5", 8}, {"EnvDL", 9},
    {"B4", 10}, {"B5", 11}, {"EnvISOB5", 12}, {"Postcard", 14},
    {"DoublePostcardRotated", 15}, {"A5", 16}, {"A6", 17}, {"B6", 18},
    {"C6", 19}, {"Folio", 20}, {"EnvPersonal", 21}, {"Env9", 22},
    {"Oficio", 23},
};

static const struct { double w, h; int code; } paper_sizes[] = {
    {612, 792, 0}, {612, 1008, 1}, {595, 842, 2}, {522, 756, 3},
    {297, 684, 6}, {279, 540, 7}, {459, 649, 8}, {312, 624, 9},
    {516, 729, 11}, {420, 595, 16}, {297, 420, 17},
};

int qpdl_paper_type(const char *name)
{
    for (size_t i = 0; i < sizeof(paper_codes) / sizeof(paper_codes[0]); i++)
        if (!strcasecmp(name, paper_codes[i].name))
            return paper_codes[i].code;
    return -1;
}

int qpdl_paper_type_for_size(double w_pt, double h_pt)
{
    for (size_t i = 0; i < sizeof(paper_sizes) / sizeof(paper_sizes[0]); i++)
        if (fabs(w_pt - paper_sizes[i].w) < 3 && fabs(h_pt - paper_sizes[i].h) < 3)
            return paper_sizes[i].code;
    return -1;
}

static void put16(unsigned char *p, unsigned v)
{
    p[0] = v >> 8;
    p[1] = v;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static void jbig_out(unsigned char *start, size_t len, void *arg)
{
    jbig_buf_t *b = arg;

    if (b->error)
        return;
    if (b->len + len > b->cap) {
        b->error = 1;
        return;
    }
    memcpy(b->data + b->len, start, len);
    b->len += len;
}

/* PJL strings must not contain quotes or control characters. */
static void pjl_clean(char *dst, size_t size, const char *src)
{
    size_t n = 0;

    for (; src && *src && n + 1 < size; src++)
        dst[n++] = (*src == '"' || (unsigned char)*src < 0x20) ? '_' : *src;
    dst[n] = 0;
}

void qpdl_job_begin(FILE *out, const qpdl_job_t *job)
{
    char u[128], t[128];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int density = job->density >= 1 && job->density <= 5 ? job->density : 3;

    pjl_clean(u, sizeof(u), job->user ? job->user : "user");
    pjl_clean(t, sizeof(t), job->title ? job->title : "document");

    fputs("\033%-12345X", out);
    fprintf(out, "@PJL COMMENT USERNAME=\"Username: %s\"\n", u);
    fprintf(out, "@PJL COMMENT DOCNAME=\"%s\"\n", t);
    fprintf(out, "@PJL JOB NAME=\"%s\"\n", t);
    fprintf(out, "@PJL DEFAULT SERVICEDATE=%04d%02d%02d\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    fprintf(out, "@PJL SET USERNAME=\"%s\"\n", u);
    fprintf(out, "@PJL SET JOBNAME=\"%s\"\n", t);
    fputs("@PJL SET MULTIBINMODE=PRINTERDEFAULT\n", out);
    if (job->econo_mode)
        fputs("@PJL SET ECONOMODE=ON\n", out);
    fputs("@PJL SET JAMRECOVERY=OFF\n", out);
    fputs("@PJL SET COLORMODE=MONO\n", out);
    fprintf(out, "@PJL SET RESOLUTION=%d\n", job->res);
    fputs("@PJL SET IMAGEQUALITY=0\n", out);
    fputs("@PJL SET RGBCOLOR=STANDARD\n", out);
    fputs("@PJL SET DUPLEX=OFF\n", out);
    fputs("@PJL SET PAPERTYPE=OFF\n", out);
    fputs("@PJL SET ALTITUDE=LOW\n", out);
    fprintf(out, "@PJL SET DENSITY=%d\n", density);
    fputs("@PJL SET RET=NORMAL\n", out);
    fputs("@PJL SET BANNERSHEET=OFF\n", out);
    fputs("@PJL SET TIMESTAMP=OFF\n", out);
    fputs("@PJL ENTER LANGUAGE = QPDL\n", out);
}

void qpdl_job_end(FILE *out)
{
    fputs("\t\033%-12345X", out);
    fflush(out);
}

static int band_is_empty(const unsigned char *p, size_t n)
{
    while (n--)
        if (*p++)
            return 0;
    return 1;
}

int qpdl_page(FILE *out, const qpdl_page_t *p, const unsigned char *bitmap,
              unsigned width, unsigned height, unsigned stride)
{
    unsigned margin_x_b = ((unsigned)ceil(MARGIN_PT * p->xres / 72.0) + 7) / 8;
    unsigned margin_y = (unsigned)ceil(MARGIN_PT * p->yres / 72.0);
    unsigned line_b = (width + 7) / 8;
    unsigned buf_w, buf_b, copy_b, rows_left, nbands, nused = 0;
    unsigned char *buf, bih[20], h[17];
    band_t *bands;
    jbig_buf_t jb = {0};
    int have_bih = 0, ret = -1;

    if (height <= margin_y || line_b <= 2 * margin_x_b)
        return -1;

    /* Band width: page width rounded to the nearest multiple of 256 dots. */
    buf_w = width & ~255u;
    if (buf_w + 128 < width)
        buf_w += 256;
    buf_b = buf_w / 8;
    copy_b = line_b - 2 * margin_x_b;
    if (copy_b > buf_b)
        copy_b = buf_b;

    rows_left = height - margin_y;
    nbands = (rows_left + BAND_HEIGHT - 1) / BAND_HEIGHT;

    /* Two blank rows in front act as the JBIG "previous lines" of row 0. */
    buf = malloc((size_t)(BAND_HEIGHT + 2) * buf_b);
    bands = calloc(nbands, sizeof(*bands));
    jb.cap = MAX_PACKET + 20;
    jb.data = malloc(jb.cap);
    if (!buf || !bands || !jb.data)
        goto done;

    for (unsigned n = 0; n < nbands; n++) {
        unsigned rows = rows_left < BAND_HEIGHT ? rows_left : BAND_HEIGHT;
        unsigned char *rows0 = buf + 2 * buf_b;
        struct jbg85_enc_state st;

        memset(buf, 0, (size_t)(BAND_HEIGHT + 2) * buf_b);
        for (unsigned y = 0; y < rows; y++) {
            const unsigned char *src = bitmap
                + (size_t)(margin_y + n * BAND_HEIGHT + y) * stride
                + margin_x_b;
            memcpy(rows0 + (size_t)y * buf_b, src, copy_b);
        }
        rows_left -= rows;

        if (band_is_empty(rows0, (size_t)BAND_HEIGHT * buf_b))
            continue;

        jb.len = 0;
        jb.error = 0;
        jbg85_enc_init(&st, buf_w, BAND_HEIGHT, jbig_out, &jb);
        jbg85_enc_options(&st, JBG_LRLTWO, BAND_HEIGHT, 0);
        for (unsigned y = 0; y < BAND_HEIGHT; y++)
            jbg85_enc_lineout(&st, buf + (size_t)(y + 2) * buf_b,
                              buf + (size_t)(y + 1) * buf_b,
                              buf + (size_t)y * buf_b);
        if (jb.error || jb.len < 20) {
            fprintf(stderr, "ERROR: JBIG band %u too large\n", n);
            goto done;
        }

        if (!have_bih) {
            memcpy(bih, jb.data, 20);
            have_bih = 1;
        }
        bands[nused].nr = n;
        bands[nused].len = jb.len - 20;
        bands[nused].data = malloc(bands[nused].len);
        if (!bands[nused].data)
            goto done;
        memcpy(bands[nused].data, jb.data + 20, bands[nused].len);
        nused++;
    }

    /* Page header */
    memset(h, 0, sizeof(h));
    h[0x1] = p->yres / 100;
    put16(h + 0x2, p->copies);
    h[0x4] = p->paper_type;
    put16(h + 0x5, (unsigned)ceil(300 * p->page_w_pt / 72.0));
    put16(h + 0x7, (unsigned)ceil(300 * p->page_h_pt / 72.0));
    h[0x9] = p->paper_source == 2 ? 2 : 1;  /* 1 auto, 2 manual feeder */
    h[0xe] = QPDL_VERSION;
    h[0xf] = 1;                     /* DocHeaderValues <0><0><1> */
    h[0x10] = p->xres / 100;
    fwrite(h, 1, 17, out);

    /* Auxiliary records: record 0x13, then the JBIG BIH */
    if (nused) {
        static const unsigned char aux[16] = {
            0x13, 0, 0, 0, 0x23, 0x15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x14
        };
        unsigned char tail[4] = { 0, 0, 1, (unsigned char)((buf_w >> 8) + 65) };

        fwrite(aux, 1, 16, out);
        fwrite(bih, 1, 20, out);
        fwrite(tail, 1, 4, out);
    }

    /* Bands */
    for (unsigned i = 0; i < nused; i++) {
        unsigned long sum = 0;

        for (size_t k = 0; k < bands[i].len; k++)
            sum += bands[i].data[k];

        h[0x0] = 0x0C;
        h[0x1] = bands[i].nr;
        put16(h + 0x2, buf_b);
        put16(h + 0x4, BAND_HEIGHT);
        h[0x6] = COLOR_BLACK;
        h[0x7] = ALGO_JBIG;
        put32(h + 0x8, bands[i].len + 4);
        fwrite(h, 1, 12, out);
        fwrite(bands[i].data, 1, bands[i].len, out);
        put32(h, sum);
        fwrite(h, 1, 4, out);
    }

    /* Page footer */
    h[0] = 1;
    put16(h + 1, p->copies);
    fwrite(h, 1, 3, out);
    ret = ferror(out) ? -1 : 0;

done:
    if (bands)
        for (unsigned i = 0; i < nused; i++)
            free(bands[i].data);
    free(bands);
    free(buf);
    free(jb.data);
    return ret;
}
