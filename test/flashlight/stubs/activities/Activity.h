#pragma once
#include <GfxRenderer.h>
#include <MappedInputManager.h>
class Activity {
 public:
  Activity(const char*, GfxRenderer& r, MappedInputManager& i) : renderer(r), mappedInput(i) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  void requestUpdate() {}

 protected:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
};
