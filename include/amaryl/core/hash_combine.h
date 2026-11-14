#pragma once

#include <functional>

#include "amaryl/core/config.h"
#include "amaryl/core/macros.h"
#include "amaryl/core/types.h"

namespace amaryl {

/// Implements the FNV-1a hash algorithm
/// @see
/// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
/// @param inData Data block of bytes
/// @param inSize Number of bytes
/// @param inSeed Seed of the hash (can be used to pass in the hash of a
/// previous operation, otherwise leave default)
/// @return Hash
inline uint64 hashBytes(const void* inData, uint32 inSize,
                        uint64 inSeed = 0xcbf29ce484222325UL) {
  uint64 hash = inSeed;
  for (const uint8* data = reinterpret_cast<const uint8*>(inData);
       data < reinterpret_cast<const uint8*>(inData) + inSize; ++data) {
    hash = hash ^ uint64(*data);
    hash = hash * 0x100000001b3UL;
  }
  return hash;
}

/// A 64 bit hash function by Thomas Wang, Jan 1997
/// See:
/// http://web.archive.org/web/20071223173210/http://www.concentric.net/~Ttwang/tech/inthash.htm
/// @param inValue Value to hash
/// @return Hash
inline uint64 hash64(uint64 inValue) {
  uint64 hash = inValue;
  hash = (~hash) + (hash << 21);  // hash = (hash << 21) - hash - 1;
  hash = hash ^ (hash >> 24);
  hash = (hash + (hash << 3)) + (hash << 8);  // hash * 265
  hash = hash ^ (hash >> 14);
  hash = (hash + (hash << 2)) + (hash << 4);  // hash * 21
  hash = hash ^ (hash >> 28);
  hash = hash + (hash << 31);
  return hash;
}

/// @brief Helper function that hashes a single value into ioSeed
/// Taken from:
/// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template <typename T>
inline void hashCombineHelper(size_t& ioSeed, const T& inValue) {
  std::hash<T> hasher;
  ioSeed ^= hasher(inValue) + 0x9e3779b9 + (ioSeed << 6) + (ioSeed >> 2);
}

/// Hash combiner to use a custom struct in an unordered map or set
///
/// Usage:
///
///		struct SomeHashKey
///		{
///		    std::string key1;
///		    std::string key2;
///		    bool key3;
///		};
///
///		AMARYL_MAKE_HASHABLE(SomeHashKey, t.key1, t.key2, t.key3)
template <typename... Values>
inline void hashCombine(std::size_t& ioSeed, Values... inValues) {
  // Hash all values together using a fold expression
  (hashCombineHelper(ioSeed, inValues), ...);
}

}  // namespace amaryl

#define AMARYL_MAKE_HASH_STRUCT(type, name, ...)  \
  struct [[nodiscard]] name {                     \
    std::size_t operator()(const type& t) const { \
      std::size_t ret = 0;                        \
      ::amaryl::hashCombine(ret, __VA_ARGS__);    \
      return ret;                                 \
    }                                             \
  };

#define AMARYL_MAKE_HASHABLE(type, ...)                  \
  namespace std {                                        \
  template <>                                            \
  AMARYL_MAKE_HASH_STRUCT(type, hash<type>, __VA_ARGS__) \
  }
