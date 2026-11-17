#pragma once

#include "amaryl/core/allocator/allocator.h"

namespace amaryl {

// Linear ("bump") allocator over a single fixed buffer.
//
// Allocations simply advance a cursor; individual deallocation is a no-op and
// `Reset()` rewinds the cursor to the start. Use it for short-lived scratch
// data (per-frame/per-pass) where everything is released together.
class CORE_API BumpAllocator final : public IAllocator {
 public:
  // Bump over a caller-provided buffer. The buffer is not owned and must outlive
  // the allocator.
  BumpAllocator(void* memory, size_type size) noexcept;
  // Own a mimalloc-allocated buffer of `size` bytes.
  explicit BumpAllocator(size_type size);

  BumpAllocator(const BumpAllocator&) = delete;
  BumpAllocator& operator=(const BumpAllocator&) = delete;

  ~BumpAllocator() override;

  // Bump `size` bytes aligned to `alignment`. Returns nullptr when the buffer is
  // exhausted.
  void* Allocate(size_type size, size_type alignment = kDefaultAlignment) override;
  void Deallocate(void* ptr, size_type size = 0) override {}  // no-op; use Reset()

  void Reset() override { cursor_ = begin_; }

  [[nodiscard]] size_type Used() const noexcept {
    return static_cast<size_type>(cursor_ - begin_);
  }
  [[nodiscard]] size_type Capacity() const noexcept {
    return static_cast<size_type>(end_ - begin_);
  }

 private:
  byte* begin_ = nullptr;
  byte* end_ = nullptr;
  byte* cursor_ = nullptr;
  bool owns_memory_ = false;
};

}  // namespace amaryl
