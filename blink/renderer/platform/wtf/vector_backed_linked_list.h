// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListReverseIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstReverseIterator;

template <typename ValueType>
class VectorBackedLinkedListNode {
  USING_FAST_MALLOC(VectorBackedLinkedListNode);

 public:
  VectorBackedLinkedListNode() = delete;

  VectorBackedLinkedListNode(wtf_size_t prev_index, wtf_size_t next_index)
      : prev_index_(prev_index), next_index_(next_index) {}

  VectorBackedLinkedListNode(wtf_size_t prev_index,
                             wtf_size_t next_index,
                             const ValueType& value)
      : prev_index_(prev_index), next_index_(next_index), value_(value) {}

  VectorBackedLinkedListNode(wtf_size_t prev_index,
                             wtf_size_t next_index,
                             ValueType&& value)
      : prev_index_(prev_index),
        next_index_(next_index),
        value_(std::move(value)) {}

  VectorBackedLinkedListNode(const VectorBackedLinkedListNode& other) = delete;

  VectorBackedLinkedListNode(VectorBackedLinkedListNode&& other) = default;

  VectorBackedLinkedListNode& operator=(
      const VectorBackedLinkedListNode& other) = delete;

  VectorBackedLinkedListNode& operator=(VectorBackedLinkedListNode&& other) =
      default;

  wtf_size_t prev_index_ = kNotFound;
  wtf_size_t next_index_ = kNotFound;
  ValueType value_ = HashTraits<ValueType>::EmptyValue();
};

// VectorBackedLinkedList maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
// The linked list is implementing in a vector (with links being indexes instead
// of pointers), to simplify the move of backing during GC compaction.
template <typename ValueType>
class VectorBackedLinkedList {
  USING_FAST_MALLOC(VectorBackedLinkedList);

 private:
  using Node = VectorBackedLinkedListNode<ValueType>;

 public:
  using Value = ValueType;
  using iterator = VectorBackedLinkedListIterator<VectorBackedLinkedList>;
  using const_iterator =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedList>;
  friend class VectorBackedLinkedListConstIterator<VectorBackedLinkedList>;
  using reverse_iterator =
      VectorBackedLinkedListReverseIterator<VectorBackedLinkedList>;
  using const_reverse_iterator =
      VectorBackedLinkedListConstReverseIterator<VectorBackedLinkedList>;

  VectorBackedLinkedList();

  // TODO(keinakashima): implement copy constructor & copy assignment operator
  VectorBackedLinkedList(VectorBackedLinkedList&&);
  VectorBackedLinkedList& operator=(VectorBackedLinkedList&&);

  ~VectorBackedLinkedList() = default;

  void swap(VectorBackedLinkedList&);

  bool empty() { return size_ == 0; }
  wtf_size_t size() const { return size_; }

  iterator begin() { return MakeIterator(UsedFirstIndex()); }
  iterator end() { return MakeIterator(anchor_index_); }
  const_iterator cbegin() const { return MakeConstIterator(UsedFirstIndex()); }
  const_iterator cend() const { return MakeConstIterator(anchor_index_); }
  reverse_iterator rbegin() { return MakeReverseIterator(UsedLastIndex()); }
  reverse_iterator rend() { return MakeReverseIterator(anchor_index_); }
  const_reverse_iterator crbegin() const {
    return MakeConstReverseIterator(UsedLastIndex());
  }
  const_reverse_iterator crend() const {
    return MakeConstReverseIterator(anchor_index_);
  }

  Value& front() { return nodes_[UsedFirstIndex()].value_; }
  const Value& front() const { return nodes_[UsedFirstIndex()].value_; }
  Value& back() { return nodes_[UsedLastIndex()].value_; }
  const Value& back() const { return nodes_[UsedLastIndex()].value_; }

  template <typename IncomingValueType>
  iterator insert(const_iterator position, IncomingValueType&& value);

  template <typename IncomingValueType>
  void push_front(IncomingValueType&& value) {
    insert(cbegin(), std::forward<IncomingValueType>(value));
  }

  template <typename IncomingValueType>
  void push_back(IncomingValueType&& value) {
    insert(cend(), std::forward<IncomingValueType>(value));
  }

  // Moves |target| right before |new_position| in a linked list. This operation
  // is executed by just updating indices of related nodes.
  void MoveTo(const_iterator target, const_iterator new_position);

  iterator erase(const_iterator);

  void pop_front() {
    DCHECK(!empty());
    erase(cbegin());
  }
  void pop_back() {
    DCHECK(!empty());
    erase(--cend());
  }

 private:
  bool IsFreeListEmpty() const { return free_head_index_ == anchor_index_; }

  wtf_size_t UsedFirstIndex() const {
    return nodes_[anchor_index_].next_index_;
  }
  wtf_size_t UsedLastIndex() const { return nodes_[anchor_index_].prev_index_; }

  iterator MakeIterator(wtf_size_t index) { return iterator(index, this); }
  const_iterator MakeConstIterator(wtf_size_t index) const {
    return const_iterator(index, this);
  }
  reverse_iterator MakeReverseIterator(wtf_size_t index) {
    return reverse_iterator(index, this);
  }
  const_reverse_iterator MakeConstReverseIterator(wtf_size_t index) const {
    return const_reverse_iterator(index, this);
  }

  bool IsIndexValid(wtf_size_t index) const {
    return 0 <= index && index < nodes_.size();
  }

  bool IsAnchor(wtf_size_t index) const { return index == anchor_index_; }

  void Unlink(const Node&);

  Vector<Node> nodes_;
  static constexpr wtf_size_t anchor_index_ = 0;
  // Anchor is not included in the free list, but it serves as the list's
  // terminator.
  wtf_size_t free_head_index_ = anchor_index_;
  wtf_size_t size_ = 0;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListIterator {
  DISALLOW_NEW();
  using ReferenceType = typename VectorBackedLinkedListType::Value&;
  using PointerType = typename VectorBackedLinkedListType::Value*;
  using const_iterator =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedListType>;

 public:
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  VectorBackedLinkedListIterator& operator++() {
    ++iterator_;
    return *this;
  }

  VectorBackedLinkedListIterator& operator--() {
    --iterator_;
    return *this;
  }

  VectorBackedLinkedListIterator& operator++(int) = delete;
  VectorBackedLinkedListIterator& operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListIterator& other) {
    return iterator_ == other.iterator_;
  }

  bool operator!=(const VectorBackedLinkedListIterator& other) {
    return !(*this == other);
  }

  operator const_iterator() const { return iterator_; }

 private:
  VectorBackedLinkedListIterator(wtf_size_t index,
                                 VectorBackedLinkedListType* container)
      : iterator_(index, container) {}

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  wtf_size_t GetIndex() const { return iterator_.GetIndex(); }

  const_iterator iterator_;

  template <typename T>
  friend class VectorBackedLinkedList;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstIterator {
  DISALLOW_NEW();
  using ReferenceType = const typename VectorBackedLinkedListType::Value&;
  using PointerType = const typename VectorBackedLinkedListType::Value*;
  using Node = typename VectorBackedLinkedListType::Node;

 public:
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  VectorBackedLinkedListConstIterator& operator++() {
    DCHECK(container_->IsIndexValid(index_));
    index_ = container_->nodes_[index_].next_index_;
    DCHECK(container_->IsIndexValid(index_));
    return *this;
  }

  VectorBackedLinkedListConstIterator& operator--() {
    DCHECK(container_->IsIndexValid(index_));
    index_ = container_->nodes_[index_].prev_index_;
    DCHECK(container_->IsIndexValid(index_));
    return *this;
  }

  VectorBackedLinkedListConstIterator operator++(int) = delete;
  VectorBackedLinkedListConstIterator operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListConstIterator& other) {
    DCHECK_EQ(container_, other.container_);
    return index_ == other.index_ && container_ == other.container_;
  }

  bool operator!=(const VectorBackedLinkedListConstIterator& other) {
    return !(*this == other);
  }

 protected:
  PointerType Get() const {
    DCHECK(container_->IsIndexValid(index_));
    DCHECK(!container_->IsAnchor(index_));
    const Node& node = container_->nodes_[index_];
    return &node.value_;
  }

  VectorBackedLinkedListConstIterator(
      wtf_size_t index,
      const VectorBackedLinkedListType* container)
      : index_(index), container_(container) {}

  wtf_size_t GetIndex() const { return index_; }

 private:
  wtf_size_t index_;
  const VectorBackedLinkedListType* container_;

  template <typename T>
  friend class VectorBackedLinkedList;
  friend class VectorBackedLinkedListIterator<VectorBackedLinkedListType>;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListReverseIterator {
  using ReferenceType = typename VectorBackedLinkedListType::Value&;
  using PointerType = typename VectorBackedLinkedListType::Value*;
  using const_reverse_iterator =
      VectorBackedLinkedListConstReverseIterator<VectorBackedLinkedListType>;

 public:
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  VectorBackedLinkedListReverseIterator& operator++() {
    ++iterator_;
    return *this;
  }

  VectorBackedLinkedListReverseIterator& operator--() {
    --iterator_;
    return *this;
  }

  VectorBackedLinkedListReverseIterator& operator++(int) = delete;
  VectorBackedLinkedListReverseIterator& operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListReverseIterator& other) {
    return iterator_ == other.iterator_;
  }

  bool operator!=(const VectorBackedLinkedListReverseIterator& other) {
    return !(*this == other);
  }

  operator const_reverse_iterator() const { return iterator_; }

 private:
  VectorBackedLinkedListReverseIterator(wtf_size_t index,
                                        VectorBackedLinkedListType* container)
      : iterator_(index, container) {}

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  wtf_size_t GetIndex() const { return iterator_.GetIndex(); }

  const_reverse_iterator iterator_;

  template <typename T>
  friend class VectorBackedLinkedList;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstReverseIterator
    : public VectorBackedLinkedListConstIterator<VectorBackedLinkedListType> {
  using Superclass =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedListType>;

 public:
  VectorBackedLinkedListConstReverseIterator& operator++() {
    Superclass::operator--();
    return *this;
  }

  VectorBackedLinkedListConstReverseIterator& operator--() {
    Superclass::operator++();
    return *this;
  }

  VectorBackedLinkedListConstReverseIterator operator++(int) = delete;
  VectorBackedLinkedListConstReverseIterator operator--(int) = delete;

 private:
  VectorBackedLinkedListConstReverseIterator(
      wtf_size_t index,
      const VectorBackedLinkedListType* container)
      : Superclass(index, container) {}

  template <typename T>
  friend class VectorBackedLinkedList;
  friend class VectorBackedLinkedListReverseIterator<
      VectorBackedLinkedListType>;
};

template <typename T>
VectorBackedLinkedList<T>::VectorBackedLinkedList() {
  // First inserts anchor, which serves as the beginning and the end of
  // the used list.
  nodes_.push_back(Node(anchor_index_, anchor_index_));
}

template <typename T>
inline VectorBackedLinkedList<T>::VectorBackedLinkedList(
    VectorBackedLinkedList&& other) {
  swap(other);
}

template <typename T>
inline VectorBackedLinkedList<T>& VectorBackedLinkedList<T>::operator=(
    VectorBackedLinkedList&& other) {
  swap(other);
  return *this;
}

template <typename T>
inline void VectorBackedLinkedList<T>::swap(VectorBackedLinkedList& other) {
  nodes_.swap(other.nodes_);
  swap(free_head_index_, other.free_head_index_);
  swap(size_, other.size_);
}

template <typename T>
template <typename IncomingValueType>
typename VectorBackedLinkedList<T>::iterator VectorBackedLinkedList<T>::insert(
    const_iterator position,
    IncomingValueType&& value) {
  wtf_size_t position_index = position.GetIndex();
  wtf_size_t prev_index = nodes_[position_index].prev_index_;
  Node& prev = nodes_[prev_index];
  Node& next = nodes_[position_index];

  wtf_size_t new_entry_index;
  if (IsFreeListEmpty()) {
    new_entry_index = nodes_.size();
    prev.next_index_ = new_entry_index;
    next.prev_index_ = new_entry_index;
    nodes_.push_back(Node(prev_index, position_index,
                          std::forward<IncomingValueType>(value)));
  } else {
    new_entry_index = free_head_index_;
    Node& free_head = nodes_[free_head_index_];
    free_head_index_ = free_head.next_index_;
    free_head = Node(prev_index, position_index,
                     std::forward<IncomingValueType>(value));
  }
  size_++;
  return iterator(new_entry_index, this);
}

template <typename T>
void VectorBackedLinkedList<T>::MoveTo(const_iterator target,
                                       const_iterator new_position) {
  wtf_size_t target_index = target.GetIndex();
  Node& target_node = nodes_[target_index];
  Unlink(target_node);

  wtf_size_t new_position_index = new_position.GetIndex();
  wtf_size_t prev_index = nodes_[new_position_index].prev_index_;
  nodes_[prev_index].next_index_ = target_index;
  nodes_[new_position_index].prev_index_ = target_index;
  target_node.prev_index_ = prev_index;
  target_node.next_index_ = new_position_index;
}

template <typename T>
typename VectorBackedLinkedList<T>::iterator VectorBackedLinkedList<T>::erase(
    const_iterator position) {
  DCHECK(position != end());
  wtf_size_t position_index = position.GetIndex();
  Node& node = nodes_[position_index];
  wtf_size_t next_index = node.next_index_;

  Unlink(node);
  node.value_ = HashTraits<T>::EmptyValue();

  node.next_index_ = free_head_index_;
  free_head_index_ = position_index;

  size_--;
  return iterator(next_index, this);
}

template <typename T>
void VectorBackedLinkedList<T>::Unlink(const Node& node) {
  wtf_size_t prev_index = node.prev_index_;
  wtf_size_t next_index = node.next_index_;

  Node& prev_node = nodes_[prev_index];
  Node& next_node = nodes_[next_index];

  prev_node.next_index_ = next_index;
  next_node.prev_index_ = prev_index;
}

}  // namespace WTF

using WTF::VectorBackedLinkedList;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_
