#include "SimpleAppsActivity.h"
#include <SD.h>
#include "components/UITheme.h"
#include "fontIds.h"

void SimpleAppsActivity::onEnter() {
  Activity::onEnter();
  loadApps();
  requestUpdate();
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  File root = SD.open("/apps");
  if (!root) return;

  File file = root.openNextFile();
  while (file) {
    std::string name = file.name();
    if (name.find(".simpleapp.json") != std::string::npos) {
      apps.push_back(name);
    }
    file = root.openNextFile();
  }
}

void SimpleAppsActivity::loop() {
  nav.onNext([this] {
    if (!apps.empty()) {
      selected = (selected + 1) % apps.size();
      requestUpdate();
    }
  });

  nav.onPrevious([this] {
    if (!apps.empty()) {
      selected = (selected - 1 + apps.size()) % apps.size();
      requestUpdate();
    }
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void SimpleAppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 20, "Apps", true);

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 120, "No apps found", true);
    renderer.drawCenteredText(UI_12_FONT_ID, 160, "Upload to /apps", true);
  } else {
    for (int i = 0; i < apps.size(); i++) {
      int y = 120 + i * 40;
      std::string line = (i == selected ? "> " : "  ") + apps[i];
      renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, line.c_str(), true);
    }
  }

  renderer.displayBuffer();
}