#include "SimpleAppsActivity.h"


#include <FS.h>
#include <SD.h>
#include <ArduinoJson.hpp>
#include <HalStorage.h>

#include "fontIds.h"

using namespace ArduinoJson;

static int tarotMode = 0; // 0 = single, 1 = three-card

void SimpleAppsActivity::onEnter() {
  Activity::onEnter();
  loadApps();
  requestUpdate();
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  auto dir = Storage.open("/apps");

  if (dir && dir.isDirectory()) {

    char name[256];

    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {

      if (file.isDirectory()) continue;

      file.getName(name, sizeof(name));

      std::string filename(name);

      if (filename.find(".simpleapp.json") != std::string::npos) {
        std::string clean = filename;

        // remove extension
        size_t pos = clean.find(".simpleapp.json");
        if (pos != std::string::npos) {
          clean = clean.substr(0, pos);
        }

        // capitalize first letter
        if (!clean.empty()) {
          clean[0] = toupper(clean[0]);
        }

        apps.push_back(clean);;
      }
    }
  }
}

bool SimpleAppsActivity::loadApp(const std::string& filename) {
  currentApp.clear();

  FsFile file;

  std::string fullPath = "/apps/" + filename;

  if (!Storage.openFileForRead("APP", fullPath, file)) {
    return false;
  }

  return !deserializeJson(currentApp, file);
}

void SimpleAppsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  return;
  }
  
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

  if (!loadApp(apps[selected])) return;

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

    int screenW = renderer.getScreenWidth();

    // =========================
    // SPREAD OPTIONS
    // =========================
    static int spreadIndex = 0;

    const char* spreadNames[] = {
      "Single Card",
      "Past / Present / Future",
      "You / Relationship / Partner",
      "Mind / Body / Spirit"
    };

    const char* spreads[][3] = {
      { "", "", "" },
      { "Past", "Present", "Future" },
      { "You", "Relationship", "Partner" },
      { "Mind", "Body", "Spirit" }
    };

    int spreadCount = 4;

    // switch spreads
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      spreadIndex = (spreadIndex - 1 + spreadCount) % spreadCount;
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      spreadIndex = (spreadIndex + 1) % spreadCount;
      requestUpdate();
    }

    int drawCount = (spreadIndex == 0) ? 1 : 3;

    // =========================
    // DRAW CARDS
    // =========================
    std::vector<int> used;
    std::vector<JsonObject> cards;
    std::vector<bool> reversedFlags;

    for (int i = 0; i < drawCount; i++) {
      int idx = random(items.size());

      while (std::find(used.begin(), used.end(), idx) != used.end()) {
        idx = random(items.size());
      }

      used.push_back(idx);
      cards.push_back(items[idx]);
      reversedFlags.push_back(reversedEnabled && random(2) == 1);
    }

    renderer.clearScreen();

    // =========================
    // HEADER
    // =========================
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 25, "Tiny Tarot", true);
    renderer.drawCenteredText(UI_12_FONT_ID, 55, spreadNames[spreadIndex], true);

    // =========================
    // SINGLE CARD
    // =========================
    if (drawCount == 1) {

      JsonObject card = cards[0];
      bool isReversed = reversedFlags[0];

      const char* name = card["name"];
      const char* symbol = card["fallbackSymbol"] | card["symbol"];
      const char* meaning = isReversed ? card["reversed"] : card["upright"];

      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 90, name, true);

      int boxW = 180;
      int boxH = 220;
      int boxX = (screenW - boxW) / 2;
      int boxY = 120;

      renderer.drawRect(boxX, boxY, boxW, boxH, 2, true);
      renderer.drawRect(boxX + 10, boxY + 10, boxW - 20, boxH - 20, true);

      int sw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
      renderer.drawText(NOTOSANS_18_FONT_ID, boxX + (boxW - sw) / 2, boxY + 80, symbol, true);

      if (isReversed) {
        renderer.drawCenteredText(NOTOSANS_12_FONT_ID, boxY + boxH - 30, "Reversed", true);
      }

      int y = boxY + boxH + 30;

      auto lines = renderer.wrappedText(UI_12_FONT_ID, meaning, screenW - 40, 5);

      for (auto& line : lines) {
        renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
        y += 22;
      }
    }

    // =========================
    // THREE CARD SPREAD
    // =========================
    else {

      int cardW = 110;
      int cardH = 140;
      int spacing = 18;

      int totalW = (cardW * 3) + (spacing * 2);
      int startX = (screenW - totalW) / 2;
      int topY = 100;

      // ===== DRAW CARDS =====
      for (int i = 0; i < 3; i++) {

        int x = startX + i * (cardW + spacing);

        JsonObject card = cards[i];
        bool isReversed = reversedFlags[i];

        const char* symbol = card["fallbackSymbol"] | card["symbol"];
        const char* label = spreads[spreadIndex][i];

        // label
        int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
        renderer.drawText(UI_12_FONT_ID, x + (cardW - lw) / 2, topY - 18, label, true);

        // box
        renderer.drawRect(x, topY, cardW, cardH, 2, true);
        renderer.drawRect(x + 6, topY + 6, cardW - 12, cardH - 12, true);

        // symbol
        int sw = renderer.getTextWidth(NOTOSANS_16_FONT_ID, symbol);
        renderer.drawText(NOTOSANS_16_FONT_ID, x + (cardW - sw) / 2, topY + 55, symbol, true);

        if (isReversed) {
          renderer.drawText(NOTOSANS_12_FONT_ID, x + 45, topY + cardH - 20, "v", true);
        }
      }

      // ===== MEANINGS =====
      int y = topY + cardH + 25;

      for (int i = 0; i < 3; i++) {

        JsonObject card = cards[i];
        bool isReversed = reversedFlags[i];

        const char* name = card["name"];
        const char* meaning = isReversed ? card["reversed"] : card["upright"];

        std::string header = std::string(spreads[spreadIndex][i]) + ": " + name;

        renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);
        y += 24;

        auto lines = renderer.wrappedText(UI_12_FONT_ID, meaning, screenW - 40, 2);

        for (auto& line : lines) {
          renderer.drawText(UI_12_FONT_ID, 30, y, line.c_str(), true);
          y += 20;
        }

        y += 12;
      }
    }
    auto labels = mappedInput.mapLabels("", "Again", "Back", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
  }


  else {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 200, "Unsupported app", true);
  }
  auto labels = mappedInput.mapLabels("", "Confirm", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
}

void SimpleAppsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Apps", true);

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 120, "No apps found", true);
  } else {
    int screenW = renderer.getScreenWidth();

    // ===== HEADER =====
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Apps", true);

    // ===== LIST =====
    int startY = 100;
    int itemHeight = 36;

    for (int i = 0; i < apps.size(); i++) {

      int y = startY + i * itemHeight;

      const char* name = apps[i].c_str();

      // highlight selected
      if (i == selected) {
        renderer.drawRect(20, y - 6, screenW - 40, 30, 1, true);

        std::string label = "▶ " + apps[i];
        renderer.drawText(NOTOSANS_16_FONT_ID, 30, y, label.c_str(), true);
      } else {
        renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, name, true);
      }
    }
    
    }
  }

  auto labels = mappedInput.mapLabels("", "Open", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}