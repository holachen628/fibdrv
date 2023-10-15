#ifndef BIGNUM_H_H
#define BIGNUM_H_H

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>

/* number[size - 1] = msb, number[0] = lsb */
typedef struct _bn {
    unsigned int *number;
    unsigned int size;
    int sign;
} bn;

int bn_cmp(const bn *a, const bn *b);
int bn_resize(bn *src, size_t size);
bn *bn_alloc(size_t size);
int bn_free(bn *src);
void bn_add(const bn *a, const bn *b, bn *c);
void bn_sub(const bn *a, const bn *b, bn *c);
void bn_mult(const bn *a, const bn *b, bn *c);
void bn_swap(bn *a, bn *b);
void bn_lshift(bn *src, size_t shift);
int bn_cpy(bn *dest, bn *src);
#endif /* BIGNUM_H_H */