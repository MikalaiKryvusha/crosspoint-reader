#include "MamaActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "activities/ActivityManager.h"
#include "fontIds.h"
#include "images/HeartBig.h"

void MamaActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void MamaActivity::render(RenderLock&&) {
  constexpr int HEART = 160;
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int line = renderer.getLineHeight(UI_12_FONT_ID);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int bodyTop = metrics.topPadding + metrics.headerHeight;
  const int bodyH = h - bodyTop - metrics.buttonHintsHeight;
  const int blockH = HEART + 36 + 3 * line;
  const int top = bodyTop + (bodyH - blockH) / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, w, metrics.headerHeight}, "Для любимой Мамы", nullptr);
  renderer.drawImage(HeartBig, (w - HEART) / 2, top, HEART, HEART);
  int y = top + HEART + 36;
  renderer.drawCenteredText(UI_12_FONT_ID, y, "Прошивка читалки", true, EpdFontFamily::BOLD);
  y += line;
  renderer.drawCenteredText(UI_12_FONT_ID, y, "улучшена Колей", true, EpdFontFamily::BOLD);
  y += line;
  renderer.drawCenteredText(UI_12_FONT_ID, y, "для любимой Мамы", true, EpdFontFamily::BOLD);
  const auto labels = mappedInput.mapLabels("Назад", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

// Back (the header arrow, the button), OK or a tap anywhere returns Home on this item.
void MamaActivity::loop() {
  int x = 0;
  int y = 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    activityManager.goHome(HomeMenuItem::MAMA_HEART);
  }
}
