#pragma once

#include <initializer_list>

#include "amaryl/core/traits.h"

namespace amaryl {

template <typename T>
struct Span {
  constexpr Span() noexcept = default;

  constexpr Span(T* data, size_t size) noexcept : data_(data), size_(size) {}

  template <size_t N>
  constexpr Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

  template <typename U>
  constexpr Span(Span<U> values) noexcept
    requires(is_same_v<remove_cv_t<U>, remove_cv_t<T>> && is_convertible_v<U*, T*>)
      : Span(values.data_, values.size_) {}

  template <typename Container>
  constexpr Span(Container&& values) noexcept
    requires(is_same_v<remove_cv_t<remove_pointer_t<decltype(values.data())>>,
                       remove_cv_t<T>> &&
             is_convertible_v<decltype(values.data()), T*> &&
             is_convertible_v<decltype(values.size()), size_t>)
      : Span(values.data(), values.size()) {}

  constexpr Span(std::initializer_list<remove_const_t<T>> values) noexcept
    requires is_const_v<T>
      : Span(values.begin(), values.end()) {}

  // std operations

  //
  const T& operator[](size_t idx) const { return data_[idx]; }
  T& operator[](size_t idx) { return data_[idx]; }

  size_t size() const { return size_; }
  size_t size_bytes() const { return sizeof(T) * size_; }
  bool empty() const { return size_ == 0; }
  Span subspan(size_t offset, size_t count = 0) const {
    return Span(data_ + offset, count ? count : size_ - offset);
  }

  T* data() { return data_; }
  const T* data() const { return data_; }

  T* begin() { return data_; }
  const T* begin() const { return data_; }
  T* end() { return data_ + size_; }
  const T* end() const { return data_ + size_; }
  const T* cbegin() const { return data_; }
  const T* cend() const { return data_ + size_; }

 private:
  T* data_{nullptr};
  size_t size_{0u};
};

struct ByteSpan {
  const std::byte* data{nullptr};
  size_t size{0};

  constexpr ByteSpan() noexcept = default;

  ByteSpan(const void* bytes, size_t bytes_size) noexcept
      : data(static_cast<const std::byte*>(bytes)), size(bytes_size) {}

  template <typename T>
    requires(__is_class(T) && is_volatile_v<T> && __is_trivially_copyable(T) &&
             (sizeof(T) & 3u) == 0)
  ByteSpan(const T& value) noexcept
      : data(reinterpret_cast<const std::byte*>(__builtin_addressof(value))),
        size(sizeof(T)) {}
};

}  // namespace amaryl
