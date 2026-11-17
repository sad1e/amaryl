#include "amaryl/core/allocator/allocator.h"

#include <cstring>

#include <mimalloc.h>

namespace amaryl {

IAllocator& DefaultAllocator() {
  static MimallocAllocator instance;
  return instance;
}

void* IAllocator::Reallocate(void* ptr, size_type old_size, size_type new_size,
                             size_type alignment) {
  if (new_size == 0) {
    Deallocate(ptr, old_size);
    return nullptr;
  }

  void* result = Allocate(new_size, alignment);
  if (ptr != nullptr) {
    std::memcpy(result, ptr, old_size < new_size ? old_size : new_size);
    Deallocate(ptr, old_size);
  }
  return result;
}

void* MimallocAllocator::Allocate(size_type size, size_type alignment) {
  return mi_malloc_aligned(size, alignment);
}

void MimallocAllocator::Deallocate(void* ptr, size_type size) {
  (void)size;
  mi_free(ptr);
}

void* MimallocAllocator::Reallocate(void* ptr, size_type old_size, size_type new_size,
                                    size_type alignment) {
  (void)old_size;
  return mi_realloc_aligned(ptr, new_size, alignment);
}

}  // namespace amaryl
