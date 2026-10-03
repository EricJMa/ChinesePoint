#include <gtest/gtest.h>

#include "chinesepoint/cjk/CjkReviewPresentation.h"

namespace {
using ChinesePoint::Cjk::ReviewPresentation;

TEST(CjkReviewPresentation, NewCardHidesAnswerAndDisablesRating) {
  const ReviewPresentation presentation;
  EXPECT_FALSE(presentation.answerVisible());
  EXPECT_FALSE(presentation.canRate());
}

TEST(CjkReviewPresentation, RevealEnablesRatingExactlyOnce) {
  ReviewPresentation presentation;
  EXPECT_TRUE(presentation.reveal());
  EXPECT_TRUE(presentation.answerVisible());
  EXPECT_TRUE(presentation.canRate());
  EXPECT_FALSE(presentation.reveal());
}

TEST(CjkReviewPresentation, NextCardRequiresAnotherReveal) {
  ReviewPresentation presentation;
  presentation.reveal();
  presentation.resetForCard();
  EXPECT_FALSE(presentation.answerVisible());
  EXPECT_FALSE(presentation.canRate());
}

TEST(CjkReviewPresentation, BaselineModeKeepsAnswerAndRatingsVisible) {
  ReviewPresentation baseline(false);
  EXPECT_TRUE(baseline.answerVisible());
  EXPECT_TRUE(baseline.canRate());
  EXPECT_FALSE(baseline.reveal());
  baseline.resetForCard();
  EXPECT_TRUE(baseline.canRate());
}
}  // namespace
