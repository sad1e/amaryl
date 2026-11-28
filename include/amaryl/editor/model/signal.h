#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "amaryl/core/types.h"

namespace amaryl::editor {

// Synchronous multicast callback. Slots connected or disconnected during emit() take
// effect at the next emit().
template <typename... Args>
struct Signal final {
  using Slot = std::function<void(Args...)>;

  uint32 connect(Slot slot) {
    slots_.push_back({++last_id_, std::move(slot)});
    return last_id_;
  }

  void disconnect(uint32 id) {
    std::erase_if(slots_, [id](const Entry& entry) { return entry.id == id; });
  }

  void emit(Args... args) const {
    const std::vector<Entry> slots = slots_;
    for (const Entry& entry : slots) {
      entry.slot(args...);
    }
  }

 private:
  struct Entry {
    uint32 id{0};
    Slot slot;
  };

  std::vector<Entry> slots_;
  uint32 last_id_{0};
};

}  // namespace amaryl::editor
