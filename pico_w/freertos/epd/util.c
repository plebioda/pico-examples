#include "util.h"
#include "stdio.h"
#include "netif/ppp/polarssl/sha1.h"

static const char *DATETIME_MONTHS[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"};

static const char *DATETIME_DOWS[7] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
};

char *datetime_str_date(char *buf, size_t buf_size, const datetime_t *t)
{
    snprintf(
        buf,
        buf_size,
        "%.3s, %d %.3s %d",
        DATETIME_DOWS[t->dotw],
        t->day,
        DATETIME_MONTHS[t->month - 1],
        t->year);

    return buf;
}

char *datetime_str_time(char *buf, size_t buf_size, const datetime_t *t)
{
    snprintf(
        buf,
        buf_size,
        "%2d:%02d:%02d",
        t->hour,
        t->min,
        t->sec);

    return buf;
}

char *datetime_str(char *buf, size_t buf_size, const datetime_t *t)
{
    snprintf(buf,
             buf_size,
             "%.3s %d %.3s %2d:%02d:%02d %d",
             DATETIME_DOWS[t->dotw],
             t->day,
             DATETIME_MONTHS[t->month - 1],
             t->hour,
             t->min,
             t->sec,
             t->year);
    return buf;
}

void print_bsec_handler_state(struct bsec_handler_state *bsec_handler_state)
{
    static const int max_hex_num = 16;
    printf("struct bsec_handler_state default_bsec_handler_state = {\n");
    printf("    .serialized_state = {\n");
    for (uint32_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
        uint8_t value = i < bsec_handler_state->serialized_state_size ? bsec_handler_state->serialized_state[i] : 0;

        if (!(i % max_hex_num))
        {
            printf("        ");
        }

        printf("0x%02x", value);
        if (i == bsec_handler_state->serialized_state_size - 1)
        {
            printf("\n");
        }
        else if ((i % max_hex_num) == (max_hex_num - 1))
        {
            printf(",\n");
        }
        else
        {
            printf(", ");
        }
    }

    printf("    },\n");
    printf("    .serialized_state_size = %lu\n", bsec_handler_state->serialized_state_size);
    printf("};\n");
}

char *sha1_to_str(char *buf, size_t buf_size, const struct sha1 *sha1)
{
    for (size_t i = 0; i < sizeof(sha1->data) && (2 * i) < buf_size; i++)
    {
        snprintf(&buf[2*i], buf_size - (2*i), "%02X", sha1->data[i]);
    }

    return buf;
}

int8_t sha1_get(struct sha1 *sha1, const uint8_t *data, uint32_t len)
{
    if (len > INT_MAX)
    {
        printf("get_sha1: invalid length = %x\n", len);
        return -1;
    }

    sha1_context sha1_ctx;

    sha1_starts(&sha1_ctx);
    sha1_update(&sha1_ctx, data, (int)len);
    sha1_finish(&sha1_ctx, sha1->data);

    return 0;
}