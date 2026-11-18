#include "amaryl/engine/render/range_allocator.h"

#include <iterator>

namespace amaryl {

uint32 RangeAllocator::allocate(uint32 size) {
  ASSERT(size != 0, "RangeAllocator: empty allocation");
  allocated_ += size;

  const auto best = free_by_size_.lower_bound(size);
  if (best == free_by_size_.end()) {
    ASSERT(end_ <= 0xffffffffu - size, "RangeAllocator: out of space");
    const uint32 offset = end_;
    end_ += size;
    return offset;
  }

  const uint32 offset = best->second;
  const uint32 free_size = best->first;
  free_by_size_.erase(best);
  free_by_offset_.erase(offset);
  if (free_size > size) {
    insertFree(offset + size, free_size - size);
  }
  return offset;
}

void RangeAllocator::free(uint32 offset, uint32 size) {
  ASSERT(size != 0 && size <= allocated_ && offset + size <= end_,
         "RangeAllocator: invalid free");
  allocated_ -= size;

  auto next = free_by_offset_.lower_bound(offset);
  ASSERT(next == free_by_offset_.end() || next->first >= offset + size,
         "RangeAllocator: range is already free");
  if (next != free_by_offset_.end() && next->first == offset + size) {
    size += next->second;
    const auto merged = next++;
    eraseFree(merged);
  }
  if (next != free_by_offset_.begin()) {
    const auto prev = std::prev(next);
    ASSERT(prev->first + prev->second <= offset, "RangeAllocator: range is already free");
    if (prev->first + prev->second == offset) {
      offset = prev->first;
      size += prev->second;
      eraseFree(prev);
    }
  }

  // Free ranges never touch the end, the space shrinks instead.
  if (offset + size == end_) {
    end_ = offset;
  } else {
    insertFree(offset, size);
  }
}

void RangeAllocator::reset() {
  free_by_offset_.clear();
  free_by_size_.clear();
  end_ = 0;
  allocated_ = 0;
}

void RangeAllocator::insertFree(uint32 offset, uint32 size) {
  free_by_offset_.emplace(offset, size);
  free_by_size_.emplace(size, offset);
}

void RangeAllocator::eraseFree(std::map<uint32, uint32>::iterator it) {
  auto [first, last] = free_by_size_.equal_range(it->second);
  for (; first != last; ++first) {
    if (first->second == it->first) {
      free_by_size_.erase(first);
      break;
    }
  }
  free_by_offset_.erase(it);
}

}  // namespace amaryl
