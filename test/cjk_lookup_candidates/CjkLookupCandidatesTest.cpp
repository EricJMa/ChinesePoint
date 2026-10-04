#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "chinesepoint/cjk/CjkLookupCandidates.h"

namespace {
using ChinesePoint::Cjk::LookupCandidate;
using ChinesePoint::Cjk::SelectableToken;

TEST(CjkLookupCandidates, KeepsShortDictionaryWordsInALongRenderedCjkRun) {
  // The real reader also puts heading/author tokens before the first sentence.
  const std::array<SelectableToken, 10> tokens = {{{"静", 0, 1, false},
                                                   {"夜", 1, 1, true},
                                                   {"思", 2, 1, true},
                                                   {"李", 3, 1, true},
                                                   {"白", 4, 1, true},
                                                   {"床", 5, 1, true},
                                                   {"前", 6, 1, true},
                                                   {"明", 7, 1, true},
                                                   {"月", 8, 1, true},
                                                   {"光，", 9, 2, true}}};
  std::array<LookupCandidate, ChinesePoint::Cjk::kMaxLookupCandidates> candidates{};
  const size_t count = ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 7, candidates.data(),
                                                                   candidates.size());
  bool found = false;
  for (size_t index = 0; index < count; ++index) found |= std::strcmp(candidates[index].text, "明月") == 0;
  EXPECT_TRUE(found);
}

TEST(CjkLookupCandidates, TriesLongestContiguousPhraseBeforeShorterPhrases) {
  const std::array<SelectableToken, 5> tokens = {
      {{"他", 0, 1, false}, {"喜欢", 1, 2, true}, {"读书", 3, 2, true}, {"。", 5, 1, true}, {"然后", 6, 2, false}}};
  std::array<LookupCandidate, 12> candidates{};

  const size_t count = ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 1, candidates.data(),
                                                                   candidates.size());

  ASSERT_EQ(count, 3u);
  EXPECT_STREQ(candidates[0].text, "他喜欢读书");
  EXPECT_STREQ(candidates[1].text, "喜欢读书");
  EXPECT_STREQ(candidates[2].text, "他喜欢");
}

TEST(CjkLookupCandidates, NeverJoinsAcrossPunctuationOrLatinText) {
  const std::array<SelectableToken, 5> tokens = {
      {{"我", 0, 1, false}, {"爱", 1, 1, true}, {"，", 2, 1, true}, {"you", 3, 3, false}, {"好", 6, 1, false}}};
  std::array<LookupCandidate, 12> candidates{};

  const size_t count = ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 0, candidates.data(),
                                                                   candidates.size());

  ASSERT_EQ(count, 1u);
  EXPECT_STREQ(candidates[0].text, "我爱");
}

TEST(CjkLookupCandidates, RejectsMalformedAndOversizeCjkTokens) {
  const std::array<SelectableToken, 1> malformed = {{{"\xE4\xB8", 0, 1, false}}};
  const std::array<SelectableToken, 2> oversized = {
      {{"这是一个非常非常非常非常非常非常非常非常长的中文词条", 0, 26, false}, {"继续", 26, 2, true}}};
  std::array<LookupCandidate, 12> candidates{};

  EXPECT_EQ(ChinesePoint::Cjk::buildCjkLookupCandidates(malformed.data(), malformed.size(), 0, candidates.data(),
                                                        candidates.size()),
            0u);
  EXPECT_EQ(ChinesePoint::Cjk::buildCjkLookupCandidates(oversized.data(), oversized.size(), 0, candidates.data(),
                                                        candidates.size()),
            0u);
}

TEST(CjkLookupCandidates, BoundsTheResultWithoutDiscardingTheLongestPhrase) {
  const std::array<SelectableToken, 3> tokens = {{{"他", 0, 1, false}, {"喜欢", 1, 2, true}, {"读书", 3, 2, true}}};
  std::array<LookupCandidate, 1> candidate{};

  const size_t count =
      ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 1, candidate.data(), candidate.size());

  ASSERT_EQ(count, 1u);
  EXPECT_STREQ(candidate[0].text, "他喜欢读书");
}

std::vector<std::string> candidateTexts(const SelectableToken* tokens, size_t count, size_t selected) {
  std::array<LookupCandidate, ChinesePoint::Cjk::kMaxLookupCandidates> candidates{};
  const size_t found =
      ChinesePoint::Cjk::buildCjkLookupCandidates(tokens, count, selected, candidates.data(), candidates.size());
  std::vector<std::string> texts;
  for (size_t index = 0; index < found; ++index) texts.emplace_back(candidates[index].text);
  return texts;
}

TEST(CjkLookupCandidates, TrailingPunctuationDoesNotHideTheWordBeforeIt) {
  // Layout attaches punctuation to the preceding Hanzi: 举头望明月，
  for (const char* ending : {"月，", "月。", "月。”", "月」"}) {
    const std::array<SelectableToken, 5> tokens = {
        {{"举", 0, 1, false}, {"头", 1, 1, true}, {"望", 2, 1, true}, {"明", 3, 1, true}, {ending, 4, 2, true}}};
    SCOPED_TRACE(ending);
    // Tapping 明: 明月 is reachable; the run still includes the start of the line.
    const auto fromMing = candidateTexts(tokens.data(), tokens.size(), 3);
    EXPECT_NE(std::find(fromMing.begin(), fromMing.end(), "明月"), fromMing.end());
    EXPECT_EQ(fromMing.front(), "举头望明月");
    // Tapping the punctuated token: the bare 月 and 明月 are both tried, longest first.
    const auto fromYue = candidateTexts(tokens.data(), tokens.size(), 4);
    EXPECT_NE(std::find(fromYue.begin(), fromYue.end(), "明月"), fromYue.end());
    EXPECT_EQ(fromYue.back(), "月");
  }
}

TEST(CjkLookupCandidates, LeadingPunctuationStartsThePhrase) {
  const std::array<SelectableToken, 3> tokens = {{{"说：", 0, 2, false}, {"“明", 2, 2, true}, {"月", 4, 1, true}}};
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 2), (std::vector<std::string>{"明月"}));
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 1), (std::vector<std::string>{"明月", "明"}));
}

TEST(CjkLookupCandidates, PhrasesStopAtAttachedPunctuation) {
  // 床前明月光，疑是地上霜。 tokenised with punctuation attached.
  const std::array<SelectableToken, 4> tokens = {
      {{"月", 0, 1, false}, {"光，", 1, 2, true}, {"疑", 3, 1, true}, {"是", 4, 1, true}}};
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 2), (std::vector<std::string>{"疑是"}));
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 1), (std::vector<std::string>{"月光", "光"}));
}

TEST(CjkLookupCandidates, PunctuationOnlyTokensHaveNoCandidates) {
  const std::array<SelectableToken, 2> tokens = {{{"月", 0, 1, false}, {"。”", 1, 2, true}}};
  EXPECT_TRUE(candidateTexts(tokens.data(), tokens.size(), 1).empty());
}

}  // namespace
