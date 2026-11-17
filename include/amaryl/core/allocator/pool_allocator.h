#pragma once

#include "amaryl/core/allocator/allocator.h"

namespace amaryl {

// Default number of slots allocated per block.
inline constexpr size_t kDefaultPoolSlotsPerBlock = 256;

// Fixed-size block pool with O(1) allocate and free.
//
// Every allocation returns a slot of `slot_size` bytes (rounded up to the default
// alignment). The requested size is ignored and must not exceed `slot_size`.
// Freed slots are recycled through an intrusive free list, making this ideal for
// many same-sized short-lived objects.
class CORE_API PoolAllocator final : public IAllocator {
 public:
  PoolAllocator(size_type slot_size,
                size_type slots_per_block = kDefaultPoolSlotsPerBlock);

  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;

  ~PoolAllocator() override;

  // Pop a slot from the free list, growing the pool with a new block when empty.
  // `alignment` must not exceed kDefaultAlignment.
  void* Allocate(size_type size, size_type alignment = kDefaultAlignment) override;
  // Push a slot back onto the free list.
  void Deallocate(void* ptr, size_type size = 0) override;

  void Reset() override;

  [[nodiscard]] size_type SlotSize() const noexcept { return slot_size_; }
  [[nodiscard]] size_type LiveAllocations() const noexcept { return live_; }

 private:
  struct Block;

  void Grow();

  size_type slot_size_;
  size_type slots_per_block_;
  size_type live_ = 0;
  void* free_list_ = nullptr;
  Block* head_ = nullptr;  // for Reset/destruction
};

}  // namespace amaryl
