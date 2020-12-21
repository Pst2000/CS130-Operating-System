/**********************NEW CODE**********************/
#ifndef THREAD_FIXED_POINT_H
#define THREAD_FIXED_POINT_H

#define P 17
#define Q 14
#define F (1 << Q)

#define CONVERT_TO_FP(n) (n * F)
#define CONVERT_TO_INT_ZERO(x) (x / F)
#define CONVERT_TO_INT_NEAREST(x) ((x>=0) ? ((x+F/2)/F) : ((x-F/2)/F))
#define ADD_FP(x, y) (x + y)
#define SUB_FP(x, y) (x - y)
#define ADD_INT(x, n) (x + (n*F))
#define SUB_INT(x, n) (x - (n*F))
#define MULT_FP(x, y) ((((int64_t)x) * y) / F)
#define MULT_INT(x, n) (x * n)
#define DIV_FP(x, y) ((((int64_t)x) * F) / y)
#define DIV_INT(x, n) (x / n)

#endif
/********************END NEW CODE********************/