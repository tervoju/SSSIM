#ifndef _client_print_h
#define _client_print_h

#include "vt100.h"

#include <stdio.h>

extern FILE * log_file;

#define  printf  dprint_printf
#define  putchar(c)         do { putchar(c); if (log_file != NULL) putc(c, log_file); } while(0)
#define  dprint(...)        do { dprint_timestamp(); dprint_printf(__VA_ARGS__); } while(0)
#define  dprint_fn(...)     do { dprint_timestamp(); dprint_printf("%s: ", __FUNCTION__); dprint_printf(__VA_ARGS__); } while(0)
#define  dprint_dim(...)    do { dprint_timestamp(); fprintf(stdout, VT_SET(VT_DIM)); dprint_printf(__VA_ARGS__); fprintf(stdout, VT_RESET); } while(0)
#define  dprint_error(...)  do { dprint_timestamp(); fprintf(stdout, VT_SET(VT_RED)); dprint_printf(__VA_ARGS__); fprintf(stdout, VT_RESET); } while(0)
#define  dprint_stdout(...) do { dprint_stdout_timestamp(); fprintf(stdout, VT_SET(VT_GREEN)); fprintf(stdout, __VA_ARGS__); fprintf(stdout, VT_RESET); } while(0)
#define  dprint_logonly(...)  do if (log_file != NULL) { putc('@', log_file); dprint_timestamp(true); fprintf(log_file, __VA_ARGS__); fflush(log_file);	} while(0)

uint32_t dprint_stdout_timestamp();
void dprint_timestamp(bool log_only = false);
int dprint_printf(const char *fmt, ...);

#endif // _client_print_h
