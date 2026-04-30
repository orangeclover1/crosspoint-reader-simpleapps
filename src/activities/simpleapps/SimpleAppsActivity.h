#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include <vector>
#include <string>

class SimpleAppsActivity final : public Activity {
  ButtonNavigator nav;
  std::vector<std::string> apps;
  int selected = 0;

  void loadApps();

 public:
  SimpleAppsActivity(GfxRenderer& r, MappedInputManager& i)
      : Activity("Apps", r, i) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};