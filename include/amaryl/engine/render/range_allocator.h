#pragma once

#include <map>

#include "amaryl/engine/common.h"

namespace amaryl {

// Best-fit allocator of element ranges in a growable linear space, e.g. a GPU buffer.
// Only bookkeeping, the space grows at the end when no free range fits and freed ranges
// are coalesced with their neighbors.
struct ENGINE_API RangeAllocator final {
  // Returns the offset of `size` contiguous elements, `size` must not be 0.
  uint32 allocate(uint32 size);
  // `offset` and `size` must match a previous `allocate()`.
  void free(uint32 offset, uint32 size);
  void reset();

  // One past the highest allocated element, the capacity the buffer needs.
  uint32 getEnd() const { return end_; }
  uint32 getAllocatedSize() const { return allocated_; }

 private:
  void insertFree(uint32 offset, uint32 size);
  void eraseFree(std::map<uint32, uint32>::iterator it);

  // offset -> size and size -> offset of every free range below `end_`.
  std::map<uint32, uint32> free_by_offset_;
  std::multimap<uint32, uint32> free_by_size_;
  uint32 end_{0};
  uint32 allocated_{0};
};

}  // namespace amaryl
