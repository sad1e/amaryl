#pragma once

#include <vector>

#include "amaryl/core/handle.h"
#include "amaryl/core/traits.h"

namespace amaryl {

template <typename HandleTag, typename ObjectType, typename IndexType = uint32>
struct Pool {
  static_assert(is_same_v<IndexType, uint16> || is_same_v<IndexType, uint32>,
                "Unsupported IndexType");

 private:
  using HandleType = Handle<HandleTag, IndexType>;
  enum : IndexType {
    ListEndSentinel =
        static_cast<IndexType>(is_same_v<IndexType, uint16> ? 0xffff : 0xffffffff)
  };

  struct MetaData {
    IndexType nextFree = ListEndSentinel;
    IndexType gen = 1;
  };

  IndexType free_list_head_ = ListEndSentinel;
  IndexType num_objects_ = 0;

  std::vector<MetaData> meta_datas_;
  std::vector<ObjectType> objects_;

 public:
  Pool() = default;

  explicit Pool(IndexType numSlotsToReserve) {
    meta_datas_.reserve(numSlotsToReserve);
    objects_.reserve(numSlotsToReserve);
  }

  // copying a Pool would duplicate all objects while existing Handles keep aliasing both
  // copies - disallow it
  NO_COPYABLE(Pool);

  Pool(Pool&&) = default;
  Pool& operator=(Pool&&) = default;

  HandleType create(ObjectType&& obj) {
    IndexType idx = 0;

    if (free_list_head_ != ListEndSentinel) {
      idx = free_list_head_;
      free_list_head_ = meta_datas_[idx].nextFree;
      objects_[idx] = std::move(obj);
    } else {
      assert(objects_.size() < ListEndSentinel);

      idx = static_cast<IndexType>(objects_.size());
      objects_.emplace_back(std::move(obj));
      meta_datas_.push_back({});
    }

    ++num_objects_;

    return HandleType(idx, meta_datas_[idx].gen);
  }

  // PERFORMANCE NOTE: all generation checks below are asserts only - in release builds an
  // out-of-date Handle (one whose slot has since been destroyed and possibly reused) is
  // not detected and dereferencing it returns a pointer to whatever object now occupies
  // that slot. Callers must not retain Handles past the lifetime of the object they refer
  // to.
  void destroy(HandleType handle) {
    if (handle.empty()) {
      return;
    }

    assert(num_objects_ > 0);
    const IndexType index = handle.index();
    assert(index < objects_.size());
    assert(handle.gen() == meta_datas_[index].gen);

    objects_[index] = ObjectType{};
    ++meta_datas_[index].gen;

    if (!meta_datas_[index].gen) {
      meta_datas_[index].gen = 1;
    }
    meta_datas_[index].nextFree = free_list_head_;
    free_list_head_ = index;
    --num_objects_;
  }

  const ObjectType* get(HandleType handle) const {
    if (handle.empty()) {
      return nullptr;
    }

    const IndexType index = handle.index();
    assert(index < objects_.size());
    assert(handle.gen() == meta_datas_[index].gen);

    return &objects_[index];
  }

  // delegate to the const overload to avoid duplicating the lookup; const_cast is safe
  // because *this is non-const here
  FORCE_INLINE ObjectType* get(HandleType handle) {
    return const_cast<ObjectType*>(static_cast<const Pool*>(this)->get(handle));
  }

  // Returns the first storage slot. This is not necessarily a live object: a
  // destroyed first slot contains ObjectType{} until it is reused.
  ObjectType& front() {
    assert(!objects_.empty());
    return objects_.front();
  }

  const ObjectType& front() const {
    assert(!objects_.empty());
    return objects_.front();
  }

  // PERFORMANCE NOTE: this builds a handle from any in-range index without checking
  // whether the slot is alive, so it can return a valid-looking handle to a freed or
  // never-initialized slot (the handle carries the slot's current generation and will
  // pass get()'s assert). Callers must only pass indices known to refer to live objects.
  HandleType getHandle(IndexType index) const {
    assert(index < objects_.size());
    if (index >= objects_.size()) {
      return {};
    }

    return HandleType(index, meta_datas_[index].gen);
  }

  // PERFORMANCE NOTE: this scans every slot, including freed ones (destroyed slots are
  // reset to ObjectType{} but stay in objects_). A search for a default-constructed
  // object can therefore match a freed slot and return a handle to it. Callers must only
  // search for objects known to be alive
  HandleType findObject(const ObjectType* obj) {
    if (!obj) {
      return {};
    }

    for (IndexType idx = 0; idx != objects_.size(); ++idx) {
      if (objects_[idx] == *obj) {
        return HandleType(idx, meta_datas_[idx].gen);
      }
    }

    return {};
  }

  void clear() {
    objects_.clear();
    meta_datas_.clear();
    free_list_head_ = ListEndSentinel;
    num_objects_ = 0;
  }

  template <typename TFunc>
  void for_each(TFunc&& fn) const {
    for (const ObjectType& object : objects_) {
      std::invoke(fn, object);
    }
  }

  // The loop will be broke when the return true.
  template <typename TFunc>
  void for_each_until(TFunc&& fn) const {
    for (const ObjectType& object : objects_) {
      if (std::invoke(fn, object)) {
        break;
      }
    }
  }

  // Return the `num_objects_`
  uint32 getNumObjects() const { return num_objects_; }

  // Return the size of objects array directly
  uint32 getSizeOfObjects() const { return objects_.size(); }
};

}  // namespace amaryl
