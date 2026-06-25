#include "http2.h"
#include <stdint.h>
#include <stdlib.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    http2_ctx_t *ctx = http2_create();
    if (!ctx) return 0;

    // Feed the fuzzer input
    http2_feed_input(ctx, data, size);

    // Try to get output
    uint8_t out[4096];
    http2_get_output(ctx, out, sizeof(out));

    // Try to read any DATA
    uint8_t buf[4096];
    uint32_t sid;
    http2_get_data(ctx, buf, sizeof(buf), &sid);

    http2_destroy(ctx);
    return 0;
}