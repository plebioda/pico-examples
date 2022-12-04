#ifndef UTIL_H
#define UTIL_H

#include "bsec_handler.h"

#define DATETIME_INIT_VALUE \
    {                       \
        .year = -1,         \
        .month = -1,        \
        .day = -1,          \
        .dotw = -1,         \
        .hour = -1,         \
        .min = -1,          \
        .sec = -1           \
    }

static inline int64_t ns_to_us(int64_t ns)
{
    return ns / 1000;
}

static inline int64_t us_to_ns(int64_t us)
{
    return us * 1000;
}

static inline uint32_t ns_to_ms_u32(int64_t ns)
{
    return (uint32_t)(ns / 1000000);
}

static inline int64_t sec_to_ns(int sec)
{
    return (int64_t)sec * 1000000000;
}


char *datetime_str_date(char *buf, size_t buf_size, const datetime_t *t);
char *datetime_str_time(char *buf, size_t buf_size, const datetime_t *t);
char *datetime_str(char *buf, size_t buf_size, const datetime_t *t);
void print_bsec_handler_state(struct bsec_handler_state *bsec_handler_state);

struct sha1
{
    uint8_t data[20];
};

char *sha1_to_str(char *buf, size_t buf_size, const struct sha1 *sha1);
int8_t sha1_get(struct sha1 *sha1, const uint8_t *data, uint32_t len);

#endif // UTIL_H