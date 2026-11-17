#pragma once

#include "amaryl/core/macros.h"

namespace amaryl {

// Non-ref counted handles (uint32_t and uint16_t), Based on
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
template <typename HandleTag, typename IndexType = uint32>
struct Handle final {
  static_assert(sizeof(IndexType) >= sizeof(uint16) &&
                    sizeof(IndexType) <= sizeof(uint32),
                "Unsupported IndexType size");

  Handle() = default;

  //< Pair to function `handleAsVoid`, unpack `void*` to `Handle` type.
  explicit Handle(void* ptr)
      : index_(static_cast<IndexType>(reinterpret_cast<uintptr_t>(ptr))),
        gen_(static_cast<IndexType>(static_cast<uint64>(
            reinterpret_cast<uintptr_t>(ptr) >> (sizeof(IndexType) * 8)))) {
    static_assert(
        sizeof(void*) >= sizeof(IndexType) * 2,
        "Packing a handle into a pointer requires a pointer twice the index width");
  }

  FORCE_INLINE bool empty() const { return gen_ == 0; }
  FORCE_INLINE bool valid() const { return gen_ != 0; }

  FORCE_INLINE IndexType index() const { return index_; }
  FORCE_INLINE IndexType gen() const { return gen_; }

  FORCE_INLINE void* indexAsVoid() const {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(index_));
  }

  //< The layout of result.
  // 63                         32 31                          0
  // +----------------------------+----------------------------+
  // |         generation         |            index           |
  // +----------------------------+----------------------------+
  //
  FORCE_INLINE void* handleAsVoid() const {
    static_assert(
        sizeof(void*) >= sizeof(IndexType) * 2,
        "Packing a handle into a pointer requires a pointer twice the index width");

    return reinterpret_cast<void*>(
        static_cast<uintptr_t>(static_cast<uint64>(gen_) << (sizeof(IndexType) * 8)) |
        index_);
  }

  FORCE_INLINE bool operator==(const Handle<HandleTag, IndexType>& other) const {
    return index_ == other.index_ && gen_ == other.gen_;
  }

  FORCE_INLINE bool operator!=(const Handle<HandleTag, IndexType>& other) const {
    return index_ != other.index_ || gen_ != other.gen_;
  }

  FORCE_INLINE explicit operator bool() const { return gen_ != 0u; }

 private:
  Handle(IndexType index, IndexType gen) : index_(index), gen_(gen) {}
  template <typename PoolHandleTag, typename ObjectType, typename PoolIndexType>
  friend struct Pool;

  IndexType index_{0u};
  IndexType gen_{0u};
};

}  // namespace amaryl

// should be used in namespace amaryl.
#define DECLARE_AMARYL_HANDLE(HandlePrefix) \
  using HandlePrefix##Handle = Handle<struct HandlePrefix##Tag>
