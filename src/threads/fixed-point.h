#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include<stdint.h>
#define f (1<<14)


static inline int convert_to_fixed(int n)
{
    return n*f;
}

static inline int convert_to_int_truncate(int x)
{
    return x/f;
}

static inline int convert_to_int_round(int x)
{
    return (x>=0) ? ((x+f/2)/f) : ((x-f/2)/f);
}


static inline int add_both_fixed(int x,int y)
{
    return x+y;
}

static inline int subtract_both_fixed(int x,int y)
{
    return x-y;
}
static inline int add_int_fixed(int n,int x)
{
    return x+n*f;
}
static inline int subtract_int_from_fixed(int x,int n)
{
    return x-n*f;
}
static inline int mult_both_fixed(int x,int y)
{
    return ((int64_t)x)*y/f;
}

static inline int mult_int_fixed(int n,int x)
{
    return x*n;
}

static inline int divide_both_fixed(int x,int y)
{
    return ((int64_t)x)*f/y;
}

static inline int divide_fixed_int(int x,int n)
{
    return x/n;
}
#endif


