// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/symbols_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"

#include <unicode/uchar.h>
#include <unicode/uniset.h>
#include <memory>

namespace blink {

namespace {
using emoji_text_iter_t = UTF16RagelIterator;
// Scanner gode generated by Ragel and imported from third_party.
#include "third_party/emoji-segmenter/src/emoji_presentation_scanner.c"
}  // namespace

SymbolsIterator::SymbolsIterator(const UChar* buffer, unsigned buffer_size)
    : cursor_(0), next_token_end_(0), next_token_emoji_(false) {
  if (buffer_size) {
    buffer_iterator_ = UTF16RagelIterator(buffer, buffer_size, cursor_);
    next_token_end_ = cursor_ + (scan_emoji_presentation(buffer_iterator_,
                                                         buffer_iterator_.end(),
                                                         &next_token_emoji_) -
                                 buffer_iterator_);
  }
}

bool SymbolsIterator::Consume(unsigned* symbols_limit,
                              FontFallbackPriority* font_fallback_priority) {
  if (cursor_ >= buffer_iterator_.end().Cursor())
    return false;

  bool current_token_emoji = false;
  do {
    cursor_ = next_token_end_;
    current_token_emoji = next_token_emoji_;

    if (cursor_ >= buffer_iterator_.end().Cursor())
      break;

    buffer_iterator_.SetCursor(cursor_);
    next_token_end_ = cursor_ + (scan_emoji_presentation(buffer_iterator_,
                                                         buffer_iterator_.end(),
                                                         &next_token_emoji_) -
                                 buffer_iterator_);

  } while (current_token_emoji == next_token_emoji_);

  *font_fallback_priority = current_token_emoji
                                ? FontFallbackPriority::kEmojiEmoji
                                : FontFallbackPriority::kText;
  *symbols_limit = cursor_;

  return true;
}

}  // namespace blink
