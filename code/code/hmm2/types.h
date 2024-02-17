#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "ic.h"

#define RASTER_DTYPE uint8_t
#define COMPRESS_DTYPE uint8_t

#define TURBOP4_ENC p4nenc8 // p4nenc16 p4nenc128v16
#define TURBOP4_DEC p4ndec8 // p4ndec16 p4ndec128v16
#define P4NENC_BOUND(n) ((n+sizeof(RASTER_DTYPE))/sizeof(RASTER_DTYPE)+(n+32)*sizeof(RASTER_DTYPE))