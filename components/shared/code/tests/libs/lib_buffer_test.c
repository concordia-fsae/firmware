#include <stdint.h>
#include "lib_buffer.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void circular_buffer_peeks_wraps_and_clears(void)
{
    LIB_BUFFER_CIRC_CREATE(circ, uint8_t, 3U) = {
        .buffer = { 1U, 2U, 3U },
        .currentPos = 0U,
    };

    TEST_ASSERT_EQUAL_UINT8(1U, LIB_BUFFER_CIRC_PEEK(&circ));
    TEST_ASSERT_EQUAL_UINT8(3U, LIB_BUFFER_CIRC_PEEKN(&circ, -1));
    TEST_ASSERT_EQUAL_UINT8(2U, LIB_BUFFER_CIRC_PEEKN(&circ, 1));

    LIB_BUFFER_CIRC_GET(&circ);
    TEST_ASSERT_EQUAL_UINT(1U, circ.currentPos);
    LIB_BUFFER_CIRC_GETSET(&circ, 9U);
    TEST_ASSERT_EQUAL_UINT8(9U, circ.buffer[1]);
    TEST_ASSERT_EQUAL_UINT(2U, circ.currentPos);

    LIB_BUFFER_CIRC_CLEAR(&circ);
    TEST_ASSERT_EQUAL_UINT(0U, circ.currentPos);
    TEST_ASSERT_EQUAL_UINT8(0U, circ.buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, circ.buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0U, circ.buffer[2]);
}

static void fifo_buffer_tracks_length_peeks_and_wraps(void)
{
    LIB_BUFFER_FIFO_CREATE(fifo, uint8_t, 4U) = {
        .buffer = { 0U },
        .startPos = 0U,
        .endPos = 0U,
    };

    TEST_ASSERT_EQUAL_UINT(0U, LIB_BUFFER_FIFO_GETLENGTH(&fifo));
    TEST_ASSERT_EQUAL_UINT(3U, LIB_BUFFER_FIFO_GETMAXCONTINUOUS(&fifo));
    LIB_BUFFER_FIFO_INSERT(&fifo, 10U);
    LIB_BUFFER_FIFO_INSERT(&fifo, 20U);
    TEST_ASSERT_EQUAL_UINT(2U, LIB_BUFFER_FIFO_GETLENGTH(&fifo));
    TEST_ASSERT_EQUAL_UINT8(10U, LIB_BUFFER_FIFO_PEEK(&fifo));
    TEST_ASSERT_EQUAL_UINT8(20U, LIB_BUFFER_FIFO_PEEKN(&fifo, 1));

    LIB_BUFFER_FIFO_POP(&fifo);
    TEST_ASSERT_EQUAL_UINT(1U, fifo.startPos);
    TEST_ASSERT_EQUAL_UINT(1U, LIB_BUFFER_FIFO_GETLENGTH(&fifo));
    LIB_BUFFER_FIFO_RESERVE(&fifo, 2U);
    TEST_ASSERT_EQUAL_UINT(3U, LIB_BUFFER_FIFO_GETLENGTH(&fifo));

    LIB_BUFFER_FIFO_CLEAR(&fifo);
    TEST_ASSERT_EQUAL_UINT(0U, fifo.startPos);
    TEST_ASSERT_EQUAL_UINT(0U, fifo.endPos);
    TEST_ASSERT_EQUAL_UINT(0U, LIB_BUFFER_FIFO_GETLENGTH(&fifo));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(circular_buffer_peeks_wraps_and_clears);
    RUN_TEST(fifo_buffer_tracks_length_peeks_and_wraps);
    return UNITY_END();
}

