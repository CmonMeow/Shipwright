#include "global.h"

int64_t __ull_rshift(uint64_t l, int64_t r) {
    return l >> r;
}

uint64_t __ull_rem(uint64_t l, uint64_t r) {
    return l % r;
}

uint64_t __ull_div(uint64_t l, uint64_t r) {
    return l / r;
}

int64_t __ll_lshift(int64_t l, int64_t r) {
    return l << r;
}

int64_t __ll_rem(int64_t l, uint64_t r) {
    return l % r;
}

int64_t __ll_div(int64_t l, int64_t r) {
    return l / r;
}

int64_t __ll_mul(int64_t l, int64_t r) {
    return l * r;
}

void __ull_divremi(uint64_t* quotient, uint64_t* remainder, uint64_t dividend, uint16_t divisor) {
    *quotient = dividend / divisor;
    *remainder = dividend % divisor;
}

int64_t __ll_mod(int64_t l, int64_t r) {
    int64_t remainder = l % r;

    if (((remainder < 0) && (r > 0)) || ((remainder > 0) && (r < 0))) {
        remainder += r;
    }
    return remainder;
}

int64_t __ll_rshift(int64_t l, int64_t r) {
    return l >> r;
}
