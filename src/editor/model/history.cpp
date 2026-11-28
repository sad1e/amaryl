#include "amaryl/editor/model/history.h"

#include <algorithm>

namespace amaryl::editor {

History::History(ObjectRegistry& objects, size_t capacity)
    : objects_(objects), capacity_(std::max<size_t>(capacity, 1)) {}

History::~History() = default;

bool History::execute(Transaction transaction) {
  if (transaction.empty() || !apply(transaction)) {
    return false;
  }
  record(std::move(transaction));
  return true;
}

void History::record(Transaction transaction) {
  if (transaction.empty()) {
    return;
  }
  if (tryMerge(transaction)) {
    onChanged.emit(transaction, EHistoryEvent::Merge);
    return;
  }
  push(transaction);
  onChanged.emit(transaction, EHistoryEvent::Do);
}

bool History::undo() {
  if (!canUndo()) {
    return false;
  }
  --applied_;
  const Transaction inverse = entries_[applied_].inverted();
  if (!apply(inverse)) {
    ALOGW("Cannot undo '%s', its objects do not exist", inverse.description.c_str());
    return false;
  }
  onChanged.emit(inverse, EHistoryEvent::Undo);
  return true;
}

bool History::redo() {
  if (!canRedo()) {
    return false;
  }
  const Transaction transaction = entries_[applied_];
  ++applied_;
  if (!apply(transaction)) {
    ALOGW("Cannot redo '%s', its objects do not exist", transaction.description.c_str());
    return false;
  }
  onChanged.emit(transaction, EHistoryEvent::Redo);
  return true;
}

void History::jumpTo(size_t applied_count) {
  applied_count = std::min(applied_count, entries_.size());
  while (applied_ > applied_count) {
    undo();
  }
  while (applied_ < applied_count) {
    redo();
  }
}

void History::clear() {
  clean_ = isDirty() ? kNoCleanState : 0;
  entries_.clear();
  applied_ = 0;
}

bool History::apply(const Transaction& transaction) {
  for (size_t i = 0; i < transaction.commands.size(); ++i) {
    if (!applyCommand(transaction.commands[i])) {
      for (size_t j = i; j-- > 0;) {
        applyCommand(transaction.commands[j].inverted());
      }
      return false;
    }
  }
  return true;
}

bool History::applyCommand(const Command& command) {
  const TypeDesc* type = objects_.getType(command.object);
  if (!type) {
    return false;
  }
  switch (command.kind) {
    case ECommandKind::SetProperty: {
      const NativeHandle native = objects_.getNative(command.object);
      const PropertyDesc* property = type->findProperty(command.property);
      return property && objects_.isAlive(command.object) && property->canWrite(native) &&
             property->set(native, command.after);
    }
    case ECommandKind::CreateObject: {
      if (!type->supportsStructuralEdits() || !command.state ||
          objects_.isAlive(command.object)) {
        return false;
      }
      const NativeHandle native = type->restore(*command.state);
      if (native == 0) {
        return false;
      }
      objects_.rebind(command.object, native);
      return true;
    }
    case ECommandKind::DestroyObject: {
      if (!type->supportsStructuralEdits() || !objects_.isAlive(command.object)) {
        return false;
      }
      type->destroy(objects_.getNative(command.object));
      objects_.unbind(command.object);
      return true;
    }
  }
  return false;
}

bool History::tryMerge(const Transaction& transaction) {
  if (transaction.merge_key == 0 || applied_ == 0 || applied_ != entries_.size() ||
      entries_.back().merge_key != transaction.merge_key) {
    return false;
  }
  for (const Command& command : transaction.commands) {
    if (command.kind != ECommandKind::SetProperty) {
      return false;
    }
  }

  Transaction& last = entries_.back();
  for (const Command& command : transaction.commands) {
    const auto it = std::find_if(last.commands.begin(), last.commands.end(),
      [&command](const Command& existing) {
        return existing.kind == ECommandKind::SetProperty &&
               existing.object == command.object && existing.property == command.property;
      });
    if (it != last.commands.end()) {
      it->after = command.after;
    } else {
      last.commands.push_back(command);
    }
  }
  if (clean_ == applied_) {
    clean_ = kNoCleanState;
  }
  return true;
}

void History::push(Transaction transaction) {
  // The redo entries are discarded, the saved state can be one of them.
  entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(applied_), entries_.end());
  if (clean_ != kNoCleanState && clean_ > applied_) {
    clean_ = kNoCleanState;
  }
  entries_.push_back(std::move(transaction));
  if (entries_.size() > capacity_) {
    entries_.erase(entries_.begin());
    clean_ = (clean_ == kNoCleanState || clean_ == 0) ? kNoCleanState : clean_ - 1;
  }
  applied_ = entries_.size();
}

}  // namespace amaryl::editor
