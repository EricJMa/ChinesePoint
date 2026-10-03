#include <gtest/gtest.h>

#include <array>

#include "chinesepoint/cjk/CjkSentenceSelection.h"

namespace {
using ChinesePoint::Cjk::SelectableToken;
using ChinesePoint::Cjk::SentenceCompleteness;
using ChinesePoint::Cjk::SentenceSelection;

TEST(CjkSentenceSelection, KeepsChineseTokensTogetherAndAnchorsTouchedWord) {
  const std::array<SelectableToken, 5> tokens = {{{"他", 10, 1, false},
                                                  {"喜欢", 11, 2, true},
                                                  {"读书", 13, 2, true},
                                                  {"。", 15, 1, true},
                                                  {"下一句", 16, 3, false}}};
  std::array<char, 64> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 1, 4, true, false,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "他喜欢读书。");
  EXPECT_EQ(selection.anchor.spineIndex, 4u);
  EXPECT_EQ(selection.anchor.visibleCodepointOffset, 11u);
  EXPECT_EQ(selection.anchor.codepointLength, 2u);
  EXPECT_EQ(selection.selectedSentenceCodepoint, 1u);
  EXPECT_EQ(selection.completeness, SentenceCompleteness::Complete);
}

TEST(CjkSentenceSelection, ReportsAnIncompletePageWindowInsteadOfPretendingItIsComplete) {
  const std::array<SelectableToken, 2> tokens = {{{"continua", 0, 7, false}, {"aqui", 8, 4, false}}};
  std::array<char, 64> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 0, 0, false, false,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "continua aqui");
  EXPECT_EQ(selection.completeness, SentenceCompleteness::TruncatedBoth);
}

// 静夜思 as laid out: <h1>静夜思</h1><p>李白</p><p>床前明月光，</p><p>疑是地上霜。</p>
// Each block starts a line, so only the first token of each block is a block start.
const std::array<SelectableToken, 12> kPoemTokens = {{{"静", 0, 1, false, true},
                                                      {"夜思", 1, 2, true, false},
                                                      {"李", 3, 1, true, true},
                                                      {"白", 4, 1, true, false},
                                                      {"床前", 5, 2, true, true},
                                                      {"明月", 7, 2, true, false},
                                                      {"光", 9, 1, true, false},
                                                      {"，", 10, 1, true, false},
                                                      {"疑是", 11, 2, true, true},
                                                      {"地上", 13, 2, true, false},
                                                      {"霜", 15, 1, true, false},
                                                      {"。", 16, 1, true, false}}};

TEST(CjkSentenceSelection, HeadingAndBylineAreNotPartOfTheVerseBelow) {
  std::array<char, 128> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(kPoemTokens.data(), kPoemTokens.size(), 5, 0, true, true,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "床前明月光，疑是地上霜。");
  EXPECT_EQ(selection.firstTokenIndex, 4u);
  EXPECT_EQ(selection.lastTokenIndex, 11u);
  EXPECT_EQ(selection.selectedSentenceCodepoint, 2u);
  EXPECT_EQ(selection.completeness, SentenceCompleteness::Complete);
}

TEST(CjkSentenceSelection, UnpunctuatedBlocksStandAlone) {
  std::array<char, 128> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(kPoemTokens.data(), kPoemTokens.size(), 1, 0, true, true,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "静夜思");
  EXPECT_EQ(selection.completeness, SentenceCompleteness::Complete);

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(kPoemTokens.data(), kPoemTokens.size(), 3, 0, true, true,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "李白");
}

TEST(CjkSentenceSelection, ParagraphStartEndsAnUnterminatedSentence) {
  const std::array<SelectableToken, 4> tokens = {{{"第一段", 0, 3, false, true},
                                                  {"没有句号", 3, 4, true, false},
                                                  {"第二段", 7, 3, true, true},
                                                  {"开始。", 10, 3, true, false}}};
  std::array<char, 64> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 2, 0, false, true,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "第二段开始。");
  EXPECT_EQ(selection.completeness, SentenceCompleteness::Complete);
}

TEST(CjkSentenceSelection, LabelEndingInColonStaysOutOfTheNextBlock) {
  const std::array<SelectableToken, 5> tokens = {{{"例句", 0, 2, false, true},
                                                  {"：", 2, 1, true, false},
                                                  {"床前", 3, 2, true, true},
                                                  {"明月光", 5, 3, true, false},
                                                  {"。", 8, 1, true, false}}};
  std::array<char, 64> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 3, 0, true, true, sentence.data(),
                                                        sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "床前明月光。");
  EXPECT_EQ(selection.completeness, SentenceCompleteness::Complete);
}

TEST(CjkSentenceSelection, PageStartingMidParagraphStaysTruncated) {
  const std::array<SelectableToken, 2> tokens = {{{"接上页", 0, 3, false, false}, {"的句子。", 3, 4, true, false}}};
  std::array<char, 64> sentence{};
  SentenceSelection selection;

  ASSERT_TRUE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 0, 0, false, true,
                                                        sentence.data(), sentence.size(), selection));
  EXPECT_EQ(selection.completeness, SentenceCompleteness::TruncatedStart);
}

TEST(CjkSentenceSelection, RefusesToTruncateSavedSentenceBuffer) {
  const std::array<SelectableToken, 2> tokens = {{{"frase", 0, 5, false}, {" longa.", 6, 7, false}}};
  std::array<char, 4> sentence{};
  SentenceSelection selection;

  EXPECT_FALSE(ChinesePoint::Cjk::buildSentenceSelection(tokens.data(), tokens.size(), 0, 0, true, true,
                                                         sentence.data(), sentence.size(), selection));
  EXPECT_STREQ(sentence.data(), "");
}

}  // namespace
