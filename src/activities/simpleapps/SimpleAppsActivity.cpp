#include "SimpleAppsActivity.h"

#include <FS.h>
#include <SD.h>
#include <ArduinoJson.hpp>

#include "fontIds.h"

using namespace ArduinoJson;

void SimpleAppsActivity::onEnter() {
  Activity::onEnter();
  loadApps();
  requestUpdate();
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  fs::File root = SD.open("/apps");
  if (!root || !root.isDirectory()) return;

  fs::File file = root.openNextFile();

  while (file) {
    if (!file.isDirectory()) {
      std::string name = file.name();

      if (name.find(".simpleapp.json") != std::string::npos) {
        apps.push_back(name);
      }
    }
    file = root.openNextFile();
  }
}

bool SimpleAppsActivity::loadApp(const std::string& path) {
  currentApp.clear();

  fs::File file = SD.open(path.c_str());
  if (!file) return false;

  return !deserializeJson(currentApp, file);
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

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
  if (apps.empty()) return;

  std::string path = "/apps/" + apps[selected];

  if (!loadApp(path)) return;

  std::string type = currentApp["type"] | "";

  renderer.clearScreen();

  // =====================
  // COIN FLIP
  // =====================
  if (type == "coin_flip") {
    int r = random(2);
    const char* result = r == 0 ? "HEADS" : "TAILS";
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 200, result, true);
  }

  // =====================
  // RANDOM ANSWER
  // =====================
  else if (type == "random_answer") {
    JsonArray arr = currentApp["answers"];
    if (arr.size() > 0) {
      int idx = random(arr.size());
      const char* txt = arr[idx];
      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 200, txt, true);
    }
  }

  // =====================
  // DAILY COMBO
  // =====================
  else if (type == "daily_combo") {
    JsonArray p = currentApp["prompts"];
    JsonArray g = currentApp["goals"];
    JsonArray t = currentApp["gratitude"];

    const char* pt = p[random(p.size())];
    const char* gt = g[random(g.size())];
    const char* tt = t[random(t.size())];

    renderer.drawText(NOTOSANS_16_FONT_ID, 20, 120, pt, true);
    renderer.drawText(NOTOSANS_16_FONT_ID, 20, 200, gt, true);
    renderer.drawText(NOTOSANS_16_FONT_ID, 20, 280, tt, true);
  }

  // =====================
  // TAROT (NEW)
  // =====================
  else if (type == "random_draw") {

    JsonArray items = currentApp["items"];
    bool reversedEnabled = currentApp["allowReversed"] | false;

    if (items.size() == 0) return;

    int idx = random(items.size());
    JsonObject card = items[idx];

    const char* name = card["name"];
    const char* symbol = card["symbol"];
    const char* upright = card["upright"];
    const char* reversed = card["reversed"];

    bool isReversed = reversedEnabled && random(2) == 1;

    // ===== DRAW CARD =====
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 40, name, true);
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 120, symbol, true);

    const char* meaning = isReversed ? reversed : upright;

    // Split text manually into lines
    int y = 200;
    std::string text = meaning;
    int maxLen = 28;

    for (size_t i = 0; i < text.length(); i += maxLen) {
      std::string line = text.substr(i, maxLen);
      renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, line.c_str(), true);
      y += 30;
    }

    if (isReversed) {
      renderer.drawCenteredText(NOTOSANS_12_FONT_ID, 170, "(Reversed)", true);
    }
  }

  else {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 200, "Unsupported app", true);
  }

  renderer.displayBuffer();
}
}

void SimpleAppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Apps", true);

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 120, "No apps found", true);
  } else {
    for (int i = 0; i < apps.size(); i++) {
      int y = 120 + i * 40;
      std::string line = (i == selected ? "> " : "  ") + apps[i];
      renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, line.c_str(), true);
    }
  }

  renderer.displayBuffer();
}