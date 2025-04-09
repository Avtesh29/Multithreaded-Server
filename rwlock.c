#include "rwlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

struct rwlock {
    PRIORITY p;
    int n;

    pthread_mutex_t lock;
    pthread_cond_t readCV;
    pthread_cond_t writeCV;

    int activeR;
    int activeW;
    int waitingR;
    int waitingW;

    int totalR;
};

typedef struct rwlock rwlock_t;

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rwl = (rwlock_t *) malloc(sizeof(rwlock_t));
    rwl->p = p;
    rwl->n = n;

    rwl->activeR = 0;
    rwl->activeW = 0;
    rwl->waitingR = 0;
    rwl->waitingW = 0;
    rwl->totalR = 0;

    pthread_cond_init(&(rwl->writeCV), NULL);
    pthread_cond_init(&(rwl->readCV), NULL);
    pthread_mutex_init(&(rwl->lock), NULL);

    return rwl;
}

void rwlock_delete(rwlock_t **l) {
    pthread_cond_destroy(&((*l)->writeCV));
    pthread_cond_destroy(&((*l)->readCV));
    pthread_mutex_destroy(&((*l)->lock));
    free(*l);
    l = NULL;
}

void reader_lock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));

    rw->waitingR += 1;
    while (rw->activeW > 0) {
        pthread_cond_wait(&(rw->readCV), &(rw->lock));
    }
    if (rw->p == WRITERS) {
        while ((rw->waitingW > 0) || (rw->activeW > 0)) {
            pthread_cond_wait(&(rw->readCV), &(rw->lock));
        }
    } else if (rw->p == N_WAY) {
        while ((rw->totalR >= rw->n) || (rw->activeW > 0)) {
            pthread_cond_wait(&(rw->readCV), &(rw->lock));
            if ((rw->waitingW == 0) && (rw->waitingR > 0)) {
                break;
            }
        }
    }
    rw->waitingR -= 1;
    rw->activeR += 1;
    rw->totalR += 1;

    pthread_mutex_unlock(&(rw->lock));
}

void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->activeR -= 1;
    pthread_mutex_unlock(&(rw->lock));

    if (rw->p == WRITERS) {
        if (rw->activeW == 0 && rw->waitingW == 0) {
            pthread_cond_signal(&(rw->readCV));
        } else {
            pthread_cond_signal(&(rw->writeCV));
        }
    } else if (rw->p == READERS) {
        if (rw->waitingR > 0) {
            pthread_cond_broadcast(&(rw->readCV));
        } else if (rw->waitingR == 0 && rw->activeR == 0) {
            pthread_cond_signal(&(rw->writeCV));
        }
    } else if (rw->p == N_WAY) {
        if (rw->totalR < rw->n && rw->waitingR > 0) {
            pthread_cond_broadcast(&(rw->readCV));
        } else {
            if (rw->waitingW > 0 && rw->activeR == 0) {
                pthread_cond_signal(&(rw->writeCV));
            } else if (rw->waitingW == 0 && rw->waitingR > 0) {
                pthread_cond_broadcast(&(rw->readCV));
            }
        }
    }
}

void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));

    rw->waitingW += 1;

    if (rw->p == READERS) {
        while ((rw->activeR > 0) || (rw->waitingR > 0)) {
            pthread_cond_wait(&(rw->writeCV), &(rw->lock));
        }
    } else if (rw->p == N_WAY) {
        while (rw->totalR < rw->n || rw->activeR > 0) {
            pthread_cond_wait(&(rw->writeCV), &(rw->lock));
            if ((rw->activeR == 0) && (rw->waitingR == 0)) {
                break;
            }
        }
    }

    while ((rw->activeW > 0) || (rw->activeR > 0)) {
        pthread_cond_wait(&(rw->writeCV), &(rw->lock));
    }
    rw->waitingW -= 1;
    rw->activeW += 1;
    rw->totalR = 0;

    pthread_mutex_unlock(&(rw->lock));
}

void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&(rw->lock));
    rw->activeW -= 1;
    pthread_mutex_unlock(&(rw->lock));

    if (rw->p == WRITERS) {
        if (rw->waitingW > 0) {
            pthread_cond_signal(&(rw->writeCV));
        } else if (rw->waitingR > 0) {
            pthread_cond_broadcast(&(rw->readCV));
        }
    } else {
        if (rw->waitingR == 0) {
            pthread_cond_signal(&(rw->writeCV));
        } else {
            pthread_cond_broadcast(&(rw->readCV));
        }
    }
}
