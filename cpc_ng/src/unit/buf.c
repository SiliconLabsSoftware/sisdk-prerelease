/***************************************************************************/ /**
 * @file
 * @brief Unit tests for sl_cpc_buf helpers.
 ******************************************************************************/

#include <stdint.h>

#include <unity_fixture.h>

#include "sl_cpc_buf.h"
#include "sl_cpc_msgq.h"
#include "sl_slist.h"
#include "unity.h"

TEST_GROUP(cpc_buf);

TEST_SETUP(cpc_buf)
{ /* empty */
}

TEST_TEAR_DOWN(cpc_buf)
{ /* empty */
}

/**
 * @brief sl_cpc_buf_chain() with a NULL tail or with @c head == @c tail does not
 *        change the head buffer.
 */
TEST(cpc_buf, chain_invalid_tail)
{
  sl_cpc_buf_t head;

  sl_cpc_buf_init(&head, NULL, 10);

  sl_cpc_buf_chain(&head, NULL);
  TEST_ASSERT_EQUAL_size_t(10, head.tot_len);
  TEST_ASSERT_EQUAL_size_t(10, head.len);
  TEST_ASSERT_NULL(head.node.node);

  sl_cpc_buf_chain(&head, &head);
  TEST_ASSERT_EQUAL_size_t(10, head.tot_len);
  TEST_ASSERT_EQUAL_size_t(10, head.len);
  TEST_ASSERT_NULL(head.node.node);
}

/**
 * @brief Chaining two buffers updates tot_len on the head and links tail via next.
 */
TEST(cpc_buf, chain_two_buffers)
{
  sl_cpc_buf_t head;
  sl_cpc_buf_t tail;

  sl_cpc_buf_init(&head, NULL, 10);
  sl_cpc_buf_init(&tail, NULL, 6);

  sl_cpc_buf_chain(&head, &tail);

  TEST_ASSERT_EQUAL_size_t(16, head.tot_len);
  TEST_ASSERT_EQUAL_size_t(10, head.len);

  TEST_ASSERT_EQUAL_size_t(6, tail.tot_len);
  TEST_ASSERT_EQUAL_size_t(6, tail.len);

  TEST_ASSERT_EQUAL_PTR(&tail.node, head.node.node);
  TEST_ASSERT_NULL(tail.node.node);
}

/**
 * @brief Repeated sl_cpc_buf_chain() on the same head builds a three-segment chain
 *        with cumulative tot_len on each segment.
 */
TEST(cpc_buf, chain_three_buffers)
{
  sl_cpc_buf_t head;
  sl_cpc_buf_t middle;
  sl_cpc_buf_t tail;

  sl_cpc_buf_init(&head, NULL, 10);
  sl_cpc_buf_init(&middle, NULL, 5);
  sl_cpc_buf_init(&tail, NULL, 3);

  sl_cpc_buf_chain(&head, &middle);
  sl_cpc_buf_chain(&head, &tail);

  TEST_ASSERT_EQUAL_size_t(18, head.tot_len);
  TEST_ASSERT_EQUAL_size_t(10, head.len);

  TEST_ASSERT_EQUAL_size_t(8, middle.tot_len);
  TEST_ASSERT_EQUAL_size_t(5, middle.len);

  TEST_ASSERT_EQUAL_size_t(3, tail.tot_len);
  TEST_ASSERT_EQUAL_size_t(3, tail.len);

  TEST_ASSERT_EQUAL_PTR(&middle.node, head.node.node);
  TEST_ASSERT_EQUAL_PTR(&tail.node, middle.node.node);
  TEST_ASSERT_NULL(tail.node.node);
}

/**
 * @brief sl_cpc_msgq_push() with a NULL buffer does not enqueue an entry.
 */
TEST(cpc_buf, msgq_push_null_buff_is_noop)
{
  sl_cpc_msgq_t msgq;

  sl_cpc_msgq_init(&msgq);
  sl_cpc_msgq_push(&msgq, NULL);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));
}

/**
 * @brief sl_cpc_msgq_pop() on an empty queue returns false and leaves len at zero.
 */
TEST(cpc_buf, msgq_pop_empty_returns_false)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));
  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));
}

/**
 * @brief One buffer pushed then popped; empty and len reflect queue state.
 */
TEST(cpc_buf, msgq_push_pop_single_buffer)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t buf;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  sl_cpc_buf_init(&buf, NULL, 7);
  sl_cpc_msgq_push(&msgq, &buf);
  TEST_ASSERT_FALSE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&buf, out);
  TEST_ASSERT_NULL(out->node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));
}

/**
 * @brief Three single buffers are popped in FIFO order with len decremented each time.
 */
TEST(cpc_buf, msgq_push_pop_fifo_order)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t first;
  sl_cpc_buf_t second;
  sl_cpc_buf_t third;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  sl_cpc_buf_init(&first, NULL, 1);
  sl_cpc_buf_init(&second, NULL, 2);
  sl_cpc_buf_init(&third, NULL, 3);

  sl_cpc_msgq_push(&msgq, &first);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));
  sl_cpc_msgq_push(&msgq, &second);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));
  sl_cpc_msgq_push(&msgq, &third);
  TEST_ASSERT_FALSE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(3, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&first, out);
  TEST_ASSERT_NULL(first.node.node);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&second, out);
  TEST_ASSERT_NULL(second.node.node);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&third, out);
  TEST_ASSERT_NULL(third.node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
}

/**
 * @brief One buffer chain counts as a single queue entry (len == 1).
 */
TEST(cpc_buf, msgq_push_pop_chain)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t head;
  sl_cpc_buf_t tail;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);
  sl_cpc_buf_init(&head, NULL, 4);
  sl_cpc_buf_init(&tail, NULL, 2);
  sl_cpc_buf_chain(&head, &tail);

  sl_cpc_msgq_push(&msgq, &head);
  TEST_ASSERT_FALSE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&head, out);
  TEST_ASSERT_EQUAL_size_t(6, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&tail.node, head.node.node);
  TEST_ASSERT_NULL(tail.node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
}

/**
 * @brief Queue holds a buffer chain then a single buffer; both pop intact.
 */
TEST(cpc_buf, msgq_push_pop_chain_single)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t head;
  sl_cpc_buf_t tail;
  sl_cpc_buf_t single;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);
  sl_cpc_buf_init(&head, NULL, 4);
  sl_cpc_buf_init(&tail, NULL, 2);
  sl_cpc_buf_chain(&head, &tail);

  sl_cpc_msgq_push(&msgq, &head);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  sl_cpc_buf_init(&single, NULL, 42);
  sl_cpc_msgq_push(&msgq, &single);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&head, out);
  TEST_ASSERT_EQUAL_size_t(6, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&tail.node, head.node.node);
  TEST_ASSERT_NULL(tail.node.node);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&single, out);
  TEST_ASSERT_EQUAL_size_t(42, out->tot_len);
  TEST_ASSERT_NULL(single.node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
}

/**
 * @brief Two buffer chains are popped in push order without merging chains.
 */
TEST(cpc_buf, msgq_push_pop_two_chains)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t c1_head;
  sl_cpc_buf_t c1_tail;
  sl_cpc_buf_t c2_head;
  sl_cpc_buf_t c2_tail;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&c1_head, NULL, 10);
  sl_cpc_buf_init(&c1_tail, NULL, 20);
  sl_cpc_buf_chain(&c1_head, &c1_tail);
  sl_cpc_msgq_push(&msgq, &c1_head);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  sl_cpc_buf_init(&c2_head, NULL, 15);
  sl_cpc_buf_init(&c2_tail, NULL, 25);
  sl_cpc_buf_chain(&c2_head, &c2_tail);
  sl_cpc_msgq_push(&msgq, &c2_head);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&c1_head, out);
  TEST_ASSERT_EQUAL_size_t(30, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&c1_tail.node, c1_head.node.node);
  TEST_ASSERT_NULL(c1_tail.node.node);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&c2_head, out);
  TEST_ASSERT_EQUAL_size_t(40, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&c2_tail.node, c2_head.node.node);
  TEST_ASSERT_NULL(c2_tail.node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
}

/**
 * @brief Mixed queue of chain, single buffer, and chain preserves FIFO and len.
 */
TEST(cpc_buf, msgq_push_pop_chain_single_chain)
{
  sl_cpc_msgq_t msgq;
  sl_cpc_buf_t c1_head;
  sl_cpc_buf_t c1_tail;
  sl_cpc_buf_t single_buf;
  sl_cpc_buf_t c2_head;
  sl_cpc_buf_t c2_tail;
  sl_cpc_buf_t *out = NULL;

  sl_cpc_msgq_init(&msgq);

  sl_cpc_buf_init(&c1_head, NULL, 10);
  sl_cpc_buf_init(&c1_tail, NULL, 20);
  sl_cpc_buf_chain(&c1_head, &c1_tail);
  sl_cpc_msgq_push(&msgq, &c1_head);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  sl_cpc_buf_init(&single_buf, NULL, 42);
  sl_cpc_msgq_push(&msgq, &single_buf);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));

  sl_cpc_buf_init(&c2_head, NULL, 15);
  sl_cpc_buf_init(&c2_tail, NULL, 25);
  sl_cpc_buf_chain(&c2_head, &c2_tail);
  sl_cpc_msgq_push(&msgq, &c2_head);
  TEST_ASSERT_FALSE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(3, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&c1_head, out);
  TEST_ASSERT_EQUAL_size_t(30, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&c1_tail.node, c1_head.node.node);
  TEST_ASSERT_NULL(c1_tail.node.node);
  TEST_ASSERT_EQUAL_size_t(2, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&single_buf, out);
  TEST_ASSERT_EQUAL_size_t(42, out->tot_len);
  TEST_ASSERT_NULL(single_buf.node.node);
  TEST_ASSERT_EQUAL_size_t(1, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_TRUE(sl_cpc_msgq_pop(&msgq, &out));
  TEST_ASSERT_EQUAL_PTR(&c2_head, out);
  TEST_ASSERT_EQUAL_size_t(40, out->tot_len);
  TEST_ASSERT_EQUAL_PTR(&c2_tail.node, c2_head.node.node);
  TEST_ASSERT_NULL(c2_tail.node.node);
  TEST_ASSERT_TRUE(sl_cpc_msgq_is_empty(&msgq));
  TEST_ASSERT_EQUAL_size_t(0, sl_cpc_msgq_len(&msgq));

  TEST_ASSERT_FALSE(sl_cpc_msgq_pop(&msgq, &out));
}

TEST_GROUP_RUNNER(cpc_buf)
{
  RUN_TEST_CASE(cpc_buf, chain_invalid_tail);
  RUN_TEST_CASE(cpc_buf, chain_two_buffers);
  RUN_TEST_CASE(cpc_buf, chain_three_buffers);
  RUN_TEST_CASE(cpc_buf, msgq_push_null_buff_is_noop);
  RUN_TEST_CASE(cpc_buf, msgq_pop_empty_returns_false);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_single_buffer);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_fifo_order);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_chain);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_chain_single);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_two_chains);
  RUN_TEST_CASE(cpc_buf, msgq_push_pop_chain_single_chain);
}
