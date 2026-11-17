#pragma once

#include "amaryl/core/allocator/allocator.h"

namespace amaryl {

// Default capacity of each arena block (1 MiB).
inline constexpr size_t kDefaultArenaBlockSize = 1u << 20;

// Growable linear ("bump") allocator.
//
// Like BumpAllocator, allocations advance a cursor and individual deallocation is
// a no-op, but when a block fills up the arena allocates another block from
// mimalloc. `Reset()` releases every block, so the whole arena is typically freed
// at once (e.g. once per frame).
class CORE_API ArenaAllocator final : public IAllocator {
 public:
  explicit ArenaAllocator(size_type block_size = kDefaultArenaBlockSize);

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  ~ArenaAllocator() override;

  // Bump `size` bytes aligned to `alignment`, growing with a new block when the
  // current one is exhausted.
  void* Allocate(size_type size, size_type alignment = kDefaultAlignment) override;
  void Deallocate(void* ptr, size_type size = 0) override {}  // no-op; use Reset()

  void Reset() override;

  [[nodiscard]] size_type Used() const noexcept;      // bytes handed out
  [[nodiscard]] size_type Capacity() const noexcept;  // bytes reserved

 private:
  struct Block;

  Block* NewBlock(size_type capacity);
  static byte* BlockData(Block* block) noexcept;

  size_type block_size_;
  Block* head_ = nullptr;     // first block (freed on Reset)
  Block* current_ = nullptr;  // block currently being bumped
};

}  // namespace amaryl
