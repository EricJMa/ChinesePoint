#pragma once

#include <cstddef>
#include <cstdint>

#include "chinesepoint/cjk/CjkSentenceSelection.h"

namespace ChinesePoint::Cjk {

// The candidate buffer is deliberately fixed: dictionary lookup happens while
// the reader page and its font caches are resident, so a CJK fallback must not
// add dynamic allocation pressure to that path.
constexpr size_t kMaxLookupCandidateBytes = 64;
constexpr uint16_t kMaxLookupCandidateCodepoints = 8;
// For a single selected Hanzi there are n windows of length n containing it:
// every window of length 1..8 fits, so ordering, not capacity, picks the word.
constexpr size_t kMaxLookupCandidates = kMaxLookupCandidateCodepoints * (kMaxLookupCandidateCodepoints + 1) / 2;

struct LookupCandidate {
  char text[kMaxLookupCandidateBytes + 1] = {};
  size_t bytes = 0;
  uint16_t codepoints = 0;
};

// Builds distinct contiguous CJK lookup queries around the selected token, in
// the order a reader expects a tap to resolve: phrases starting at the tapped
// token (longest first), then other phrases containing it (longest first),
// then the tapped token alone. Punctuation attached to a token's edge ("月，")
// is dropped from the query and ends the phrase there; so does a block start.
// Malformed UTF-8, Latin text or an overlong phrase is never emitted.
size_t buildCjkLookupCandidates(const SelectableToken* tokens, size_t tokenCount, size_t selectedTokenIndex,
                                LookupCandidate* output, size_t outputCapacity);

}  // namespace ChinesePoint::Cjk
