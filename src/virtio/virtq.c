/*
 * Copyright (c) 2026 Abe Kohandel
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "logging.h"
#include "virtq.h"

#include "fifo.h"

/**
 * Marks the descriptor as continuing via the next field.
 */
#define VIRTQ_DESC_F_NEXT (1U << 0)

/**
 * Marks the descriptor as write-only (otherwise read-only).
 */
#define VIRTQ_DESC_F_WRITE (1U << 1)

struct virtq_desc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
};

static inline size_t virtq_desc_mem_size(size_t size) {
  return size * sizeof(struct virtq_desc);
}

struct virtq_avail {
  uint16_t flags;
  uint16_t idx;
  uint16_t ring[];
};

static inline size_t virtq_avail_mem_size(uint16_t num) {
  const size_t ring_size = num * sizeof(uint16_t);
  const size_t used_event_size = sizeof(uint16_t);

  return sizeof(struct virtq_avail) + ring_size + used_event_size;
}

struct virtq_used_elem {
  uint32_t id;
  uint32_t len;
};

struct virtq_used {
  uint16_t flags;
  uint16_t idx;
  struct virtq_used_elem ring[];
};

static inline size_t virtq_used_mem_size(uint16_t num) {
  const size_t avail_event_size = sizeof(uint16_t);

  return sizeof(struct virtq_used) + num * sizeof(struct virtq_used_elem) +
         avail_event_size;
}

#define ALIGN(size, alignment) (((size) + (alignment - 1)) & ~(alignment - 1))
static inline size_t virtq_legacy_mem_size(uint16_t num, uint16_t alignment) {

  return ALIGN(virtq_desc_mem_size(num) + virtq_avail_mem_size(num),
               alignment) +
         ALIGN(virtq_avail_mem_size(num), alignment);
}

struct desc_extras {
  uint64_t vaddr;
  void *context;
};

struct virtq {
  uint16_t num;
  bool legacy;
  pthread_spinlock_t lock;

  struct virtq_desc *desc;
  intptr_t desc_paddr;

  struct virtq_avail *avail;
  intptr_t avail_paddr;

  struct virtq_used *used;
  intptr_t used_paddr;
  uint16_t last_seen_used;

  struct fifo *free_descs;
  struct desc_extras *desc_extras;
};

int virtq_desc_paddr(const struct virtq *const vq, intptr_t *const paddr) {
  if (vq == NULL || paddr == NULL) {
    return EINVAL;
  }

  *paddr = vq->desc_paddr;

  return EOK;
}

int virtq_avail_paddr(const struct virtq *const vq, intptr_t *const paddr) {
  if (vq == NULL || paddr == NULL) {
    return EINVAL;
  }

  *paddr = vq->avail_paddr;

  return EOK;
}

int virtq_used_paddr(const struct virtq *const vq, intptr_t *const paddr) {
  if (vq == NULL || paddr == NULL) {
    return EINVAL;
  }

  *paddr = vq->used_paddr;

  return EOK;
}

int virtq_size(const struct virtq *const vq, uint16_t *const num) {
  if (vq == NULL || num == NULL) {
    return EINVAL;
  }

  *num = vq->num;

  return EOK;
}

static int virtq_create_legacy(struct virtq *vq, uint16_t num) {
  const size_t pagesize = getpagesize();
  size_t avail_offset;
  size_t used_offset;
  int rc;

  vq->legacy = true;
  vq->desc = mmap64(0, virtq_legacy_mem_size(num, pagesize),
                    PROT_READ | PROT_WRITE | PROT_NOCACHE,
                    MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);

  if (vq->desc == MAP_FAILED) {
    rc = errno;
    log_err("failed to map legacy virtqueue memory: %s", strerror(rc));
    return rc;
  }

  avail_offset = virtq_desc_mem_size(num);
  vq->avail = (void *)((uintptr_t)vq->desc + avail_offset);

  used_offset = ALIGN(avail_offset + virtq_avail_mem_size(num), pagesize);
  vq->used = (void *)((uintptr_t)vq->desc + used_offset);

  return 0;
}

static int virtq_destroy_legacy(struct virtq *vq) {
  int rc = munmap(vq->desc, virtq_legacy_mem_size(vq->num, getpagesize()));
  if (rc != EOK) {
    return errno;
  }

  return EOK;
}

static int virtq_create_modern(struct virtq *vq, uint16_t num) {
  int rc;
  vq->legacy = false;
  vq->desc =
      mmap64(0, virtq_desc_mem_size(num), PROT_READ | PROT_WRITE | PROT_NOCACHE,
             MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);
  if (vq->desc == MAP_FAILED) {
    rc = errno;
    log_err("failed to map virtqueue descriptor table: %s", strerror(rc));
    return rc;
  }

  vq->avail = mmap64(0, virtq_avail_mem_size(num),
                     PROT_READ | PROT_WRITE | PROT_NOCACHE,
                     MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);
  if (vq->avail == MAP_FAILED) {
    rc = errno;
    log_err("failed to map virtqueue available ring: %s", strerror(rc));
    goto unmap_vq_desc;
  }

  vq->used =
      mmap64(0, virtq_used_mem_size(num), PROT_READ | PROT_WRITE | PROT_NOCACHE,
             MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);
  if (vq->used == MAP_FAILED) {
    rc = errno;
    log_err("failed to map virtqueue used ring: %s", strerror(rc));
    goto unmap_vq_avail;
  }

  return EOK;

unmap_vq_avail:
  munmap(vq->avail, virtq_avail_mem_size(num));
unmap_vq_desc:
  munmap(vq->desc, virtq_desc_mem_size(num));
  return ENOMEM;
}

static int virtq_destroy_modern(struct virtq *vq) {
  int rc;

  rc = munmap(vq->desc, virtq_desc_mem_size(vq->num));
  if (rc != EOK) {
    return errno;
  }

  rc = munmap(vq->avail, virtq_avail_mem_size(vq->num));
  if (rc != EOK) {
    return errno;
  }

  rc = munmap(vq->used, virtq_used_mem_size(vq->num));
  if (rc != EOK) {
    return errno;
  }

  return EOK;
}

int virtq_create(size_t size, bool legacy, struct virtq **vq) {
  struct virtq *pvq;
  int rc;

  if (vq == NULL) {
    return EINVAL;
  }

  log_debug("creating virtqueue: size=%zu, legacy=%d", size, legacy);

  pvq = mmap64(0, sizeof(struct virtq), PROT_READ | PROT_WRITE | PROT_NOCACHE,
               MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);
  if (pvq == MAP_FAILED) {
    rc = errno;
    log_err("failed to allocate virtqueue structure: %s", strerror(rc));
    return rc;
  }

  rc = legacy ? virtq_create_legacy(pvq, size) : virtq_create_modern(pvq, size);
  if (rc != EOK) {
    goto unmap_vq;
  }

  rc = mem_offset64(pvq->desc, NOFD, virtq_desc_mem_size(pvq->num),
                    &pvq->desc_paddr, 0);
  if (rc != EOK) {
    log_err("failed to get descriptor table physical address: %s",
            strerror(rc));
    goto destroy;
  }

  rc = mem_offset64(pvq->avail, NOFD, virtq_avail_mem_size(pvq->num),
                    &pvq->avail_paddr, 0);
  if (rc != EOK) {
    log_err("failed to get available ring physical address: %s", strerror(rc));
    goto destroy;
  }

  rc = mem_offset64(pvq->used, NOFD, virtq_used_mem_size(pvq->num),
                    &pvq->used_paddr, 0);
  if (rc != EOK) {
    log_err("failed to get used ring physical address: %s", strerror(rc));
    goto destroy;
  }

  pvq->desc_extras = calloc(size, sizeof(struct desc_extras));
  if (pvq->desc_extras == NULL) {
    rc = errno;
    log_err("failed to allocate descriptor extras: %s", strerror(rc));
    goto destroy;
  }

  rc = fifo_create(size, sizeof(uint16_t), &pvq->free_descs);
  if (rc != EOK) {
    log_err("failed to create free descriptor fifo: %s", strerror(rc));
    goto extras;
  }

  for (uint16_t i = 0; i < size; i++) {
    if (fifo_put(pvq->free_descs, &i) != 0) {
      goto fifo;
    }
  }

  rc = pthread_spin_init(&pvq->lock, PTHREAD_PROCESS_PRIVATE);
  if (rc != EOK) {
    log_err("failed to initialize virtqueue lock: %s", strerror(rc));
    goto fifo;
  }

  pvq->avail->flags = 0;
  pvq->avail->idx = 0;
  pvq->num = size;
  pvq->last_seen_used = 0;

  *vq = pvq;

  log_debug("virtqueue created successfully");
  return EOK;

fifo:
  fifo_destory(pvq->free_descs);

extras:
  free(pvq->desc_extras);

destroy:
  legacy ? virtq_destroy_legacy(pvq) : virtq_destroy_modern(pvq);

unmap_vq:
  munmap(pvq, sizeof(struct virtq));

  return rc;
}

int virtq_destroy(struct virtq *const vq) {
  int rc;

  if (vq == NULL) {
    return EINVAL;
  }

  rc = vq->legacy ? virtq_destroy_legacy(vq) : virtq_destroy_modern(vq);
  if (rc != EOK) {
    return rc;
  }

  free(vq->desc_extras);

  rc = fifo_destory(vq->free_descs);
  if (rc != EOK) {
    return rc;
  }

  rc = pthread_spin_destroy(&vq->lock);
  if (rc != EOK) {
    return rc;
  }

  rc = munmap(vq, sizeof(struct virtq));
  if (rc != EOK) {
    return errno;
  }

  return EOK;
}

int virtq_alloc_desc(struct virtq *const vq, uint32_t len, bool writable,
                     uint16_t *idx) {
  int rc;
  void *vaddr;
  intptr_t paddr;

  if (vq == NULL || idx == NULL) {
    return EINVAL;
  }

  rc = fifo_get(vq->free_descs, idx);
  if (rc != EOK) {
    return rc;
  }

  vaddr = mmap64(0, len, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                 MAP_SHARED | MAP_PHYS | MAP_ANON, NOFD, 0);
  if (vaddr == MAP_FAILED) {
    rc = errno;
    goto free_desc;
  }

  rc = mem_offset64(vaddr, NOFD, len, &paddr, 0);
  if (rc != EOK) {
    rc = errno;
    goto unmap_desc;
  }

  vq->desc_extras[*idx].vaddr = (uint64_t)vaddr;
  vq->desc_extras[*idx].context = NULL;

  vq->desc[*idx].addr = paddr;
  vq->desc[*idx].len = len;
  vq->desc[*idx].flags = writable ? VIRTQ_DESC_F_WRITE : 0;
  vq->desc[*idx].next = 0;

  return EOK;

unmap_desc:
  munmap((void *)vaddr, len);
free_desc:
  fifo_put(vq->free_descs, idx);
  return rc;
}

int virtq_free_desc(struct virtq *const vq, uint16_t idx) {
  int rc;

  rc = munmap((void *)vq->desc_extras[idx].vaddr, vq->desc[idx].len);
  if (rc != EOK) {
    return rc;
  }

  vq->desc_extras[idx].vaddr = 0;
  vq->desc_extras[idx].context = NULL;

  vq->desc[idx].addr = 0;
  vq->desc[idx].len = 0;
  vq->desc[idx].flags = 0;
  vq->desc[idx].next = 0;

  return fifo_put(vq->free_descs, &idx);
}

int virtq_get_desc_vaddr(const struct virtq *const vq, uint16_t idx,
                         uint64_t *vaddr) {
  if (vq == NULL || vaddr == NULL) {
    return EINVAL;
  }

  if (idx >= vq->num) {
    return EINVAL;
  }

  *vaddr = vq->desc_extras[idx].vaddr;

  return EOK;
}

int virtq_get_desc_len(const struct virtq *const vq, uint16_t idx,
                       uint32_t *len) {
  if (vq == NULL || len == NULL) {
    return EINVAL;
  }

  if (idx >= vq->num) {
    return EINVAL;
  }

  *len = vq->desc[idx].len;

  return EOK;
}

int virtq_get_desc_flags(const struct virtq *const vq, uint16_t idx,
                         uint16_t *flags) {
  if (vq == NULL || flags == NULL) {
    return EINVAL;
  }

  if (idx >= vq->num) {
    return EINVAL;
  }

  *flags = vq->desc[idx].flags;

  return EOK;
}

int virtq_get_desc_context(const struct virtq *const vq, uint16_t idx,
                           void **context) {
  if (vq == NULL || context == NULL) {
    return EINVAL;
  }

  if (idx >= vq->num) {
    return EINVAL;
  }

  *context = vq->desc_extras[idx].context;

  return EOK;
}

int virtq_put_desc_chain(struct virtq *const vq, size_t count, uint16_t *idx) {
  int rc;

  for (size_t n = 0; n < count; n++) {
    if (n > 0) {
      vq->desc[idx[n - 1]].next |= idx[n];
      vq->desc[idx[n - 1]].flags |= VIRTQ_DESC_F_NEXT;
    }
  }
  vq->desc[idx[count - 1]].next = 0;

  rc = pthread_spin_lock(&vq->lock);
  if (rc != EOK) {
    return rc;
  }

  vq->avail->ring[vq->avail->idx % vq->num] = idx[0];
  vq->avail->idx++;

  return pthread_spin_unlock(&vq->lock);
}

int virtq_set_desc_context(struct virtq *const vq, uint16_t idx,
                           void *context) {
  if (vq == NULL || context == NULL) {
    return EINVAL;
  }

  if (idx >= vq->num) {
    return EINVAL;
  }

  vq->desc_extras[idx].context = context;

  return EOK;
}

int virtq_get_desc_chain(struct virtq *const vq, size_t *count, uint16_t *idx) {
  uint16_t desc_idx;
  uint16_t desc_count;
  uint16_t flags;
  int rc;

  if (vq == NULL || count == NULL || idx == NULL) {
    return EINVAL;
  }

  rc = pthread_spin_lock(&vq->lock);
  if (rc != EOK) {
    return rc;
  }

  if (vq->used->idx == vq->last_seen_used) {
    rc = ENOENT;
    goto unlock;
  }

  desc_idx = vq->used->ring[vq->last_seen_used % vq->num].id;
  for (desc_count = 0; desc_count < *count; desc_count++) {
    idx[desc_count] = desc_idx;

    flags = vq->desc[desc_idx].flags;
    if (flags & VIRTQ_DESC_F_NEXT) {
      desc_idx = vq->desc[desc_idx].next;
    } else {
      break;
    }
  }

  if (desc_count == *count) {
    rc = ENOMEM;
    goto unlock;
  }

  *count = desc_count + 1;
  vq->last_seen_used++;
  rc = EOK;

unlock:
  pthread_spin_unlock(&vq->lock);
  return rc;
}

void virtq_print(const struct virtq *const vq) {
  printf("virtq: num=%u, last_seen_used=%u\n", vq->num, vq->last_seen_used);

  for (size_t i = 0; i < vq->num; i++) {
    printf("desc[%zu]: addr=%" PRIx64 ", len=%u, flags=%u, next=%u\n", i,
           vq->desc[i].addr, vq->desc[i].len, vq->desc[i].flags,
           vq->desc[i].next);
  }

  printf("avail idx=%u\n", vq->avail->idx);
  for (size_t i = 0; i < vq->num; i++) {
    printf("avail ring[%zu]: desc_idx=%u\n", i, vq->avail->ring[i]);
  }

  printf("used idx=%u\n", vq->used->idx);
  for (size_t i = 0; i < vq->num; i++) {
    printf("used ring[%zu]: desc_idx=%u, len=%u\n", i, vq->used->ring[i].id,
           vq->used->ring[i].len);
  }
}
