#pragma once

#include <atomic>
#include <cstddef>

#include "amaryl/core/macros.h"

namespace amaryl {

template <int Size>
struct TAtomicOperationsSupport {
  enum { bSupported = (Size == sizeof(int) || Size == sizeof(ptrdiff_t)) };
};

template <typename T>
struct TAtomicAdditiveType {
  typedef T AdditiveType;
  static const int kAddScale = 1;
};

template <typename T>
struct TAtomicAdditiveType<T*> {
  typedef ptrdiff_t AdditiveType;
  static const int kAddScale = sizeof(T);
};

template <int N>
struct TAtomicTraits {
  static inline bool isLockFree();
};

// clang-format off
#if ATOMIC_INT_LOCK_FREE == 2
  template <>
  inline bool TAtomicTraits<4>::isLockFree() { return true; }
#elif ATOMIC_INT_LOCK_FREE == 1
  template <>
  inline bool TAtomicTraits<4>::isLockFree() { return false; }
#else
  template <>
  inline bool TAtomicTraits<4>::isLockFree() { return false; }
#endif

template <>
struct TAtomicOperationsSupport<1> { enum { bSupported = 1 }; };
#if ATOMIC_CHAR_LOCK_FREE == 2
  //> x86/x64 support, but arm instruction not support.
  template <>
  inline bool TAtomicTraits<1>::isLockFree() { return true; }
#elif ATOMIC_CHAR_LOCK_FREE == 1
  template <>
  inline bool TAtomicTraits<1>::isLockFree() { return false; }
#else
  template <>
  inline bool TAtomicTraits<1>::isLockFree() { return false; }
#endif

template <>
struct TAtomicOperationsSupport<2> { enum { bSupported = 1 }; };
#if ATOMIC_SHORT_LOCK_FREE == 2

  template <>
  inline bool TAtomicTraits<2>::isLockFree() { return true; }
#elif ATOMIC_SHORT_LOCK_FREE == 1
  template <>
  inline bool TAtomicTraits<2>::isLockFree() { return false; }
#else
  template <>
  inline bool TAtomicTraits<2>::isLockFree() { return false; }
#endif


template <>
struct TAtomicOperationsSupport<8> { enum { bSupported = 1 }; };
#if ATOMIC_LLONG_LOCK_FREE == 2
  template <>
  inline bool TAtomicTraits<8>::isLockFree() { return true; }
#elif ATOMIC_LLONG_LOCK_FREE == 1
  template <>
  inline bool TAtomicTraits<8>::isLockFree() { return false; }
#else
  template <>
  inline bool TAtomicTraits<8>::isLockFree() { return false; }
#endif

// clang-format on

/**
 * atomic operations, there are two version of operations, notice
 * the parameter with `volatile`.
 */
template <typename T>
struct TAtomicOperations {
  typedef std::atomic<T> ValueType;

  template <typename Ty>
  static inline Ty load(const std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty load(const volatile std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty loadRelaxed(const std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty loadRelaxed(const volatile std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty loadAcquire(const std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty loadAcquire(const volatile std::atomic<Ty>& v) noexcept {
    return v.load(std::memory_order_acquire);
  }

  template <typename Ty>
  static inline void store(std::atomic<Ty>& v, Ty new_value) noexcept {
    v.store(new_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline void storeRelaxed(std::atomic<Ty>& v, Ty new_value) noexcept {
    v.store(new_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline void storeRelease(std::atomic<Ty>& v, Ty new_value) noexcept {
    v.store(new_value, std::memory_order_release);
  }

  /**
   * The ref and deRef methods are used for reference counting.
   */

  template <typename Ty>
  static inline bool ref(std::atomic<Ty>& v) {
    /* Conceptually, we want to
     *    return ++v != 0;
     * However, that would be sequentially consistent, and thus stronger
     * than what we need. Based on
     * http://eel.is/c++draft/atomics.types.memop#6, we know that
     * >>> the `fetch_add` return original value, but variable has already inc.
     * <<< pre-increment is equivalent to fetch_add(1) + 1. Unlike
     * pre-increment, fetch_add takes a memory order argument, so we can get the
     * desired acquire-release semantics. One last gotcha is that fetch_add(1) +
     * 1 would need to be converted back to T, because it's susceptible to
     * integer promotion. To sidestep this issue and to avoid UB on signed
     * overflow, we rewrite the expression to:
     */
    return v.fetch_add(1, std::memory_order_acq_rel) != Ty(-1);
  }

  template <typename Ty>
  static inline bool deRef(std::atomic<Ty>& v) noexcept {
    return v.fetch_sub(1, std::memory_order_acq_rel) != Ty(1);
  }

  template <typename Ty>
  static Ty fetchStoreRelaxed(std::atomic<Ty>& v, Ty new_value) noexcept {
    return v.exchange(new_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static Ty fetchStoreAcquire(std::atomic<Ty>& v, Ty new_value) noexcept {
    return v.exchange(new_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static Ty fetchStoreRelease(std::atomic<Ty>& v, Ty new_value) noexcept {
    return v.exchange(new_value, std::memory_order_release);
  }

  template <typename Ty>
  static Ty fetchStoreOrdered(std::atomic<Ty>& v, Ty new_value) noexcept {
    return v.exchange(new_value, std::memory_order_acq_rel);
  }

  template <typename Ty>
  static inline Ty fetchAddRelaxed(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_add(add_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty fetchAddAcquire(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_add(add_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty fetchAddRelease(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_add(add_value, std::memory_order_release);
  }

  template <typename Ty>
  static inline Ty fetchAddOrdered(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_add(add_value, std::memory_order_acq_rel);
  }

  template <typename Ty>
  static inline Ty fetchSubRelaxed(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_sub(add_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty fetchSubAcquire(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_sub(add_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty fetchSubRelease(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_sub(add_value, std::memory_order_release);
  }

  template <typename Ty>
  static inline Ty fetchSubOrdered(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_sub(add_value, std::memory_order_acq_rel);
  }

  template <typename Ty>
  static inline Ty fetchAndRelaxed(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_and(add_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty fetchAndAcquire(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_and(add_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty fetchAndRelease(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_and(add_value, std::memory_order_release);
  }

  template <typename Ty>
  static inline Ty fetchAndOrdered(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_and(add_value, std::memory_order_acq_rel);
  }

  template <typename Ty>
  static inline Ty fetchOrRelaxed(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_or(add_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty fetchOrAcquire(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_or(add_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty fetchOrRelease(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_or(add_value, std::memory_order_release);
  }

  template <typename Ty>
  static inline Ty fetchOrOrdered(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_or(add_value, std::memory_order_acq_rel);
  }

  template <typename Ty>
  static inline Ty fetchXorRelaxed(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_xor(add_value, std::memory_order_relaxed);
  }

  template <typename Ty>
  static inline Ty fetchXorAcquire(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_xor(add_value, std::memory_order_acquire);
  }

  template <typename Ty>
  static inline Ty fetchXorRelease(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_xor(add_value, std::memory_order_release);
  }

  template <typename Ty>
  static inline Ty fetchXorOrdered(
      std::atomic<Ty>& v,
      typename TAtomicAdditiveType<Ty>::AdditiveType add_value) noexcept {
    return v.fetch_xor(add_value, std::memory_order_acq_rel);
  }

  static inline bool isTestAndSetNative() noexcept {
    return TAtomicTraits<sizeof(T)>::isLockFree();
  }
  static inline constexpr bool isTestAndSetWaitFree() noexcept { return false; }

  template <typename Ty>
  static bool testAndSetRelaxed(std::atomic<Ty>& value, Ty expected_value, Ty new_value,
                                Ty* current_value = nullptr) noexcept {
    bool t = value.compare_exchange_strong(
        expected_value, new_value, std::memory_order_relaxed, std::memory_order_relaxed);
    if (current_value) {
      *current_value = expected_value;
    }
    return t;
  }

  template <typename Ty>
  static bool testAndSetAcquire(std::atomic<Ty>& value, Ty expected_value, Ty new_value,
                                Ty* current_value = nullptr) noexcept {
    bool t = value.compare_exchange_strong(
        expected_value, new_value, std::memory_order_acquire, std::memory_order_acquire);
    if (current_value) {
      *current_value = expected_value;
    }
    return t;
  }

  template <typename Ty>
  static bool testAndSetRelease(std::atomic<Ty>& value, Ty expected_value, Ty new_value,
                                Ty* current_value = nullptr) noexcept {
    bool t = value.compare_exchange_strong(
        expected_value, new_value, std::memory_order_release, std::memory_order_relaxed);
    if (current_value) {
      *current_value = expected_value;
    }
    return t;
  }

  template <typename Ty>
  static bool testAndSetOrdered(std::atomic<Ty>& value, Ty expected_value, Ty new_value,
                                Ty* current_value = nullptr) noexcept {
    bool t = value.compare_exchange_strong(
        expected_value, new_value, std::memory_order_acq_rel, std::memory_order_acquire);
    if (current_value) {
      *current_value = expected_value;
    }
    return t;
  }

  static inline bool isReferenceCountingNative() noexcept { return isTestAndSetNative(); }
  static inline constexpr bool isReferenceCountingWaitFree() noexcept { return false; }

  static inline bool isFetchStoreNative() noexcept { return isTestAndSetNative(); }
  static inline constexpr bool isFetchStoreWaitFree() noexcept { return false; }
  static inline bool isFetchAddNative() noexcept { return isTestAndSetNative(); }
  static inline constexpr bool isFetchAddWaitFree() noexcept { return false; }
};

}  // namespace amaryl
