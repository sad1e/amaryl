#include "amaryl/core/allocator/pool_allocator.h"

#include <mimalloc.h>

#include "amaryl/core/assert.h"
#include "amaryl/core/templates.h"

namespace amaryl {

struct PoolAllocator::Block {
  Block* next = nullptr;
  size_type slot_count = 0;
  // Followed by `slot_count` slots of `slot_size_` bytes each.
};

PoolAllocator::PoolAllocator(size_type slot_size, size_type slots_per_block)
    : slot_size_(AlignUp(slot_size, kDefaultAlignment)),
      slots_per_block_(slots_per_block) {
  ASSERT(slot_size_ >= sizeof(void*), "slot size must fit a free-list pointer");
}

PoolAllocator::~PoolAllocator() {
  Block* block = head_;
  while (block != nullptr) {
    Block* next = block->next;
    mi_free(block);
    block = next;
  }
}

void PoolAllocator::Grow() {
  const size_type data_offset =
      static_cast<size_type>(AlignUp(sizeof(Block), kDefaultAlignment));
  const size_type alloc_size = data_offset + slots_per_block_ * slot_size_;
  auto* block = static_cast<Block*>(mi_malloc_aligned(alloc_size, kDefaultAlignment));
  block->next = head_;
  block->slot_count = slots_per_block_;
  head_ = block;

  byte* slots = reinterpret_cast<byte*>(block) + data_offset;
  for (size_type i = 0; i < slots_per_block_; ++i) {
    void* slot = slots + i * slot_size_;
    *static_cast<void**>(slot) = free_list_;
    free_list_ = slot;
  }
}

void* PoolAllocator::Allocate(size_type size, size_type alignment) {
  ASSERT(size <= slot_size_, "requested size exceeds pool slot size");
  ASSERT(alignment <= kDefaultAlignment, "pool slots are aligned to kDefaultAlignment");

  if (free_list_ == nullptr) {
    Grow();
  }

  void* slot = free_list_;
  free_list_ = *static_cast<void**>(slot);
  ++live_;
  return slot;
}

void PoolAllocator::Deallocate(void* ptr, size_type size) {
  (void)size;
  if (ptr == nullptr) {
    return;
  }

  *static_cast<void**>(ptr) = free_list_;
  free_list_ = ptr;
  ASSERT(live_ > 0);
  --live_;
}

void PoolAllocator::Reset() {
  // Rebuild the free list from every block and drop the live counter.
  free_list_ = nullptr;
  live_ = 0;

  const size_type data_offset =
      static_cast<size_type>(AlignUp(sizeof(Block), kDefaultAlignment));
  for (Block* block = head_; block != nullptr; block = block->next) {
    byte* slots = reinterpret_cast<byte*>(block) + data_offset;
    for (size_type i = 0; i < block->slot_count; ++i) {
      void* slot = slots + i * slot_size_;
      *static_cast<void**>(slot) = free_list_;
      free_list_ = slot;
    }
  }
}

}  // namespace amaryl
