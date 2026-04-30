#include "SimpleAppsActivity.h"

#include <SD.h>
#include <esp_system.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "EpdFontFamily.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* APPS_DIR = "/apps";
constexpr const char* SIMPLEAPP_SUFFIX = ".simpleapp.json";

bool endsWith(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) return false;
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string safeString(JsonVariantConst value, const char* fallback = "") {
  if (value.is<const char*>()) return value.as<const char*>();
  return fallback;
}

// ✅ UPDATED: supports string OR object
const char* answerText(JsonVariantConst value) {
  if (value.is<const char*>()) {
    return value.as<const char*>();
  }
  if (value.is<JsonObjectConst>()) {
    return value["text"] | "No answer.";
  }
  return "No answer.";
}

// ✅ UPDATED: supports per-answer symbol + fallback
const char* answerSymbol(JsonVariantConst value, const char* fallback) {
  if (value.is<JsonObjectConst>()) {
    return value["symbol"] | value["fallbackSymbol"] | fallback;
  }
  return fallback;
}

const char* appTypeName(const std::string& type) {
  if (type == "coin_flip") return "Coin Flip";
  if (type == "random_answer") return "Oracle";
  if (type == "random_draw") return "Random Draw";
  return "Simple App";
}

}  // namespace

void SimpleAppsActivity::onEnter() {
  Activity::onEnter();
  seed = esp_random();
  selectedApp = 0;
  selectedAction = 0;
  selectedResult = 0;
  mode = SimpleAppsMode::AppList;
  loadApps();
  requestUpdate();
}

uint32_t SimpleAppsActivity::nextRandom() {
  seed = 1664525UL * seed + 1013904223UL;
  return seed;
}

int SimpleAppsActivity::randomIndex(int maxExclusive) {
  if (maxExclusive <= 0) return 0;
  return static_cast<int>(nextRandom() % static_cast<uint32_t>(maxExclusive));
}

bool SimpleAppsActivity::randomBool() {
  return (nextRandom() & 1U) != 0U;
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  File root = SD.open(APPS_DIR);
  if (!root || !root.isDirectory()) return;

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      std::string filename = file.name();
      if (endsWith(filename, SIMPLEAPP_SUFFIX)) {
        std::string path = filename;
        if (path.rfind("/", 0) != 0) path = std::string(APPS_DIR) + "/" + path;

        SimpleAppEntry entry;
        entry.path = path;
        entry.name = filename;

        JsonDocument doc;
        if (!deserializeJson(doc, file)) {
          entry.name = safeString(doc["name"], filename.c_str());
          entry.type = safeString(doc["type"], "unknown");
          apps.push_back(entry);
        }
      }
    }
    file = root.openNextFile();
  }

  std::sort(apps.begin(), apps.end(),
            [](const SimpleAppEntry& a, const SimpleAppEntry& b) {
              return a.name < b.name;
            });

  if (selectedApp >= (int)apps.size()) selectedApp = 0;
}

bool SimpleAppsActivity::loadCurrentApp() {
  currentApp.clear();

  if (apps.empty()) return false;

  File file = SD.open(apps[selectedApp].path.c_str(), FILE_READ);
  if (!file) return false;

  return !deserializeJson(currentApp, file);
}

void SimpleAppsActivity::runSelectedAction() {
  drawResults.clear();
  selectedResult = 0;

  if (!loadCurrentApp()) return;

  std::string type = safeString(currentApp["type"], "");

  if (type == "coin_flip") {
    selectedResult = randomIndex(2);
    mode = SimpleAppsMode::Result;
    return;
  }

  if (type == "random_answer") {
    JsonArray answers = currentApp["answers"];
    selectedResult = randomIndex(answers.size());
    mode = SimpleAppsMode::Result;
    return;
  }
}

void SimpleAppsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode == SimpleAppsMode::AppList) finish();
    else mode = SimpleAppsMode::AppList;
    requestUpdate();
    return;
  }

  if (mode == SimpleAppsMode::AppList) {
    buttonNavigator.onNext([this] {
      if (!apps.empty()) {
        selectedApp = (selectedApp + 1) % apps.size();
        requestUpdate();
      }
    });

    buttonNavigator.onPrevious([this] {
      if (!apps.empty()) {
        selectedApp = (selectedApp - 1 + apps.size()) % apps.size();
        requestUpdate();
      }
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      mode = SimpleAppsMode::AppMenu;
      requestUpdate();
    }
  }

  if (mode == SimpleAppsMode::AppMenu) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      runSelectedAction();
      requestUpdate();
    }
  }

  if (mode == SimpleAppsMode::Result) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      runSelectedAction();
      requestUpdate();
    }
  }
}

void SimpleAppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  if (mode == SimpleAppsMode::AppList) {
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Apps", true);

    if (apps.empty()) {
      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 120, "No apps found", true);
    } else {
      for (int i = 0; i < apps.size(); i++) {
        int y = 120 + i * 40;
        std::string line = (i == selectedApp ? "> " : "  ") + apps[i].name;
        renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, line.c_str(), true);
      }
    }
  }

  else if (mode == SimpleAppsMode::Result) {
    std::string type = safeString(currentApp["type"], "");

    if (type == "random_answer") {
      JsonArray answers = currentApp["answers"];
      JsonVariant answer = answers[selectedResult];

      const char* symbol = answerSymbol(answer, currentApp["symbol"] | "8");
      const char* text = answerText(answer);

      drawCardBox(200, 120, 200, 200, symbol, false);

      int y = 360;
      drawWrappedText(60, y, 480, text, 4);
    }
  }

  renderer.displayBuffer();
}

void SimpleAppsActivity::drawCardBox(int x, int y, int w, int h, const char* symbol, bool reversed) {
  renderer.drawRect(x, y, w, h, 3, true);

  int textWidth = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol, EpdFontFamily::BOLD);
  renderer.drawText(NOTOSANS_18_FONT_ID, x + (w - textWidth) / 2, y + h / 2 - 10,
                    symbol, true, EpdFontFamily::BOLD);
}

void SimpleAppsActivity::drawWrappedText(int x, int& y, int maxWidth, const char* text, int maxLines) {
  auto lines = renderer.wrappedText(UI_12_FONT_ID, text, maxWidth, maxLines);
  for (auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true);
    y += 18;
  }
}