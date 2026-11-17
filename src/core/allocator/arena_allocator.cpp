#include "amaryl/core/allocator/arena_allocator.h"

#include <mimalloc.h>

#include "amaryl/core/assert.h"
#include "amaryl/core/templates.h"

namespace amaryl {

struct ArenaAllocator::Block {
  Block* next = nullptr;
  size_type used = 0;      // bytes consumed in this block
  size_type capacity = 0;  // usable bytes in this block
};

ArenaAllocator::ArenaAllocator(size_type block_size) : block_size_(block_size) {}

ArenaAllocator::~ArenaAllocator() { Reset(); }

byte* ArenaAllocator::BlockData(Block* block) noexcept {
  return reinterpret_cast<byte*>(
      AlignUp(reinterpret_cast<uintptr_t>(block) + sizeof(Block), kDefaultAlignment));
}

ArenaAllocator::Block* ArenaAllocator::NewBlock(size_type capacity) {
  // Reserve room for the header plus enough padding to keep `BlockData` aligned.
  const size_type alloc_size = sizeof(Block) + (kDefaultAlignment - 1) + capacity;
  auto* block = static_cast<Block*>(mi_malloc_aligned(alloc_size, kDefaultAlignment));
  block->next = head_;
  block->used = 0;
  block->capacity = capacity;

  head_ = block;
  current_ = block;
  return block;
}

void* ArenaAllocator::Allocate(size_type size, size_type alignment) {
  ASSERT(alignment != 0 && IsPowerOfTwo(alignment), "alignment must be a power of two");

  if (current_ == nullptr) {
    NewBlock(block_size_ < size ? size : block_size_);
  }

  Block* block = current_;
  byte* data = BlockData(block);
  const uintptr_t cursor = reinterpret_cast<uintptr_t>(data) + block->used;
  const uintptr_t aligned = AlignUp(cursor, alignment);
  const size_type offset = static_cast<size_type>(aligned - reinterpret_cast<uintptr_t>(data));

  if (offset + size > block->capacity) {
    // Current block cannot fit the allocation; bump in a fresh block.
    block = NewBlock(block_size_ < size ? size : block_size_);
    data = BlockData(block);
    block->used = size;
    return data;  // `data` is already aligned to kDefaultAlignment
  }

  block->used = offset + size;
  return reinterpret_cast<void*>(aligned);
}

void ArenaAllocator::Reset() {
  Block* block = head_;
  while (block != nullptr) {
    Block* next = block->next;
    mi_free(block);
    block = next;
  }
  head_ = nullptr;
  current_ = nullptr;
}

size_t ArenaAllocator::Used() const noexcept {
  size_type total = 0;
  for (Block* block = head_; block != nullptr; block = block->next) {
    total += block->used;
  }
  return total;
}

size_t ArenaAllocator::Capacity() const noexcept {
  size_type total = 0;
  for (Block* block = head_; block != nullptr; block = block->next) {
    total += block->capacity;
  }
  return total;
}

}  // namespace amaryl
