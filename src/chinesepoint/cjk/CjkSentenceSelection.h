#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "chinesepoint/cjk/CjkLearnerModel.h"

namespace ChinesePoint::Cjk {

struct SelectableToken {
  std::string_view text;
  uint32_t visibleCodepointOffset = 0;
  uint16_t visibleCodepointLength = 0;
  bool joinWithoutSpaceBefore = false;
  // First token of a paragraph, heading or table cell.
  bool startsBlock = false;
};

enum class SentenceCompleteness : uint8_t {
  Complete,
  TruncatedStart,
  TruncatedEnd,
  TruncatedBoth,
};

struct SentenceSelection {
  TextAnchor anchor{};
  uint16_t firstTokenIndex = 0;
  uint16_t lastTokenIndex = 0;
  uint16_t selectedSentenceCodepoint = 0;
  SentenceCompleteness completeness = SentenceCompleteness::Complete;
};

bool isSentenceTerminal(std::string_view token);
// Estimated visible-text offset of the word after `word`, which starts at
// `wordOffset`: words are assumed contiguous, separated by one space unless
// `nextJoinsWithoutSpace`. Exact for single-spaced source text; collapsed
// whitespace, synthetic list markers and RTL lines make it approximate.
uint32_t followingWordOffset(uint32_t wordOffset, std::string_view word, bool nextJoinsWithoutSpace);
// Clause punctuation (e.g. "，" ending a verse line) that carries a sentence into the next block.
// Colons are excluded: a block ending in "：" is usually a heading or label introducing the next.
bool continuesSentence(std::string_view token);
uint16_t utf8CodepointCount(std::string_view text);
// `text` without the punctuation layout attaches to its edges (e.g. "月，" or
// "「明"); `leadingCodepoints` receives how many codepoints were dropped in front.
std::string_view trimEdgePunctuation(std::string_view text, uint16_t& leadingCodepoints);
uint32_t selectionFingerprint(std::string_view text);

// Builds the sentence around `selectedTokenIndex` into caller-owned storage.
// A block start ends the sentence unless the previous block ends mid-sentence,
// so headings and bylines are never captured as part of the text below them.
// It returns false rather than truncating: a partial sentence must never be
// written to a learner record as if it were complete.
bool buildSentenceSelection(const SelectableToken* tokens, size_t tokenCount, size_t selectedTokenIndex,
                            uint16_t spineIndex, bool startsAtSectionBoundary, bool endsAtSectionBoundary, char* output,
                            size_t outputCapacity, SentenceSelection& selection);

}  // namespace ChinesePoint::Cjk
