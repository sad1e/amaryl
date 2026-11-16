#pragma once

#include "amaryl/core/atomic_cxx11.h"

namespace amaryl {

template <typename T>
struct TBasicAtomicInt {
  typedef T ValueType;
  typedef TAtomicOperations<T> AtomicOperations;

  static_assert(std::is_integral_v<T>, "template parameter is not an integral type");
  static_assert(TAtomicOperationsSupport<sizeof(T)>::bSupported,
                "template parameter is an integral of a size not supported on "
                "this platform");

  typename AtomicOperations::ValueType value;

  T LoadRelaxed() const noexcept { return AtomicOperations::loadRelaxed(value); }
  void StoreRelaxed(T new_value) noexcept {
    AtomicOperations::storeRelaxed(value, new_value);
  }

  T LoadAcquire() const noexcept { return AtomicOperations::loadAcquire(value); }
  void StoreRelease(T new_value) noexcept {
    AtomicOperations::storeRelease(value, new_value);
  }

  operator T() const noexcept { return loadAcquire(); }
  T operator=(T new_value) noexcept {
    storeRelease(new_value);
    return new_value;
  }

  static constexpr bool IsReferenceCountingNative() {
    return AtomicOperations::IsReferenceCountingNative();
  }
  static constexpr bool IsReferenceCountingWaitFree() {
    return AtomicOperations::IsReferenceCountingWaitFree();
  }

  bool Ref() noexcept { return AtomicOperations::Ref(value); }
  bool DeRef() noexcept { return AtomicOperations::DeRef(value); }

  static constexpr bool IsTestAndSetNative() noexcept {
    return AtomicOperations::IsTestAndSetNative();
  }
  static constexpr bool IsTestAndSetWaitFree() noexcept {
    return AtomicOperations::IsTestAndSetWaitFree();
  }

  bool TestAndSetRelaxed(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetRelaxed(value, expected_value, new_value);
  }
  bool TestAndSetAcquire(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetAcquire(value, expected_value, new_value);
  }
  bool TestAndSetRelease(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetRelease(value, expected_value, new_value);
  }
  bool TestAndSetOrdered(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetOrdered(value, expected_value, new_value);
  }

  bool TestAndSetRelaxed(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetRelaxed(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetAcquire(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetAcquire(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetRelease(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetRelease(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetOrdered(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetOrdered(value, expected_value, new_value,
                                               &current_value);
  }

  static constexpr bool IsFetchStoreNative() noexcept {
    return AtomicOperations::IsFetchStoreNative();
  }
  static constexpr bool IsFetchStoreWaitFree() noexcept {
    return AtomicOperations::IsFetchStoreWaitFree();
  }

  T FetchStoreRelaxed(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelaxed(value, new_value);
  }
  T FetchStoreAcquire(T new_value) noexcept {
    return AtomicOperations::fetchStoreAcquire(value, new_value);
  }
  T FetchStoreRelease(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelease(value, new_value);
  }
  T FetchStoreOrdered(T new_value) noexcept {
    return AtomicOperations::fetchStoreOrdered(value, new_value);
  }

  static constexpr bool IsFetchAddNative() noexcept {
    return AtomicOperations::IsFetchAddNative();
  }
  static constexpr bool IsFetchAddWaitFree() noexcept {
    return AtomicOperations::IsFetchAddWaitFree();
  }

  T FetchAddRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchAddRelaxed(value, value_to_add);
  }
  T FetchAddAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchAddAcquire(value, value_to_add);
  }
  T FetchAddRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchAddRelease(value, value_to_add);
  }
  T FetchAddOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchAddOrdered(value, value_to_add);
  }

  T FetchSubRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchSubRelaxed(value, value_to_add);
  }
  T FetchSubAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchSubAcquire(value, value_to_add);
  }
  T FetchSubRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchSubRelease(value, value_to_add);
  }
  T FetchSubOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchSubOrdered(value, value_to_add);
  }

  T FetchAndRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchAndRelaxed(value, value_to_add);
  }
  T FetchAndAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchAndAcquire(value, value_to_add);
  }
  T FetchAndRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchAndRelease(value, value_to_add);
  }
  T FetchAndOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchAndOrdered(value, value_to_add);
  }

  T FetchOrRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchOrRelaxed(value, value_to_add);
  }
  T FetchOrAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchOrAcquire(value, value_to_add);
  }
  T FetchOrRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchOrRelease(value, value_to_add);
  }
  T FetchOrOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchOrOrdered(value, value_to_add);
  }

  T FetchXorRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchXorRelaxed(value, value_to_add);
  }
  T FetchXorAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchXorAcquire(value, value_to_add);
  }
  T FetchXorRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchXorRelease(value, value_to_add);
  }
  T FetchXorOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchXorOrdered(value, value_to_add);
  }

  T operator++() noexcept { return fetchAddOrdered(1) + 1; }
  T operator++(int) noexcept { return fetchAddOrdered(1); }
  T operator--() noexcept { return fetchSubOrdered(1) - 1; }
  T operator--(int) noexcept { return fetchSubOrdered(1); }

  T operator+=(T v) noexcept { return fetchAddOrdered(v) + v; }
  T operator-=(T v) noexcept { return fetchSubOrdered(v) - v; }
  T operator&=(T v) noexcept { return fetchAndOrdered(v) & v; }
  T operator|=(T v) noexcept { return fetchOrOrdered(v) | v; }
  T operator^=(T v) noexcept { return fetchXorOrdered(v) ^ v; }

  TBasicAtomicInt() = default;
  constexpr TBasicAtomicInt(T v) noexcept : value(v) {}
  TBasicAtomicInt(const TBasicAtomicInt&) = delete;
  TBasicAtomicInt& operator=(const TBasicAtomicInt&) = delete;
  TBasicAtomicInt& operator=(const TBasicAtomicInt&) volatile = delete;
};

typedef TBasicAtomicInt<int> BasicAtomicInt;

template <typename T>
struct TBasicAtomicPointer {
  typedef T* TPtr;
  typedef TAtomicOperations<TPtr> AtomicOperations;
  typedef typename AtomicOperations::ValueType AtomicValueType;

  AtomicValueType value;

  T LoadRelaxed() const noexcept { return AtomicOperations::loadRelaxed(value); }
  void StoreRelaxed(T new_value) noexcept {
    AtomicOperations::storeRelaxed(value, new_value);
  }

  T LoadAcquire() const noexcept { return AtomicOperations::loadAcquire(value); }
  void StoreRelease(T new_value) noexcept {
    AtomicOperations::storeRelease(value, new_value);
  }

  operator T() const noexcept { return loadAcquire(); }
  T operator=(T new_value) noexcept {
    storeRelease(new_value);
    return new_value;
  }

  static constexpr bool IsTestAndSetNative() noexcept {
    return AtomicOperations::IsTestAndSetNative();
  }
  static constexpr bool IsTestAndSetWaitFree() noexcept {
    return AtomicOperations::IsTestAndSetWaitFree();
  }

  bool TestAndSetRelaxed(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetRelaxed(value, expected_value, new_value);
  }
  bool TestAndSetAcquire(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetAcquire(value, expected_value, new_value);
  }
  bool TestAndSetRelease(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetRelease(value, expected_value, new_value);
  }
  bool TestAndSetOrdered(T expected_value, T new_value) noexcept {
    return AtomicOperations::TestAndSetOrdered(value, expected_value, new_value);
  }

  bool TestAndSetRelaxed(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetRelaxed(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetAcquire(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetAcquire(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetRelease(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetRelease(value, expected_value, new_value,
                                               &current_value);
  }
  bool TestAndSetOrdered(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::TestAndSetOrdered(value, expected_value, new_value,
                                               &current_value);
  }

  static constexpr bool IsFetchStoreNative() noexcept {
    return AtomicOperations::IsFetchStoreNative();
  }
  static constexpr bool IsFetchStoreWaitFree() noexcept {
    return AtomicOperations::IsFetchStoreWaitFree();
  }

  T FetchStoreRelaxed(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelaxed(value, new_value);
  }
  T FetchStoreAcquire(T new_value) noexcept {
    return AtomicOperations::fetchStoreAcquire(value, new_value);
  }
  T FetchStoreRelease(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelease(value, new_value);
  }
  T FetchStoreOrdered(T new_value) noexcept {
    return AtomicOperations::fetchStoreOrdered(value, new_value);
  }

  static constexpr bool IsFetchAddNative() noexcept {
    return AtomicOperations::IsFetchAddNative();
  }
  static constexpr bool IsFetchAddWaitFree() noexcept {
    return AtomicOperations::IsFetchAddWaitFree();
  }

  T FetchAddRelaxed(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddRelaxed(value, value_to_add);
  }
  T FetchAddAcquire(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddAcquire(value, value_to_add);
  }
  T FetchAddRelease(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddRelease(value, value_to_add);
  }
  T FetchAddOrdered(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddOrdered(value, value_to_add);
  }

  T FetchSubRelaxed(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubRelaxed(value, value_to_add);
  }
  T FetchSubAcquire(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubAcquire(value, value_to_add);
  }
  T FetchSubRelease(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubRelease(value, value_to_add);
  }
  T FetchSubOrdered(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubOrdered(value, value_to_add);
  }

  T operator++() noexcept { return fetchAddOrdered(1) + 1; }
  T operator++(int) noexcept { return fetchAddOrdered(1); }
  T operator--() noexcept { return fetchSubOrdered(1) - 1; }
  T operator--(int) noexcept { return fetchSubOrdered(1); }

  T operator+=(T v) noexcept { return fetchAddOrdered(v) + v; }
  T operator-=(T v) noexcept { return fetchSubOrdered(v) - v; }

  TBasicAtomicPointer() = default;
  constexpr TBasicAtomicPointer(T v) noexcept : value(v) {}
  TBasicAtomicPointer(const TBasicAtomicPointer&) = delete;
  TBasicAtomicPointer& operator=(const TBasicAtomicPointer&) = delete;
  TBasicAtomicPointer& operator=(const TBasicAtomicPointer&) volatile = delete;
};

}  // namespace amaryl
