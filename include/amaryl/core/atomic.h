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

  T loadRelaxed() const noexcept { return AtomicOperations::loadRelaxed(value); }
  void storeRelaxed(T new_value) noexcept {
    AtomicOperations::storeRelaxed(value, new_value);
  }

  T loadAcquire() const noexcept { return AtomicOperations::loadAcquire(value); }
  void storeRelease(T new_value) noexcept {
    AtomicOperations::storeRelease(value, new_value);
  }

  operator T() const noexcept { return loadAcquire(); }
  T operator=(T new_value) noexcept {
    storeRelease(new_value);
    return new_value;
  }

  static constexpr bool isReferenceCountingNative() {
    return AtomicOperations::isReferenceCountingNative();
  }
  static constexpr bool isReferenceCountingWaitFree() {
    return AtomicOperations::isReferenceCountingWaitFree();
  }

  bool ref() noexcept { return AtomicOperations::ref(value); }
  bool deRef() noexcept { return AtomicOperations::deRef(value); }

  static constexpr bool isTestAndSetNative() noexcept {
    return AtomicOperations::isTestAndSetNative();
  }
  static constexpr bool isTestAndSetWaitFree() noexcept {
    return AtomicOperations::isTestAndSetWaitFree();
  }

  bool testAndSetRelaxed(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetRelaxed(value, expected_value, new_value);
  }
  bool testAndSetAcquire(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetAcquire(value, expected_value, new_value);
  }
  bool testAndSetRelease(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetRelease(value, expected_value, new_value);
  }
  bool testAndSetOrdered(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetOrdered(value, expected_value, new_value);
  }

  bool testAndSetRelaxed(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetRelaxed(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetAcquire(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetAcquire(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetRelease(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetRelease(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetOrdered(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetOrdered(value, expected_value, new_value,
                                               &current_value);
  }

  static constexpr bool isFetchStoreNative() noexcept {
    return AtomicOperations::isFetchStoreNative();
  }
  static constexpr bool isFetchStoreWaitFree() noexcept {
    return AtomicOperations::isFetchStoreWaitFree();
  }

  T fetchStoreRelaxed(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelaxed(value, new_value);
  }
  T fetchStoreAcquire(T new_value) noexcept {
    return AtomicOperations::fetchStoreAcquire(value, new_value);
  }
  T fetchStoreRelease(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelease(value, new_value);
  }
  T fetchStoreOrdered(T new_value) noexcept {
    return AtomicOperations::fetchStoreOrdered(value, new_value);
  }

  static constexpr bool isFetchAddNative() noexcept {
    return AtomicOperations::isFetchAddNative();
  }
  static constexpr bool isFetchAddWaitFree() noexcept {
    return AtomicOperations::isFetchAddWaitFree();
  }

  T fetchAddRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchAddRelaxed(value, value_to_add);
  }
  T fetchAddAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchAddAcquire(value, value_to_add);
  }
  T fetchAddRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchAddRelease(value, value_to_add);
  }
  T fetchAddOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchAddOrdered(value, value_to_add);
  }

  T fetchSubRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchSubRelaxed(value, value_to_add);
  }
  T fetchSubAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchSubAcquire(value, value_to_add);
  }
  T fetchSubRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchSubRelease(value, value_to_add);
  }
  T fetchSubOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchSubOrdered(value, value_to_add);
  }

  T fetchAndRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchAndRelaxed(value, value_to_add);
  }
  T fetchAndAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchAndAcquire(value, value_to_add);
  }
  T fetchAndRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchAndRelease(value, value_to_add);
  }
  T fetchAndOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchAndOrdered(value, value_to_add);
  }

  T fetchOrRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchOrRelaxed(value, value_to_add);
  }
  T fetchOrAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchOrAcquire(value, value_to_add);
  }
  T fetchOrRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchOrRelease(value, value_to_add);
  }
  T fetchOrOrdered(T value_to_add) noexcept {
    return AtomicOperations::fetchOrOrdered(value, value_to_add);
  }

  T fetchXorRelaxed(T value_to_add) noexcept {
    return AtomicOperations::fetchXorRelaxed(value, value_to_add);
  }
  T fetchXorAcquire(T value_to_add) noexcept {
    return AtomicOperations::fetchXorAcquire(value, value_to_add);
  }
  T fetchXorRelease(T value_to_add) noexcept {
    return AtomicOperations::fetchXorRelease(value, value_to_add);
  }
  T fetchXorOrdered(T value_to_add) noexcept {
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

  T loadRelaxed() const noexcept { return AtomicOperations::loadRelaxed(value); }
  void storeRelaxed(T new_value) noexcept {
    AtomicOperations::storeRelaxed(value, new_value);
  }

  T loadAcquire() const noexcept { return AtomicOperations::loadAcquire(value); }
  void storeRelease(T new_value) noexcept {
    AtomicOperations::storeRelease(value, new_value);
  }

  operator T() const noexcept { return loadAcquire(); }
  T operator=(T new_value) noexcept {
    storeRelease(new_value);
    return new_value;
  }

  static constexpr bool isTestAndSetNative() noexcept {
    return AtomicOperations::isTestAndSetNative();
  }
  static constexpr bool isTestAndSetWaitFree() noexcept {
    return AtomicOperations::isTestAndSetWaitFree();
  }

  bool testAndSetRelaxed(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetRelaxed(value, expected_value, new_value);
  }
  bool testAndSetAcquire(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetAcquire(value, expected_value, new_value);
  }
  bool testAndSetRelease(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetRelease(value, expected_value, new_value);
  }
  bool testAndSetOrdered(T expected_value, T new_value) noexcept {
    return AtomicOperations::testAndSetOrdered(value, expected_value, new_value);
  }

  bool testAndSetRelaxed(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetRelaxed(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetAcquire(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetAcquire(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetRelease(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetRelease(value, expected_value, new_value,
                                               &current_value);
  }
  bool testAndSetOrdered(T expected_value, T new_value, T& current_value) noexcept {
    return AtomicOperations::testAndSetOrdered(value, expected_value, new_value,
                                               &current_value);
  }

  static constexpr bool isFetchStoreNative() noexcept {
    return AtomicOperations::isFetchStoreNative();
  }
  static constexpr bool isFetchStoreWaitFree() noexcept {
    return AtomicOperations::isFetchStoreWaitFree();
  }

  T fetchStoreRelaxed(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelaxed(value, new_value);
  }
  T fetchStoreAcquire(T new_value) noexcept {
    return AtomicOperations::fetchStoreAcquire(value, new_value);
  }
  T fetchStoreRelease(T new_value) noexcept {
    return AtomicOperations::fetchStoreRelease(value, new_value);
  }
  T fetchStoreOrdered(T new_value) noexcept {
    return AtomicOperations::fetchStoreOrdered(value, new_value);
  }

  static constexpr bool isFetchAddNative() noexcept {
    return AtomicOperations::isFetchAddNative();
  }
  static constexpr bool isFetchAddWaitFree() noexcept {
    return AtomicOperations::isFetchAddWaitFree();
  }

  T fetchAddRelaxed(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddRelaxed(value, value_to_add);
  }
  T fetchAddAcquire(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddAcquire(value, value_to_add);
  }
  T fetchAddRelease(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddRelease(value, value_to_add);
  }
  T fetchAddOrdered(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchAddOrdered(value, value_to_add);
  }

  T fetchSubRelaxed(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubRelaxed(value, value_to_add);
  }
  T fetchSubAcquire(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubAcquire(value, value_to_add);
  }
  T fetchSubRelease(ptrdiff_t value_to_add) noexcept {
    return AtomicOperations::fetchSubRelease(value, value_to_add);
  }
  T fetchSubOrdered(ptrdiff_t value_to_add) noexcept {
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
