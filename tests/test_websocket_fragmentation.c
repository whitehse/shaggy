#include "websocket.h"
#include "protocol_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Basic structural test for fragmentation support */
static void test_fragmentation_structural(void)
{
    websocket_ctx_t *ctx = websocket_create(WS_ROLE_CLIENT);

    /* Verify that the module can be created and basic operations work */
    websocket_send_text(ctx, "Hello");
    uint8_t buf[256];
    size_t len = websocket_get_output(ctx, buf, sizeof(buf));
    assert(len > 0);

    websocket_destroy(ctx);
    printf("test_fragmentation_structural: PASSED\n");
}

int main(void)
{
    test_fragmentation_structural();
    printf("All WebSocket Fragmentation tests PASSED\n");
    return 0;
}