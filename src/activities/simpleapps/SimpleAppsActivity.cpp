#include "SimpleAppsActivity.h"

#include <FS.h>
#include <SD.h>
#include <esp_system.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "EpdFontFamily.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"


namespace {
constexpr const char* APPS_DIR = "/apps";
constexpr const char* SUFFIX = ".simpleapp.json";

bool endsWith(const std::string& v, const std::string& s) {
  return v.size() >= s.size() && v.compare(v.size() - s.size(), s.size(), s) == 0;
}

std::string safeString(JsonVariantConst v, const char* f = "") {
  return v.is<const char*>() ? v.as<const char*>() : f;
}

const char* answerText(JsonVariantConst v) {
  if (v.is<const char*>()) return v.as<const char*>();
  if (v.is<JsonObjectConst>()) return v["text"] | "No answer.";
  return "No answer.";
}

const char* answerSymbol(JsonVariantConst v, const char* f) {
  if (v.is<JsonObjectConst>()) return v["symbol"] | v["fallbackSymbol"] | f;
  return f;
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
  return maxExclusive <= 0 ? 0 : static_cast<int>(nextRandom() % static_cast<uint32_t>(maxExclusive));
}

bool SimpleAppsActivity::randomBool() {
  return (nextRandom() & 1U) != 0U;
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  fs::File root = SD.open(APPS_DIR);
  if (!root || !root.isDirectory()) return;

  fs::File file = root.openNextFile();

  while (file) {
    if (!file.isDirectory()) {
      std::string filename = file.name();

      if (endsWith(filename, SUFFIX)) {
        std::string path = filename;
        if (path.rfind("/", 0) != 0) {
          path = std::string(APPS_DIR) + "/" + path;
        }

        JsonDocument doc;
        if (!deserializeJson(doc, file)) {
          SimpleAppEntry e;
          e.path = path;
          e.name = safeString(doc["name"], filename.c_str());
          e.type = safeString(doc["type"], "unknown");
          apps.push_back(e);
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

  fs::File file = SD.open(apps[selectedApp].path.c_str(), FILE_READ);
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
    JsonArrayConst answers = currentApp["answers"].as<JsonArrayConst>();
    selectedResult = randomIndex(static_cast<int>(answers.size()));
    mode = SimpleAppsMode::Result;
    return;
  }

  if (type == "daily_combo") {
    mode = SimpleAppsMode::Result;
    return;
  }

  if (type == "random_draw") {
    JsonArrayConst items = currentApp["items"].as<JsonArrayConst>();
    JsonArrayConst modes = currentApp["drawModes"].as<JsonArrayConst>();

    int count = 1;
    if (!modes.isNull() && selectedAction < static_cast<int>(modes.size())) {
      count = modes[selectedAction]["count"] | 1;
    }

    bool allowRev = currentApp["allowReversed"] | false;
    std::vector<int> used;

    for (int i = 0; i < count && i < static_cast<int>(items.size()); ++i) {
      int idx = randomIndex(static_cast<int>(items.size()));
      while (std::find(used.begin(), used.end(), idx) != used.end()) {
        idx = randomIndex(static_cast<int>(items.size()));
      }

      used.push_back(idx);
      drawResults.push_back({idx, allowRev && randomBool()});
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

    if (mode == SimpleAppsMode::Result && safeString(currentApp["type"], "") == "random_draw") {
      mode = SimpleAppsMode::AppMenu;
    } else {
      mode = SimpleAppsMode::AppList;
    }

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

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !apps.empty() && loadCurrentApp()) {
      selectedAction = 0;

      if (safeString(currentApp["type"], "") == "random_draw") {
        mode = SimpleAppsMode::AppMenu;
      } else {
        runSelectedAction();
      }

      requestUpdate();
    }

    return;
  }

  if (mode == SimpleAppsMode::AppMenu) {
    int count = 1;

    if (safeString(currentApp["type"], "") == "random_draw") {
      JsonArrayConst modes = currentApp["drawModes"].as<JsonArrayConst>();
      count = std::max(1, static_cast<int>(modes.size()));
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      selectedAction = ButtonNavigator::previousIndex(selectedAction, count);
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      selectedAction = ButtonNavigator::nextIndex(selectedAction, count);
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      runSelectedAction();
      requestUpdate();
    }

    return;
  }

  if (mode == SimpleAppsMode::Result && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    runSelectedAction();
    requestUpdate();
  }
}

void SimpleAppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  if (mode == SimpleAppsMode::AppList) {
    renderAppList();
  } else if (mode == SimpleAppsMode::AppMenu) {
    renderAppMenu();
  } else {
    renderResult();
  }

  renderer.displayBuffer();
}

void SimpleAppsActivity::renderAppList() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, 8, w, 60}, "Apps");

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 140, "No apps found", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, 178, "Upload .simpleapp.json files to /apps", true);
    return;
  }

  std::vector<std::string> names;
  std::vector<UIIcon> icons;

  for (auto& a : apps) {
    names.push_back(a.name);
    icons.push_back(Settings);
  }

  GUI.drawButtonMenu(
      renderer, Rect{0, 90, w, h - 150}, static_cast<int>(names.size()), selectedApp,
      [&names](int i) { return names[i]; }, [&icons](int i) { return icons[i]; });

  auto labels = mappedInput.mapLabels("", "Open", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SimpleAppsActivity::renderAppMenu() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  std::string name = safeString(currentApp["name"], "Simple App");
  GUI.drawHeader(renderer, Rect{0, 8, w, 60}, name.c_str());

  JsonArrayConst modes = currentApp["drawModes"].as<JsonArrayConst>();

  if (!modes.isNull() && modes.size() > 0) {
    std::string modeName = safeString(modes[selectedAction]["name"], "Draw");

    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 130, modeName.c_str(), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, 170, "Left/Right: choose spread", true);
    renderer.drawCenteredText(UI_12_FONT_ID, 202, "Confirm: draw", true);
  } else {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 150, "Draw", true, EpdFontFamily::BOLD);
  }

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: run   Back: Apps", true);
}

void SimpleAppsActivity::renderResult() {
  std::string type = safeString(currentApp["type"], "");

  if (type == "coin_flip") {
    renderCoinFlipResult();
  } else if (type == "random_answer") {
    renderRandomAnswerResult();
  } else if (type == "random_draw") {
    renderRandomDrawResult();
  } else if (type == "daily_combo") {
    renderDailyComboResult();
  } else {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 150, "Unsupported app type", true, EpdFontFamily::BOLD);
  }
}

void SimpleAppsActivity::renderCoinFlipResult() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  JsonArrayConst opts = currentApp["options"].as<JsonArrayConst>();

  const char* label = selectedResult == 0 ? "HEADS" : "TAILS";
  const char* symbol = selectedResult == 0 ? "H" : "T";

  if (!opts.isNull() && opts.size() >= 2) {
    label = opts[selectedResult]["label"] | label;
    symbol = opts[selectedResult]["symbol"] | opts[selectedResult]["fallbackSymbol"] | symbol;
  }

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, safeString(currentApp["name"], "Coin Flip").c_str(), true,
                            EpdFontFamily::BOLD);

  drawCardBox(w / 2 - 80, 120, 160, 160, symbol, false);
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 330, label, true, EpdFontFamily::BOLD);

  if (!opts.isNull() && opts.size() >= 2) {
    int y = 372;
    drawWrappedText(30, y, w - 60, opts[selectedResult]["meaning"] | "", 3);
  }

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: again   Back: Apps", true);
}

void SimpleAppsActivity::renderRandomAnswerResult() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  JsonArrayConst answers = currentApp["answers"].as<JsonArrayConst>();
  JsonVariantConst ans;

  if (!answers.isNull() && selectedResult < static_cast<int>(answers.size())) {
    ans = answers[selectedResult];
  }

  const char* symbol = answerSymbol(ans, currentApp["symbol"] | currentApp["fallbackSymbol"] | "*");
  const char* text = answerText(ans);

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, safeString(currentApp["name"], "Prompt").c_str(), true,
                            EpdFontFamily::BOLD);

  drawCardBox(w / 2 - 65, 92, 130, 130, symbol, false);

  int y = 255;
  drawWrappedText(34, y, w - 68, text, 8);

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: again   Back: Apps", true);
}

void SimpleAppsActivity::renderRandomDrawResult() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 18, safeString(currentApp["name"], "Tarot").c_str(), true,
                            EpdFontFamily::BOLD);

  JsonArrayConst items = currentApp["items"].as<JsonArrayConst>();
  JsonArrayConst modes = currentApp["drawModes"].as<JsonArrayConst>();

  JsonArrayConst labels;
  if (!modes.isNull() && selectedAction < static_cast<int>(modes.size())) {
    labels = modes[selectedAction]["labels"].as<JsonArrayConst>();
  }

  if (drawResults.size() == 1) {
    JsonObjectConst item = items[drawResults[0].itemIndex];

    drawCardBox(w / 2 - 95, 90, 190, 245, item["symbol"] | item["fallbackSymbol"] | "?", drawResults[0].reversed);

    int y = 370;
    char header[140];

    snprintf(header, sizeof(header), "%s - %s", item["name"] | "Card",
             drawResults[0].reversed ? "Reversed" : "Upright");

    renderer.drawText(NOTOSANS_16_FONT_ID, 24, y, header, true, EpdFontFamily::BOLD);
    y += 36;

    drawWrappedText(30, y, w - 60, drawResults[0].reversed ? (item["reversed"] | "") : (item["upright"] | ""), 5);
  } else {
    int cw = 122;
    int ch = 145;
    int top = 80;
    int xs[3] = {22, 179, 336};

    for (int i = 0; i < static_cast<int>(drawResults.size()) && i < 3; i++) {
      JsonObjectConst item = items[drawResults[i].itemIndex];

      const char* label = "Card";
      if (!labels.isNull() && i < static_cast<int>(labels.size())) label = labels[i] | label;

      int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
      renderer.drawText(UI_12_FONT_ID, xs[i] + (cw - lw) / 2, top - 22, label, true);

      drawCardBox(xs[i], top, cw, ch, item["symbol"] | item["fallbackSymbol"] | "?", drawResults[i].reversed);
    }

    int y = top + ch + 28;

    for (int i = 0; i < static_cast<int>(drawResults.size()) && i < 3; i++) {
      JsonObjectConst item = items[drawResults[i].itemIndex];

      const char* label = "Card";
      if (!labels.isNull() && i < static_cast<int>(labels.size())) label = labels[i] | label;

      char header[160];
      snprintf(header, sizeof(header), "%s: %s - %s", label, item["name"] | "Card",
               drawResults[i].reversed ? "Reversed" : "Upright");

      renderer.drawText(UI_12_FONT_ID, 20, y, header, true, EpdFontFamily::BOLD);
      y += 23;

      drawWrappedText(34, y, w - 68, drawResults[i].reversed ? (item["reversed"] | "") : (item["upright"] | ""), 2);
      y += 10;
    }
  }

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: redraw   Back: menu", true);
}

void SimpleAppsActivity::renderDailyComboResult() {
  int w = renderer.getScreenWidth();
  int h = renderer.getScreenHeight();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 24, safeString(currentApp["name"], "Daily").c_str(), true,
                            EpdFontFamily::BOLD);

  JsonArrayConst prompts = currentApp["prompts"].as<JsonArrayConst>();
  JsonArrayConst goals = currentApp["goals"].as<JsonArrayConst>();
  JsonArrayConst grat = currentApp["gratitude"].as<JsonArrayConst>();

  const char* p = "What matters today?";
  const char* g = "Do one small thing.";
  const char* t = "Notice one good thing.";

  if (!prompts.isNull() && prompts.size() > 0) {
    p = answerText(prompts[randomIndex(static_cast<int>(prompts.size()))]);
  }

  if (!goals.isNull() && goals.size() > 0) {
    g = answerText(goals[randomIndex(static_cast<int>(goals.size()))]);
  }

  if (!grat.isNull() && grat.size() > 0) {
    t = answerText(grat[randomIndex(static_cast<int>(grat.size()))]);
  }

  int y = 90;

  renderer.drawText(NOTOSANS_16_FONT_ID, 24, y, "Prompt", true, EpdFontFamily::BOLD);
  y += 28;
  drawWrappedText(36, y, w - 72, p, 3);
  y += 18;

  renderer.drawText(NOTOSANS_16_FONT_ID, 24, y, "Goal", true, EpdFontFamily::BOLD);
  y += 28;
  drawWrappedText(36, y, w - 72, g, 3);
  y += 18;

  renderer.drawText(NOTOSANS_16_FONT_ID, 24, y, "Gratitude", true, EpdFontFamily::BOLD);
  y += 28;
  drawWrappedText(36, y, w - 72, t, 3);

  renderer.drawCenteredText(UI_12_FONT_ID, h - 45, "Confirm: new day   Back: Apps", true);
}

void SimpleAppsActivity::drawCardBox(int x, int y, int w, int h, const char* symbol, bool reversed) {
  renderer.drawRect(x, y, w, h, 3, true);
  renderer.drawRect(x + 8, y + 8, w - 16, h - 16, true);

  int tw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol, EpdFontFamily::BOLD);

  renderer.drawText(NOTOSANS_18_FONT_ID, x + (w - tw) / 2, y + h / 2 - 15, symbol, true, EpdFontFamily::BOLD);

  if (reversed) {
    const char* m = currentApp["ui"]["reversedMarker"] | "v";
    int mw = renderer.getTextWidth(NOTOSANS_16_FONT_ID, m, EpdFontFamily::BOLD);

    renderer.drawText(NOTOSANS_16_FONT_ID, x + (w - mw) / 2, y + h - 40, m, true, EpdFontFamily::BOLD);
  }
}

void SimpleAppsActivity::drawWrappedText(int x, int& y, int maxWidth, const char* text, int maxLines) {
  const auto lines = renderer.wrappedText(UI_12_FONT_ID, text, maxWidth, maxLines);

  for (const auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 3;
  }
}