#pragma once

namespace ChinesePoint::Cjk {

// Presentation only: revealing an answer never changes a card or its schedule.
class ReviewPresentation final {
 public:
  explicit constexpr ReviewPresentation(const bool revealFirst = true) : revealFirst_(revealFirst) { resetForCard(); }

  constexpr void resetForCard() { answerVisible_ = !revealFirst_; }
  constexpr bool answerVisible() const { return answerVisible_; }
  constexpr bool canRate() const { return answerVisible_; }
  constexpr bool reveal() {
    if (answerVisible_) return false;
    answerVisible_ = true;
    return true;
  }

 private:
  bool revealFirst_;
  bool answerVisible_ = false;
};

}  // namespace ChinesePoint::Cjk
