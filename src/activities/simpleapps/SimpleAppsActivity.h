#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include <ArduinoJson.hpp>
#include <vector>
#include <string>

class SimpleAppsActivity final : public Activity {
  ButtonNavigator nav;

  std::vector<std::string> apps;
  int selected = 0;

  ArduinoJson::JsonDocument currentApp;

 public:
  SimpleAppsActivity(GfxRenderer& r, MappedInputManager& i)
      : Activity("Apps", r, i) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void renderCoinFlip();
  void renderEightBall();
  void renderDaily();
  void renderTarot();
  void drawTarot();
  void loadApps();
  bool loadApp(const std::string& path);
};