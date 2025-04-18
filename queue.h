#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct queue queue_t;

/** @brief Dynamically allocates and initializes a new queue with a
 *         maximum size, size
 *
 *  @param size the maximum size of the queue
 *
 *  @return a pointer to a new queue_t
 */
queue_t *queue_new(int size);

/** @brief Delete your queue and free all of its memory.
 *
 *  @param q the queue to be deleted.
 */
void queue_delete(queue_t **q);

/** @brief push an element onto a queue
 *
 *  @param q the queue to push an element into.
 *
 *  @param elem th element to add to the queue
 *
 *  @return A bool indicating success or failure.
 */
bool queue_push(queue_t *q, void *elem);

/** @brief pop an element from a queue.
 *
 *  @param q the queue to pop an element from.
 *
 *  @param elem a place to assign the poped element.
 *
 *  @return A bool indicating success or failure.
 */
bool queue_pop(queue_t *q, void **elem);
