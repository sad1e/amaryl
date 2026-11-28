#pragma once

#include <limits>
#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/editor/model/signal.h"
#include "amaryl/editor/model/transaction.h"

namespace amaryl::editor {

enum class EHistoryEvent : uint8 {
  Do,
  Undo,
  Redo,
  // The last entry absorbed a transaction with the same merge key.
  Merge,
};

// The undo stack and the single entry point of data changes. Listeners get every change as
// the applied transaction, views do not know do from undo.
struct EDITOR_API History final {
  static constexpr size_t kDefaultCapacity = 256;

  explicit History(ObjectRegistry& objects, size_t capacity = kDefaultCapacity);
  ~History();

  NO_COPYABLE_AND_MOVEABLE(History);

  // Applies, then records. When a command fails, the applied commands are rolled back and
  // nothing is recorded.
  bool execute(Transaction transaction);
  // Records a transaction that is already applied.
  void record(Transaction transaction);

  bool canUndo() const { return applied_ > 0; }
  bool canRedo() const { return applied_ < entries_.size(); }
  bool undo();
  bool redo();
  void jumpTo(size_t applied_count);
  void clear();

  Span<const Transaction> getEntries() const { return entries_; }
  size_t getAppliedCount() const { return applied_; }
  bool isDirty() const { return clean_ != applied_; }
  void markClean() { clean_ = applied_; }

  ObjectRegistry& getObjects() const { return objects_; }

  // Undo gives the inverted transaction.
  Signal<const Transaction&, EHistoryEvent> onChanged;

 private:
  static constexpr size_t kNoCleanState = std::numeric_limits<size_t>::max();

  bool apply(const Transaction& transaction);
  bool applyCommand(const Command& command);
  bool tryMerge(const Transaction& transaction);
  void push(Transaction transaction);

  ObjectRegistry& objects_;
  std::vector<Transaction> entries_;
  size_t applied_{0};
  size_t clean_{0};
  size_t capacity_;
};

}  // namespace amaryl::editor
