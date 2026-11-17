#include "amaryl/core/allocator/bump_allocator.h"

#include <mimalloc.h>

#include "amaryl/core/assert.h"
#include "amaryl/core/templates.h"

namespace amaryl {

BumpAllocator::BumpAllocator(void* memory, size_type size) noexcept
    : begin_(static_cast<byte*>(memory)),
      end_(begin_ + size),
      cursor_(begin_),
      owns_memory_(false) {}

BumpAllocator::BumpAllocator(size_type size)
    : begin_(static_cast<byte*>(mi_malloc_aligned(size, kDefaultAlignment))),
      end_(begin_ + size),
      cursor_(begin_),
      owns_memory_(true) {}

BumpAllocator::~BumpAllocator() {
  if (owns_memory_) {
    mi_free(begin_);
  }
}

void* BumpAllocator::Allocate(size_type size, size_type alignment) {
  ASSERT(alignment != 0 && IsPowerOfTwo(alignment), "alignment must be a power of two");

  byte* result =
      reinterpret_cast<byte*>(AlignUp(reinterpret_cast<uintptr_t>(cursor_), alignment));
  byte* next = result + size;
  if (next > end_) {
    return nullptr;  // buffer exhausted
  }

  cursor_ = next;
  return result;
}

}  // namespace amaryl
