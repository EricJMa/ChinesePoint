#include "chinesepoint/cjk/CjkSentenceSelection.h"

#include <cstring>
#include <limits>

namespace ChinesePoint::Cjk {
namespace {

SentenceCompleteness completenessFor(bool completeStart, bool completeEnd) {
  if (completeStart && completeEnd) return SentenceCompleteness::Complete;
  if (!completeStart && !completeEnd) return SentenceCompleteness::TruncatedBoth;
  return completeStart ? SentenceCompleteness::TruncatedEnd : SentenceCompleteness::TruncatedStart;
}

}  // namespace

uint16_t utf8CodepointCount(const std::string_view text) {
  uint32_t count = 0;
  for (const unsigned char ch : text) {
    if ((ch & 0xC0u) != 0x80u) ++count;
  }
  return count > std::numeric_limits<uint16_t>::max() ? std::numeric_limits<uint16_t>::max()
                                                      : static_cast<uint16_t>(count);
}

namespace {

// Length of the UTF-8 sequence starting with `lead`; 0 for a continuation byte.
size_t sequenceLength(const unsigned char lead) {
  if (lead < 0x80u) return 1;
  if ((lead & 0xE0u) == 0xC0u) return 2;
  if ((lead & 0xF0u) == 0xE0u) return 3;
  if ((lead & 0xF8u) == 0xF0u) return 4;
  return 0;
}

uint32_t decodeAt(const std::string_view text, const size_t offset, const size_t length) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (length == 1) return lead;
  uint32_t value = lead & (0x7Fu >> length);
  for (size_t index = 1; index < length; ++index)
    value = (value << 6u) | (static_cast<unsigned char>(text[offset + index]) & 0x3Fu);
  return value;
}

// ASCII punctuation, General Punctuation (quotes, dashes, ellipsis), CJK
// Symbols and Punctuation (。、「」《》【】) and fullwidth ，！？：；（）.
bool isEdgePunctuation(const uint32_t cp) {
  if (cp < 0x80u)
    return (cp >= 0x21u && cp <= 0x2Fu) || (cp >= 0x3Au && cp <= 0x40u) || (cp >= 0x5Bu && cp <= 0x60u) ||
           (cp >= 0x7Bu && cp <= 0x7Eu);
  return (cp >= 0x2010u && cp <= 0x205Eu) || (cp >= 0x3001u && cp <= 0x3003u) || (cp >= 0x3008u && cp <= 0x3011u) ||
         (cp >= 0x3014u && cp <= 0x301Fu) || (cp >= 0xFF01u && cp <= 0xFF0Fu) || (cp >= 0xFF1Au && cp <= 0xFF20u) ||
         (cp >= 0xFF3Bu && cp <= 0xFF40u) || (cp >= 0xFF5Bu && cp <= 0xFF65u);
}

}  // namespace

std::string_view trimEdgePunctuation(std::string_view text, uint16_t& leadingCodepoints) {
  leadingCodepoints = 0;
  while (!text.empty()) {
    const size_t length = sequenceLength(static_cast<unsigned char>(text.front()));
    if (length == 0 || length > text.size() || !isEdgePunctuation(decodeAt(text, 0, length))) break;
    text.remove_prefix(length);
    ++leadingCodepoints;
  }
  while (!text.empty()) {
    size_t start = text.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0u) == 0x80u) --start;
    const size_t length = sequenceLength(static_cast<unsigned char>(text[start]));
    if (length != text.size() - start || !isEdgePunctuation(decodeAt(text, start, length))) break;
    text.remove_suffix(length);
  }
  return text;
}

uint32_t selectionFingerprint(const std::string_view text) {
  uint32_t hash = 0x811C9DC5u;
  for (const unsigned char ch : text) {
    hash ^= ch;
    hash *= 0x01000193u;
  }
  return hash;
}

bool isSentenceTerminal(const std::string_view token) {
  if (token.empty()) return false;
  const char last = token.back();
  if (last == '.' || last == '!' || last == '?') return true;
  constexpr std::string_view terminals[] = {"。", "！", "？"};
  for (const auto terminal : terminals) {
    if (token.size() >= terminal.size() &&
        token.compare(token.size() - terminal.size(), terminal.size(), terminal) == 0) {
      return true;
    }
  }
  return false;
}

uint32_t followingWordOffset(const uint32_t wordOffset, const std::string_view word, const bool nextJoinsWithoutSpace) {
  return wordOffset + utf8CodepointCount(word) + (nextJoinsWithoutSpace ? 0u : 1u);
}

bool continuesSentence(const std::string_view token) {
  if (token.empty()) return false;
  const char last = token.back();
  if (last == ',' || last == ';') return true;
  constexpr std::string_view marks[] = {"，", "、", "；"};
  for (const auto mark : marks) {
    if (token.size() >= mark.size() && token.compare(token.size() - mark.size(), mark.size(), mark) == 0) return true;
  }
  return false;
}

bool buildSentenceSelection(const SelectableToken* tokens, const size_t tokenCount, const size_t selectedTokenIndex,
                            const uint16_t spineIndex, const bool startsAtSectionBoundary,
                            const bool endsAtSectionBoundary, char* output, const size_t outputCapacity,
                            SentenceSelection& selection) {
  selection = {};
  if (output != nullptr && outputCapacity > 0) output[0] = '\0';
  if (tokens == nullptr || tokenCount == 0 || tokenCount > std::numeric_limits<uint16_t>::max() ||
      selectedTokenIndex >= tokenCount || output == nullptr || outputCapacity == 0) {
    return false;
  }

  const auto blockBreakBefore = [tokens](const size_t index) {
    return tokens[index].startsBlock && !continuesSentence(tokens[index - 1].text);
  };

  size_t first = selectedTokenIndex;
  bool completeStart = false;
  while (first > 0) {
    if (isSentenceTerminal(tokens[first - 1].text) || blockBreakBefore(first)) {
      completeStart = true;
      break;
    }
    --first;
  }
  if (first == 0) completeStart = startsAtSectionBoundary || tokens[0].startsBlock;

  size_t last = selectedTokenIndex;
  bool completeEnd = false;
  for (; last < tokenCount; ++last) {
    if (isSentenceTerminal(tokens[last].text) || (last + 1 < tokenCount && blockBreakBefore(last + 1))) {
      completeEnd = true;
      break;
    }
  }
  if (last == tokenCount) {
    last = tokenCount - 1;
    completeEnd = endsAtSectionBoundary;
  }

  size_t requiredBytes = 1;
  uint32_t requiredCodepoints = 0;
  uint32_t selectedCodepoint = 0;
  bool emitted = false;
  for (size_t index = first; index <= last; ++index) {
    const auto& token = tokens[index];
    const bool addSpace = emitted && !token.joinWithoutSpaceBefore;
    if (index == selectedTokenIndex) selectedCodepoint = requiredCodepoints + (addSpace ? 1u : 0u);
    requiredBytes += token.text.size() + (addSpace ? 1u : 0u);
    requiredCodepoints +=
        (token.visibleCodepointLength > 0 ? token.visibleCodepointLength : utf8CodepointCount(token.text)) +
        (addSpace ? 1u : 0u);
    emitted = emitted || !token.text.empty();
  }
  if (requiredBytes > outputCapacity || selectedCodepoint > std::numeric_limits<uint16_t>::max()) return false;

  size_t position = 0;
  emitted = false;
  for (size_t index = first; index <= last; ++index) {
    const auto& token = tokens[index];
    if (emitted && !token.joinWithoutSpaceBefore) output[position++] = ' ';
    if (!token.text.empty()) {
      std::memcpy(output + position, token.text.data(), token.text.size());
      position += token.text.size();
      emitted = true;
    }
  }
  output[position] = '\0';

  // Anchor the word itself, not punctuation layout attached to it.
  const auto& selected = tokens[selectedTokenIndex];
  uint16_t leading = 0;
  std::string_view bareWord = trimEdgePunctuation(selected.text, leading);
  if (bareWord.empty()) {
    bareWord = selected.text;
    leading = 0;
  }
  selection.anchor = {spineIndex, selected.visibleCodepointOffset + leading, utf8CodepointCount(bareWord),
                      selectionFingerprint(bareWord)};
  selection.firstTokenIndex = static_cast<uint16_t>(first);
  selection.lastTokenIndex = static_cast<uint16_t>(last);
  selection.selectedSentenceCodepoint = static_cast<uint16_t>(selectedCodepoint);
  selection.completeness = completenessFor(completeStart, completeEnd);
  return true;
}

}  // namespace ChinesePoint::Cjk
