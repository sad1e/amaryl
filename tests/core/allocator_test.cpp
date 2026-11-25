#include "amaryl/core/allocator/allocator.h"

#include <cstdint>
#include <vector>

#include "amaryl/core/allocator/arena_allocator.h"
#include "amaryl/core/allocator/bump_allocator.h"
#include "amaryl/core/allocator/pool_allocator.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

bool IsAlignedPtr(const void* ptr, size_t alignment) {
  return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

}  // namespace

int main() {
  "mimalloc allocator basics"_test = [] {
    MimallocAllocator allocator;

    void* p = allocator.Allocate(64);
    expect(p != nullptr);
    expect(IsAlignedPtr(p, kDefaultAlignment));

    void* aligned = allocator.Allocate(64, 64);
    expect(IsAlignedPtr(aligned, 64));

    int* grown = static_cast<int*>(allocator.Reallocate(p, 64, 128));
    expect(grown != nullptr);
    grown[0] = 42;

    allocator.Deallocate(grown, 128);
    allocator.Deallocate(aligned, 64);
  };

  "default allocator"_test = [] {
    IAllocator& allocator = DefaultAllocator();
    void* p = allocator.Allocate(32);
    expect(p != nullptr);
    allocator.Deallocate(p, 32);
  };

  "bump allocator alignment and reset"_test = [] {
    BumpAllocator allocator(256);

    void* a = allocator.Allocate(8, 8);
    void* b = allocator.Allocate(8, 16);
    void* c = allocator.Allocate(8, 32);
    expect(a != nullptr && b != nullptr && c != nullptr);
    expect(IsAlignedPtr(a, 8));
    expect(IsAlignedPtr(b, 16));
    expect(IsAlignedPtr(c, 32));

    const size_t used = allocator.Used();
    expect(used >= 24u);  // three 8-byte allocations (plus alignment padding)
    expect(allocator.Capacity() == 256u);

    allocator.Reset();
    expect(allocator.Used() == 0u);
  };

  "bump allocator exhausts buffer"_test = [] {
    BumpAllocator allocator(32);
    void* p = allocator.Allocate(32);
    expect(p != nullptr);
    expect(allocator.Allocate(1) == nullptr);
  };

  "arena allocator grows across blocks"_test = [] {
    ArenaAllocator allocator(64);

    void* a = allocator.Allocate(48);
    expect(a != nullptr);
    expect(IsAlignedPtr(a, kDefaultAlignment));

    // This does not fit in the remaining space of the first block and forces a
    // second block.
    void* b = allocator.Allocate(48, 32);
    expect(b != nullptr);
    expect(IsAlignedPtr(b, 32));

    expect(allocator.Capacity() >= 128u);
    expect(allocator.Used() >= 96u);

    allocator.Reset();
    expect(allocator.Used() == 0u);
    expect(allocator.Capacity() == 0u);
  };

  "pool allocator recycles slots"_test = [] {
    PoolAllocator allocator(32, 4);

    void* a = allocator.Allocate(32);
    void* b = allocator.Allocate(32);
    void* c = allocator.Allocate(32);
    void* d = allocator.Allocate(32);  // last slot of the first block
    expect(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
    expect(IsAlignedPtr(a, kDefaultAlignment));
    expect(allocator.LiveAllocations() == 4u);

    allocator.Deallocate(b, 32);
    expect(allocator.LiveAllocations() == 3u);

    // The freed slot must be recycled before a new block is grown.
    void* e = allocator.Allocate(32);
    expect(e == b);
    expect(allocator.LiveAllocations() == 4u);

    allocator.Deallocate(a, 32);
    allocator.Deallocate(c, 32);
    allocator.Deallocate(d, 32);
    allocator.Deallocate(e, 32);
    expect(allocator.LiveAllocations() == 0u);

    allocator.Reset();
  };

  "stl container over arena allocator"_test = [] {
    ArenaAllocator arena(256);
    StlAllocator<int, ArenaAllocator> stl_allocator(arena);

    std::vector<int, StlAllocator<int, ArenaAllocator>> values(stl_allocator);
    values.reserve(16);
    for (int i = 0; i < 16; ++i) {
      values.push_back(i);
    }

    expect(values.size() == 16u);
    for (int i = 0; i < 16; ++i) {
      expect(values[static_cast<size_t>(i)] == i);
    }
  };

  "stl container over bump allocator"_test = [] {
    BumpAllocator bump(32);
    StlAllocator<int, BumpAllocator> stl_allocator(bump);

    std::vector<int, StlAllocator<int, BumpAllocator>> values(stl_allocator);
  };

  return 0;
}
