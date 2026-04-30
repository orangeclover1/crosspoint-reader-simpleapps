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

const char* appTypeName(const std::string& type) {
  if (type == "coin_flip") return "Coin Flip";
  if (type == "random_answer") return "Oracle";
  if (type == "random_draw") return "Random Draw";
  return "Simple App";
}

const char* answerText(JsonVariantConst value) {
  if (value.is<const char*>()) return value.as<const char*>();
  if (value.is<JsonObjectConst>()) return value["text"] | "No answer.";
  return "No answer.";
}

const char* answerSymbol(JsonVariantConst value, const char* fallback) {
  if (value.is<JsonObjectConst>()) return value["symbol"] | value["fallbackSymbol"] | fallback;
  return fallback;
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
        entry.type = "unknown";

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        if (!error) {
          entry.name = safeString(doc["name"], filename.c_str());
          entry.type = safeString(doc["type"], "unknown");
          apps.push_back(entry);
        }
      }
    }
    file = root.openNextFile();
  }

  std::sort(apps.begin(), apps.end(), [](const SimpleAppEntry& a, const SimpleAppEntry& b) {
    return a.name < b.name;
  });

  if (selectedApp >= static_cast<int>(apps.size())) selectedApp = 0;
}

bool SimpleAppsActivity::loadCurrentApp() {
  currentApp.clear();

  if (apps.empty() || selectedApp < 0 || selectedApp >= static_cast<int>(apps.size())) return false;

  File file = SD.open(apps[selectedApp].path.c_str(), FILE_READ);
  if (!file) return false;

  DeserializationError error = deserializeJson(currentApp, file);
  return !error;
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
    JsonArrayConst answers = currentApp["answers"].as<JsonArrayConst>();
    selectedResult = randomIndex(static_cast<int>(answers.size()));
    mode = SimpleAppsMode::Result;
    return;
  }

  if (type == "random_draw") {
    JsonArrayConst items = currentApp["items"].as<JsonArrayConst>();
    JsonArrayConst drawModes = currentApp["drawModes"].as<JsonArrayConst>();

    int drawCount = 1;
    if (!drawModes.isNull() && selectedAction < static_cast<int>(drawModes.size())) {
      drawCount = drawModes[selectedAction]["count"] | 1;
    }

    bool allowReversed = currentApp["allowReversed"] | false;
    std::vector<int> used;

    for (int i = 0; i < drawCount && i < static_cast<int>(items.size()); ++i) {
      int index = randomIndex(static_cast<int>(items.size()));
      while (std::find(used.begin(), used.end(), index) != used.end()) {
        index = randomIndex(static_cast<int>(items.size()));
      }
      used.push_back(index);

      SimpleDrawResult result;
      result.itemIndex = index;
      result.reversed = allowReversed && randomBool();
      drawResults.push_back(result);
    }

    mode = SimpleAppsMode::Result;
  }
}

void SimpleAppsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode == SimpleAppsMode::AppList) {
      finish();
      return;
    }
    if (mode == SimpleAppsMode::Result) {
      mode = SimpleAppsMode::AppMenu;
      requestUpdate();
      return;
    }
    mode = SimpleAppsMode::AppList;
    requestUpdate();
    return;
  }

  if (mode == SimpleAppsMode::AppList) {
    buttonNavigator.onNext([this] {
      if (!apps.empty()) {
        selectedApp = ButtonNavigator::nextIndex(selectedApp, static_cast<int>(apps.size()));
        requestUpdate();
      }
    });

    buttonNavigator.onPrevious([this] {
      if (!apps.empty()) {
        selectedApp = ButtonNavigator::previousIndex(selectedApp, static_cast<int>(apps.size()));
        requestUpdate();
      }
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!apps.empty() && loadCurrentApp()) {
        selectedAction = 0;
        mode = SimpleAppsMode::AppMenu;
        requestUpdate();
      }
    }
    return;
  }

  if (mode == SimpleAppsMode::AppMenu) {
    std::string type = safeString(currentApp["type"], "");
    int actionCount = 1;

    if (type == "random_draw") {
      JsonArrayConst drawModes = currentApp["drawModes"].as<JsonArrayConst>();
      actionCount = std::max(1, static_cast<int>(drawModes.size()));
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      selectedAction = ButtonNavigator::previousIndex(selectedAction, actionCount);
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      selectedAction = ButtonNavigator::nextIndex(selectedAction, actionCount);
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      runSelectedAction();
      requestUpdate();
    }
    return;
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

  switch (mode) {
    case SimpleAppsMode::AppList: renderAppList(); break;
    case SimpleAppsMode::AppMenu: renderAppMenu(); break;
    case SimpleAppsMode::Result: renderResult(); break;
  }

  renderer.displayBuffer();
}

void SimpleAppsActivity::renderAppList() {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, 8, pageWidth, 60}, "Apps");

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 140, "No apps found", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, 178, "Upload .simpleapp.json files to /apps", true);
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight - 45, "Back: Home", true);
    return;
  }

  std::vector<std::string> names;
  std::vector<UIIcon> icons;
  for (const auto& app : apps) {
    names.push_back(app.name);
    icons.push_back(Settings);
  }

  GUI.drawButtonMenu(
      renderer, Rect{0, 90, pageWidth, pageHeight - 150}, static_cast<int>(names.size()), selectedApp,
      [&names](int index) { return names[index]; }, [&icons](int index) { return icons[index]; });

  const auto labels = mappedInput.mapLabels("", "Open", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SimpleAppsActivity::renderAppMenu() {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  std::string name = safeString(currentApp["name"], "Simple App");
  std::string type = safeString(currentApp["type"], "");

  GUI.drawHeader(renderer, Rect{0, 8, pageWidth, 60}, name.c_str());

  if (type == "random_draw") {
    JsonArrayConst drawModes = currentApp["drawModes"].as<JsonArrayConst>();
    if (!drawModes.isNull() && !drawModes.isEmpty()) {
      JsonObjectConst modeObj = drawModes[selectedAction];
      std::string modeName = safeString(modeObj["name"], "Draw");
      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 130, modeName.c_str(), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_12_FONT_ID, 170, "Left/Right: choose spread", true);
      renderer.drawCenteredText(UI_12_FONT_ID, 202, "Confirm: draw", true);
    } else {
      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 150, "Draw", true, EpdFontFamily::BOLD);
    }
  } else {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 130, appTypeName(type), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, 175, "Confirm: run", true);
  }

  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight - 45, "Confirm: run   Back: Apps", true);
}

void SimpleAppsActivity::renderResult() {
  std::string type = safeString(currentApp["type"], "");

  if (type == "coin_flip") renderCoinFlipResult();
  else if (type == "random_answer") renderRandomAnswerResult();
  else if (type == "random_draw") renderRandomDrawResult();
  else renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 150, "Unsupported app type", true, EpdFontFamily::BOLD);
}

void SimpleAppsActivity::renderCoinFlipResult() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();

  JsonArrayConst options = currentApp["options"].as<JsonArrayConst>();
  const char* label = selectedResult == 0 ? "HEADS" : "TAILS";
  const char* symbol = selectedResult == 0 ? "H" : "T";

  if (!options.isNull() && options.size() >= 2) {
    label = options[selectedResult]["label"] | label;
    symbol = options[selectedResult]["symbol"] | options[selectedResult]["fallbackSymbol"] | symbol;
  }

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, safeString(currentApp["name"], "Coin Flip").c_str(), true, EpdFontFamily::BOLD);
  drawCardBox(w / 2 - 80, 120, 160, 160, symbol, false);
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 330, label, true, EpdFontFamily::BOLD);

  if (!options.isNull() && options.size() >= 2) {
    int y = 372;
    drawWrappedText(30, y, w - 60, options[selectedResult]["meaning"] | "", 3);
  }

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: again   Back: menu", true);
}

void SimpleAppsActivity::renderRandomAnswerResult() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();

  JsonArrayConst answers = currentApp["answers"].as<JsonArrayConst>();
  JsonVariantConst answer;
  if (!answers.isNull() && selectedResult < static_cast<int>(answers.size())) answer = answers[selectedResult];

  const char* fallbackSymbol = currentApp["symbol"] | currentApp["fallbackSymbol"] | "8";
  const char* symbol = answerSymbol(answer, fallbackSymbol);
  const char* text = answerText(answer);

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, safeString(currentApp["name"], "Oracle").c_str(), true, EpdFontFamily::BOLD);
  drawCardBox(w / 2 - 90, 105, 180, 180, symbol, false);

  int y = 325;
  drawWrappedText(40, y, w - 80, text, 4);

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: ask again   Back: menu", true);
}

void SimpleAppsActivity::renderRandomDrawResult() {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 18, safeString(currentApp["name"], "Tarot").c_str(), true, EpdFontFamily::BOLD);

  JsonArrayConst items = currentApp["items"].as<JsonArrayConst>();
  JsonArrayConst drawModes = currentApp["drawModes"].as<JsonArrayConst>();

  JsonArrayConst labels;
  if (!drawModes.isNull() && selectedAction < static_cast<int>(drawModes.size())) {
    labels = drawModes[selectedAction]["labels"].as<JsonArrayConst>();
  }

  if (drawResults.size() == 1) {
    const JsonObjectConst item = items[drawResults[0].itemIndex];
    const char* symbol = item["symbol"] | item["fallbackSymbol"] | "?";
    drawCardBox(w / 2 - 95, 90, 190, 245, symbol, drawResults[0].reversed);

    int y = 370;
    char header[140];
    snprintf(header, sizeof(header), "%s - %s", item["name"] | "Card", drawResults[0].reversed ? "Reversed" : "Upright");
    renderer.drawText(NOTOSANS_16_FONT_ID, 24, y, header, true, EpdFontFamily::BOLD);
    y += 36;

    const char* meaning = drawResults[0].reversed ? (item["reversed"] | "") : (item["upright"] | "");
    drawWrappedText(30, y, w - 60, meaning, 4);
  } else {
    const int cardW = 122;
    const int cardH = 145;
    const int top = 80;
    const int xs[3] = {22, 179, 336};

    for (int i = 0; i < static_cast<int>(drawResults.size()) && i < 3; ++i) {
      const JsonObjectConst item = items[drawResults[i].itemIndex];
      const char* label = "Card";
      if (!labels.isNull() && i < static_cast<int>(labels.size())) label = labels[i] | label;

      const int labelWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
      renderer.drawText(UI_12_FONT_ID, xs[i] + (cardW - labelWidth) / 2, top - 22, label, true);
      drawCardBox(xs[i], top, cardW, cardH, item["symbol"] | item["fallbackSymbol"] | "?", drawResults[i].reversed);
    }

    int y = top + cardH + 28;
    for (int i = 0; i < static_cast<int>(drawResults.size()) && i < 3; ++i) {
      const JsonObjectConst item = items[drawResults[i].itemIndex];
      const char* label = "Card";
      if (!labels.isNull() && i < static_cast<int>(labels.size())) label = labels[i] | label;

      char header[160];
      snprintf(header, sizeof(header), "%s: %s - %s", label, item["name"] | "Card", drawResults[i].reversed ? "Reversed" : "Upright");
      renderer.drawText(UI_12_FONT_ID, 20, y, header, true, EpdFontFamily::BOLD);
      y += 23;

      const char* meaning = drawResults[i].reversed ? (item["reversed"] | "") : (item["upright"] | "");
      drawWrappedText(34, y, w - 68, meaning, 2);
      y += 10;
    }
  }

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: redraw   Back: menu", true);
}

void SimpleAppsActivity::drawCardBox(int x, int y, int w, int h, const char* symbol, bool reversed) {
  renderer.drawRect(x, y, w, h, 3, true);
  renderer.drawRect(x + 8, y + 8, w - 16, h - 16, true);

  const int textWidth = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol, EpdFontFamily::BOLD);
  renderer.drawText(NOTOSANS_18_FONT_ID, x + (w - textWidth) / 2, y + h / 2 - 15, symbol, true, EpdFontFamily::BOLD);

  if (reversed) {
    const char* marker = currentApp["ui"]["reversedMarker"] | "v";
    const int markerWidth = renderer.getTextWidth(NOTOSANS_16_FONT_ID, marker, EpdFontFamily::BOLD);
    renderer.drawText(NOTOSANS_16_FONT_ID, x + (w - markerWidth) / 2, y + h - 40, marker, true, EpdFontFamily::BOLD);
  }
}

void SimpleAppsActivity::drawWrappedText(int x, int& y, int maxWidth, const char* text, int maxLines) {
  const auto lines = renderer.wrappedText(UI_12_FONT_ID, text, maxWidth, maxLines);
  for (const auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 3;
  }
}
