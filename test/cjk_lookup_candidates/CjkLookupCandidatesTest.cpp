#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <set>
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

TEST(CjkLookupCandidates, PrefersPhrasesStartingAtTheTapThenContainingItThenTheTokenAlone) {
  const std::array<SelectableToken, 5> tokens = {
      {{"他", 0, 1, false}, {"喜欢", 1, 2, true}, {"读书", 3, 2, true}, {"。", 5, 1, true}, {"然后", 6, 2, false}}};
  std::array<LookupCandidate, 12> candidates{};

  const size_t count = ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 1, candidates.data(),
                                                                   candidates.size());

  ASSERT_EQ(count, 4u);
  EXPECT_STREQ(candidates[0].text, "喜欢读书");
  EXPECT_STREQ(candidates[1].text, "他喜欢读书");
  EXPECT_STREQ(candidates[2].text, "他喜欢");
  EXPECT_STREQ(candidates[3].text, "喜欢");
}

TEST(CjkLookupCandidates, NeverJoinsAcrossPunctuationOrLatinText) {
  const std::array<SelectableToken, 5> tokens = {
      {{"我", 0, 1, false}, {"爱", 1, 1, true}, {"，", 2, 1, true}, {"you", 3, 3, false}, {"好", 6, 1, false}}};
  std::array<LookupCandidate, 12> candidates{};

  const size_t count = ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 0, candidates.data(),
                                                                   candidates.size());

  ASSERT_EQ(count, 2u);
  EXPECT_STREQ(candidates[0].text, "我爱");
  EXPECT_STREQ(candidates[1].text, "我");
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

TEST(CjkLookupCandidates, BoundsTheResultWithoutDiscardingTheBestCandidate) {
  const std::array<SelectableToken, 3> tokens = {{{"他", 0, 1, false}, {"喜欢", 1, 2, true}, {"读书", 3, 2, true}}};
  std::array<LookupCandidate, 1> candidate{};

  const size_t count =
      ChinesePoint::Cjk::buildCjkLookupCandidates(tokens.data(), tokens.size(), 1, candidate.data(), candidate.size());

  ASSERT_EQ(count, 1u);
  EXPECT_STREQ(candidate[0].text, "喜欢读书");
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
    // Tapping 明: 明月 starts there and comes first; the run still reaches the line start.
    const auto fromMing = candidateTexts(tokens.data(), tokens.size(), 3);
    EXPECT_EQ(fromMing.front(), "明月");
    EXPECT_NE(std::find(fromMing.begin(), fromMing.end(), "举头望明月"), fromMing.end());
    // Tapping the punctuated token: phrases containing it, then the bare 月 last.
    const auto fromYue = candidateTexts(tokens.data(), tokens.size(), 4);
    EXPECT_NE(std::find(fromYue.begin(), fromYue.end(), "明月"), fromYue.end());
    EXPECT_EQ(fromYue.back(), "月");
  }
}

TEST(CjkLookupCandidates, LeadingPunctuationStartsThePhrase) {
  const std::array<SelectableToken, 3> tokens = {{{"说：", 0, 2, false}, {"“明", 2, 2, true}, {"月", 4, 1, true}}};
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 2), (std::vector<std::string>{"明月", "月"}));
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 1), (std::vector<std::string>{"明月", "明"}));
}

TEST(CjkLookupCandidates, PhrasesStopAtAttachedPunctuation) {
  // 床前明月光，疑是地上霜。 tokenised with punctuation attached.
  const std::array<SelectableToken, 4> tokens = {
      {{"月", 0, 1, false}, {"光，", 1, 2, true}, {"疑", 3, 1, true}, {"是", 4, 1, true}}};
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 2), (std::vector<std::string>{"疑是", "疑"}));
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 1), (std::vector<std::string>{"月光", "光"}));
}

TEST(CjkLookupCandidates, PhrasesNeverSpanTwoParagraphs) {
  // <p>…明</p><p>月，…</p>: the paragraph ends with 明 and the next starts with 月，
  const std::array<SelectableToken, 4> tokens = {
      {{"望", 0, 1, false, true}, {"明", 1, 1, true, false}, {"月，", 2, 2, true, true}, {"光", 4, 1, true, false}}};
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 1), (std::vector<std::string>{"望明", "明"}));
  EXPECT_EQ(candidateTexts(tokens.data(), tokens.size(), 2), (std::vector<std::string>{"月"}));
}

// The first candidate a dictionary contains, as DictionaryWordSelectActivity resolves a tap.
std::string resolve(const SelectableToken* tokens, size_t count, size_t selected, const std::set<std::string>& dict) {
  for (const auto& text : candidateTexts(tokens, count, selected)) {
    if (dict.count(text)) return text;
  }
  return "";
}

TEST(CjkLookupCandidates, ATapResolvesToTheWordEvenWhenItsHanziAreEntries) {
  // 他忽然有点害怕， with a full dictionary: every single Hanzi is an entry too.
  const std::set<std::string> dict = {"他", "忽", "然", "有", "点", "害", "怕", "忽然", "有点", "害怕"};
  const std::array<SelectableToken, 7> tokens = {{{"他", 0, 1, false},
                                                  {"忽", 1, 1, true},
                                                  {"然", 2, 1, true},
                                                  {"有", 3, 1, true},
                                                  {"点", 4, 1, true},
                                                  {"害", 5, 1, true},
                                                  {"怕，", 6, 2, true}}};
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 5, dict), "害怕");  // 害 starts the word
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 6, dict), "害怕");  // 怕， ends it
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 1, dict), "忽然");
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 2, dict), "忽然");
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 0, dict), "他");  // no longer word: the Hanzi itself
  // A word starting at the tap wins over a longer one merely containing it,
  // and a containing word wins over the bare Hanzi.
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 4, std::set<std::string>{"有点害怕", "点害", "点"}), "点害");
  EXPECT_EQ(resolve(tokens.data(), tokens.size(), 4, std::set<std::string>{"有点害怕", "点"}), "有点害怕");
}

TEST(CjkLookupCandidates, PunctuationOnlyTokensHaveNoCandidates) {
  const std::array<SelectableToken, 2> tokens = {{{"月", 0, 1, false}, {"。”", 1, 2, true}}};
  EXPECT_TRUE(candidateTexts(tokens.data(), tokens.size(), 1).empty());
}

}  // namespace
