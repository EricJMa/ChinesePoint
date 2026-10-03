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
// For a single selected Hanzi there are n windows of length n containing it.
// Keep every window of length 2..8: a cap of 12 retained long misses while
// silently discarding common two-Hanzi words in real, longer reading runs.
constexpr size_t kMaxLookupCandidates = kMaxLookupCandidateCodepoints * (kMaxLookupCandidateCodepoints + 1) / 2 - 1;

struct LookupCandidate {
  char text[kMaxLookupCandidateBytes + 1] = {};
  size_t bytes = 0;
  uint16_t codepoints = 0;
};

// Builds distinct, longest-first contiguous CJK phrases around the selected
// token. The selected token itself is intentionally excluded because the
// caller always tries the normal StarDict lookup first. A malformed UTF-8
// token, punctuation, Latin text, or an overlong phrase is never emitted.
size_t buildCjkLookupCandidates(const SelectableToken* tokens, size_t tokenCount, size_t selectedTokenIndex,
                                LookupCandidate* output, size_t outputCapacity);

}  // namespace ChinesePoint::Cjk
