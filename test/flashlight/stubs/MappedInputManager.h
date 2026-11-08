#pragma once
class MappedInputManager {
 public:
  enum class Button { Back, Confirm };
  bool tapped = false, back = false, confirm = false;
  bool wasScreenTapped(int&, int&) const { return tapped; }
  bool wasReleased(Button b) const { return b == Button::Back ? back : confirm; }
};
