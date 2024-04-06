#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define SLAB_SIZE 4096  // 每个slab的大小
#define CACHE_SIZE 32   // 每个缓存的对象大小
#define ALLOC_START 0x87000000  // 分配器的起始虚拟地址
#define ALLOC_END 0x88000000    // 分配器的结束虚拟地址

struct slab {
  struct slab *next;  // 指向下一个slab
  char objects[SLAB_SIZE - sizeof(struct slab *)];  // slab中的对象
};

struct cache {
  struct spinlock lock;  // 缓存的锁
  int size;              // 缓存中对象的大小
  struct slab *slabs;    // 缓存中的slab列表
  void *free;            // 缓存中的空闲对象列表
};

static struct cache caches[NCPU][CACHE_SIZE];  // 每个CPU的缓存数组
static char *alloc_ptr = (char *)ALLOC_START;  // 当前分配的虚拟地址

void *hwy_kmalloc(int size) {
  if (size <= 0 || size > CACHE_SIZE)
    return 0;

  struct cache *c = &caches[cpuid()][size - 1];  // 根据对象大小选择对应的缓存

  acquire(&c->lock);

  if (c->free) {  // 如果缓存中有空闲对象,直接从空闲列表中分配
    void *ptr = c->free;
    c->free = *(void **)ptr;
    release(&c->lock);
    return ptr;
  }

  if (!c->slabs) {  // 如果缓存中没有slab,则创建一个新的slab
    if (alloc_ptr + SLAB_SIZE > (char *)ALLOC_END) {  // 检查是否超出分配器的地址范围
      release(&c->lock);
      return 0;
    }
    struct slab *s = (struct slab *)alloc_ptr;
    alloc_ptr += SLAB_SIZE;
    s->next = c->slabs;
    c->slabs = s;
  }

struct slab *s = c->slabs;
void *ptr = s->objects;
ptr = (char *)s->objects + c->size;

if ((char *)ptr + c->size > (char *)s + SLAB_SIZE) {  // 如果当前slab已满,移动到下一个slab
  c->slabs = s->next;
}


  release(&c->lock);
  return ptr;
}

void hwy_kfree(void *ptr) {
  if (!ptr)
    return;

  struct cache *c = &caches[cpuid()][*(int *)((char *)ptr - 4) - 1];  // 根据对象头部的大小信息找到对应的缓存

  acquire(&c->lock);

  *(void **)ptr = c->free;  // 将对象添加到缓存的空闲列表中
  c->free = ptr;

  release(&c->lock);
}
