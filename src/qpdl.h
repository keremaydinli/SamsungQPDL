/*
 * qpdl.h - Encoder for Samsung QPDL v5 / JBIG (algorithm 0x15) printers,
 *          e.g. the Samsung ML-1670 / ML-1675.
 *
 * Protocol details were taken from the SpliX project (CLP-310 / 0x15 path).
 */
#ifndef QPDL_H
#define QPDL_H

#include <stdio.h>

typedef struct {
    const char *user;
    const char *title;
    int         res;            /* dots per inch, normally 600 */
    int         econo_mode;     /* toner save: 0 off, 1 on */
    int         density;        /* toner density 1 (light) .. 5 (dark) */
} qpdl_job_t;

typedef struct {
    int    xres, yres;          /* dots per inch, normally 600 x 600 */
    int    paper_type;          /* QPDL paper code: 0 Letter, 1 Legal, 2 A4 ... */
    int    paper_source;        /* 1 auto, 2 manual feeder */
    double page_w_pt;           /* physical page size in points (1/72 in) */
    double page_h_pt;
    int    copies;
} qpdl_page_t;

/* Paper code for a CUPS/PPD page size name, or -1 if unknown. */
int  qpdl_paper_type(const char *name);

/* Paper code for a page size in points, or -1 if unknown. */
int  qpdl_paper_type_for_size(double w_pt, double h_pt);

/* PJL job header; call once before the first page. */
void qpdl_job_begin(FILE *out, const qpdl_job_t *job);

/*
 * Encode and write one page. The bitmap is 1 bit per pixel, MSB first,
 * 1 = black, covering the whole physical page at the given resolution.
 * Returns 0 on success, -1 on error.
 */
int  qpdl_page(FILE *out, const qpdl_page_t *p, const unsigned char *bitmap,
               unsigned width, unsigned height, unsigned stride);

/* PJL job footer; call once after the last page. */
void qpdl_job_end(FILE *out);

#endif
