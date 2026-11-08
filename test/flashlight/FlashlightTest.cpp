#include <CrossPointSettings.h>
#include <HalFrontlight.h>
#include <activities/ActivityManager.h>
#include <components/UITheme.h>

#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "activities/util/FlashlightActivity.h"

int main() {
  GfxRenderer renderer;
  MappedInputManager input;
  auto& light = Frontlight;
  for (bool on : {false, true})
    for (unsigned brightness : {0u, 1u, 5u, 50u, 100u})
      for (unsigned warmth : {0u, 50u, 100u}) {
        light.begin(brightness, warmth, on);
        FlashlightActivity activity(renderer, input);
        activity.onEnter();
        assert(physicalOutput == 100 && rawLevel == 255);
        assert(light.brightness() == brightness && light.isOn() == on && light.warmth() == warmth);
        for (bool dark : {false, true})
          for (int width : {480, 800}) {
            SETTINGS.screenInverted = dark;
            panelInverted = dark;
            panelWidth = width;
            activity.render(RenderLock{});
            assert(shownValue == 255 && lastHeader.width == width && lastLabel == "Exit");
          }
        const auto count = activityManager.homes;
        activity.loop();
        assert(activityManager.homes == count);
        input.tapped = true;
        activity.loop();
        input.tapped = false;
        assert(activityManager.homes == count + 1 && activityManager.selected == HomeMenuItem::FLASHLIGHT);
        input.back = true;
        activity.loop();
        input.back = false;
        input.confirm = true;
        activity.loop();
        input.confirm = false;
        assert(activityManager.homes == count + 3);
        activity.onExit();
        assert(physicalOutput == (on ? static_cast<int>(brightness) : 0));
        assert(light.brightness() == brightness && light.isOn() == on && light.warmth() == warmth);
      }
  std::puts(
      "Flashlight real activity+HAL: temporary output, saved state, dark/light display, oriented width and exit inputs "
      "passed");
}
