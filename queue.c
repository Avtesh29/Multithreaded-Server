#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

struct queue {
    void **buf;
    int n, in, out;

    sem_t mutex;
    sem_t empty_sem;
    sem_t full_sem;
};
typedef struct queue queue_t;

queue_t *queue_new(int size) {
    if (size == 0) {
        return NULL;
    }

    queue_t *q = (queue_t *) malloc(sizeof(queue_t));

    q->buf = (void **) malloc(size * sizeof(void *));

    q->n = size;
    q->in = 0;
    q->out = 0;

    sem_init(&(q->mutex), 0, 1);
    sem_init(&(q->empty_sem), 0, 0);
    sem_init(&(q->full_sem), 0, size);

    return q;
}

void queue_delete(queue_t **q) {
    sem_destroy(&((*q)->mutex));
    sem_destroy(&((*q)->empty_sem));
    sem_destroy(&((*q)->full_sem));
    free(*(*q)->buf);
    free(*q);
    q = NULL;
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) {
        return false;
    }

    sem_wait(&(q->full_sem));
    sem_wait(&(q->mutex));

    (q->buf)[q->in] = elem;
    q->in = ((q->in) + 1) % (q->n);

    sem_post((&q->mutex));
    sem_post(&(q->empty_sem));

    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    }

    sem_wait(&(q->empty_sem));
    sem_wait(&(q->mutex));

    *elem = (q->buf)[q->out];
    (q->buf)[q->out] = NULL;
    q->out = ((q->out) + 1) % (q->n);

    sem_post(&(q->mutex));
    sem_post(&(q->full_sem));

    return true;
}
