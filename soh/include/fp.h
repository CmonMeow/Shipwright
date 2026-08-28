#ifndef FP_H
#define FP_H
#include <libultraship/libultra.h>

extern float qNaN0x3FFFFF;
extern float qNaN0x10000;
extern float sNaN0x3FFFFF;

float floorf(float x);
double floor(double x);
int32_t lfloorf(float x);
int32_t lfloor(double x);

float ceilf(float x);
double ceil(double x);
int32_t lceilf(float x);
int32_t lceil(double x);

float truncf(float x);
double trunc(double x);
int32_t ltruncf(float x);
int32_t ltrunc(double x);

float nearbyintf(float x);
double nearbyint(double x);
int32_t lnearbyintf(float x);
int32_t lnearbyint(double x);

float roundf(float x);
double round(double x);

#endif
