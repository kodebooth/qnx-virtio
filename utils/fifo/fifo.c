/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include "fifo.h"

#include "pthread.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct fifo {
  pthread_spinlock_t lock;

  size_t head;
  size_t tail;
  size_t count;
  size_t capacity;
  size_t elem_size;
  uint8_t data[];
};

static inline void *fifo_elem_addr(struct fifo *const fifo,
                                   const size_t index) {
  return fifo->data + fifo->elem_size * index;
}

int fifo_create(const size_t capacity, const size_t elem_size,
                struct fifo **fifo) {
  int rc;
  struct fifo *pfifo;

  if (fifo == NULL) {
    return EINVAL;
  }

  pfifo = malloc(sizeof(struct fifo) + capacity * elem_size);
  if (pfifo == NULL) {
    return ENOMEM;
  }

  rc = pthread_spin_init(&pfifo->lock, PTHREAD_PROCESS_PRIVATE);
  if (rc != EOK) {
    free(pfifo);
    return rc;
  }

  pfifo->head = 0;
  pfifo->tail = 0;
  pfifo->count = 0;
  pfifo->capacity = capacity;
  pfifo->elem_size = elem_size;

  *fifo = pfifo;

  return EOK;
}

int fifo_destory(struct fifo *const fifo) {
  int rc;

  if (fifo == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_destroy(&fifo->lock);
  if (rc != EOK) {
    return rc;
  }

  free(fifo);
  return EOK;
}

int fifo_empty(const struct fifo *const fifo, bool *const empty) {
  if (fifo == NULL || empty == NULL) {
    return EINVAL;
  }

  *empty = fifo->count == 0 ? true : false;
  return EOK;
}

int fifo_full(const struct fifo *const fifo, bool *const full) {
  if (fifo == NULL || full == NULL) {
    return EINVAL;
  }

  *full = fifo->count == fifo->capacity ? true : false;
  return EOK;
}

int fifo_put(struct fifo *const fifo, const void *const elem) {
  int rc = EOK;
  bool full;

  rc = pthread_spin_lock(&fifo->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = fifo_full(fifo, &full);
  if (rc != EOK) {
    goto exit;
  }

  if (full) {
    rc = ENOMEM;
    goto exit;
  }

  memcpy(fifo_elem_addr(fifo, fifo->tail), elem, fifo->elem_size);
  fifo->tail = (fifo->tail + 1) % fifo->capacity;
  fifo->count++;

exit:
  pthread_spin_unlock(&fifo->lock);
  return rc;
}

int fifo_get(struct fifo *const fifo, void *const elem) {
  int rc = EOK;
  bool empty;

  rc = pthread_spin_lock(&fifo->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = fifo_empty(fifo, &empty);
  if (rc != EOK) {
    goto exit;
  }

  if (empty) {
    rc = ENOENT;
    goto exit;
  }

  memcpy(elem, fifo_elem_addr(fifo, fifo->head), fifo->elem_size);
  fifo->head = (fifo->head + 1) % fifo->capacity;
  fifo->count--;

exit:
  pthread_spin_unlock(&fifo->lock);
  return rc;
}
