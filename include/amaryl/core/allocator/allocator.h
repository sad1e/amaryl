#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "amaryl/core/config.h"
#include "amaryl/core/types.h"

namespace amaryl {

// Default allocation alignment. Mirrors VECTOR_ALIGNMENT so SIMD/vector types can
// be allocated without extra work.
inline constexpr size_t kDefaultAlignment = DEFAULT_ALIGNMENT;

// Base interface for every memory allocator.
//
// The interface is modelled on the C++ standard library's `Allocator` named
// requirements: the raw `Allocate`/`Deallocate` map to `allocate`/`deallocate`,
// and the `StlAllocator<T, TAllocator>` adapter exposes any derived allocator to
// STL containers.
//
// Implementations are intentionally not thread-safe unless documented.
class CORE_API IAllocator {
 public:
  using value_type = byte;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  virtual ~IAllocator() = default;

  // Allocate `size` bytes aligned to `alignment` (a power of two).
  // Returns nullptr only when the allocator cannot satisfy the request.
  [[nodiscard]] virtual void* Allocate(size_type size,
                                       size_type alignment = kDefaultAlignment) = 0;

  // Free a pointer previously returned by `Allocate`. `size` is optional and is
  // only required by allocators that do not track allocation sizes on their own.
  virtual void Deallocate(void* ptr, size_type size = 0) = 0;

  // Grow or shrink an allocation. The default implementation allocates a new
  // block, copies `min(old_size, new_size)` bytes and frees the old block.
  // Linear allocators (bump/arena) keep the old block until Reset(); allocators
  // that cannot support this at all must override it.
  [[nodiscard]] virtual void* Reallocate(void* ptr, size_type old_size,
                                         size_type new_size,
                                         size_type alignment = kDefaultAlignment);

  // Reset the allocator to its initial state. Linear allocators (bump/arena)
  // discard all live allocations; pool allocators rebuild their free list.
  virtual void Reset() {}

  // vvv STL-compatible convenience interface (operates on `value_type` == byte) vvv

  [[nodiscard]] pointer allocate(size_type n) {
    return static_cast<pointer>(Allocate(n * sizeof(value_type), alignof(value_type)));
  }

  void deallocate(pointer p, size_type n) { Deallocate(p, n * sizeof(value_type)); }

  [[nodiscard]] size_type max_size() const noexcept {
    return size_type(-1) / sizeof(value_type);
  }

  // ^^^ STL-compatible convenience interface ^^^
};

// Process-wide default allocator backed by mimalloc.
CORE_API IAllocator& DefaultAllocator();

// Allocator backed directly by mimalloc's aligned allocation primitives. This is
// the basic interface the other allocators build on: every allocation goes
// through `mi_malloc_aligned`/`mi_realloc_aligned` and every free through
// `mi_free`.
class CORE_API MimallocAllocator final : public IAllocator {
 public:
  void* Allocate(size_type size, size_type alignment = kDefaultAlignment) override;
  void Deallocate(void* ptr, size_type size = 0) override;
  void* Reallocate(void* ptr, size_type old_size, size_type new_size,
                   size_type alignment = kDefaultAlignment) override;
};

// Adapter exposing an `IAllocator`-derived allocator through the standard
// `Allocator` interface so it can back STL containers.
//
//   ArenaAllocator arena;
//   std::vector<int, StlAllocator<int, ArenaAllocator>> values(
//       StlAllocator<int, ArenaAllocator>(arena));
//
// The adapter is stateful (it refers to a concrete allocator instance), so it
// must be constructed from a live allocator; a default-constructed adapter is
// only usable as a copy source for a container that already owns one.
template <typename T, typename TAllocator>
class StlAllocator {
 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using void_pointer = void*;
  using const_void_pointer = const void*;
  using is_always_equal = std::false_type;

  template <typename U>
  struct rebind {
    using other = StlAllocator<U, TAllocator>;
  };

  StlAllocator() noexcept = default;
  StlAllocator(TAllocator& allocator) noexcept : allocator_(&allocator) {}

  template <typename U>
  StlAllocator(const StlAllocator<U, TAllocator>& other) noexcept
      : allocator_(other.allocator_) {}

  [[nodiscard]] pointer allocate(size_type n) {
    return static_cast<pointer>(allocator_->Allocate(n * sizeof(T), alignof(T)));
  }

  void deallocate(pointer p, size_type n) { allocator_->Deallocate(p, n * sizeof(T)); }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  [[nodiscard]] size_type max_size() const noexcept { return size_type(-1) / sizeof(T); }

  // The underlying allocator this adapter forwards to.
  [[nodiscard]] TAllocator& resource() const noexcept { return *allocator_; }

  StlAllocator select_on_container_copy_construction() const noexcept { return *this; }

  template <typename U, typename UAllocator>
  friend class StlAllocator;

 private:
  TAllocator* allocator_ = nullptr;
};

template <typename T, typename U, typename TAllocator>
bool operator==(const StlAllocator<T, TAllocator>& lhs,
                const StlAllocator<U, TAllocator>& rhs) noexcept {
  return &lhs.resource() == &rhs.resource();
}

template <typename T, typename U, typename TAllocator>
bool operator!=(const StlAllocator<T, TAllocator>& lhs,
                const StlAllocator<U, TAllocator>& rhs) noexcept {
  return !(lhs == rhs);
}

}  // namespace amaryl
