#include "SimpleAppsActivity.h"


#include <FS.h>
#include <SD.h>
#include <ArduinoJson.hpp>
#include <HalStorage.h>
#include "components/UITheme.h"

#include "fontIds.h"

using namespace ArduinoJson;


static int spreadIndex = 0;

void SimpleAppsActivity::onEnter() {
  Activity::onEnter();
  loadApps();
  requestUpdate();
}

void SimpleAppsActivity::loadApps() {
  apps.clear();

  auto dir = Storage.open("/apps");

  // Safety check
  if (!dir || !dir.isDirectory()) {
    return;
  }

  char name[256];

  // Iterate files safely
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {

    if (!file) continue;
    if (file.isDirectory()) continue;

    file.getName(name, sizeof(name));

    std::string filename(name);

    // Skip hidden/system files
    if (!filename.empty() && filename[0] == '.') {
      continue;
    }

    // Only accept .simpleapp.json
    if (filename.find(".simpleapp.json") != std::string::npos) {
      apps.push_back(filename);
    }
  }

  // Optional: sort alphabetically
  std::sort(apps.begin(), apps.end());
}



bool SimpleAppsActivity::loadApp(const std::string& filename) {
  currentApp.clear();

  FsFile file;

  std::string fullPath = "/apps/" + filename;

  if (!Storage.openFileForRead("APP", fullPath, file)) {
    return false;
  }

  DeserializationError err = deserializeJson(currentApp, file);
  return !err;
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

  // switch spreads
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    spreadIndex = (spreadIndex - 1 + 4) % 4;
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    spreadIndex = (spreadIndex + 1) % 4;
    requestUpdate();
  }


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
    renderer.drawCenteredText(UI_10_FONT_ID, 75, "<  > to change", true);

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

      int y = boxY + boxH + 40;

      auto lines = renderer.wrappedText(UI_12_FONT_ID, meaning, screenW - 40, 5);

      for (auto& line : lines) {
        renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
        y += 24;
      }
    }

    // =========================
    // THREE CARD SPREAD
    // =========================
    else {

      int cardW = 120;
      int cardH = 140;
      int spacing = 12;

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

      // ===== MEANINGS (IMPROVED) =====
      int y = topY + cardH + 30;

      for (int i = 0; i < drawCount; i++) {

        JsonObject card = cards[i];
        bool isReversed = reversedFlags[i];

        const char* name = card["name"];
        const char* meaning = isReversed ? card["reversed"] : card["upright"];

        // ===== HEADER =====
        std::string header;

        if (drawCount == 1) {
          header = name;
        } else {
          header = std::string(spreads[spreadIndex][i]) + ": " + name;
        }

        renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);
        y += 24;

        // ===== KEYWORDS (optional but nice) =====
        if (card.containsKey("keywords")) {
          JsonArray kws = card["keywords"];

          std::string kwLine = "";
          for (int k = 0; k < kws.size(); k++) {
            kwLine += kws[k].as<const char*>();
            if (k < kws.size() - 1) kwLine += ", ";
          }

          renderer.drawText(UI_10_FONT_ID, 30, y, kwLine.c_str(), true);
          y += 18;
        }

        // ===== MEANING TEXT =====
        auto lines = renderer.wrappedText(
          UI_12_FONT_ID,
          meaning,
          renderer.getScreenWidth() - 40,
          (drawCount == 1) ? 5 : 2   // more space for single card
        );

        for (auto& line : lines) {
          renderer.drawText(UI_12_FONT_ID, 30, y, line.c_str(), true);
          y += 20;
        }

        // ===== DIVIDER =====
        if (i < drawCount - 1) {
          y += 6;
          renderer.drawLine(20, y, renderer.getScreenWidth() - 20, y);
          y += 12;
        } else {
          y += 16;
        }
      }
    }
    
    
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

  int screenW = renderer.getScreenWidth();

  // ===== HEADER =====
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Apps", true);

  if (apps.empty()) {
    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 120, "No apps found", true);
  } else {

    int startY = 100;
    int itemHeight = 36;

    for (int i = 0; i < apps.size(); i++) {

      int y = startY + i * itemHeight;

      std::string display = apps[i];
      size_t pos = display.find(".simpleapp.json");
      if (pos != std::string::npos) {
        display = display.substr(0, pos);
      }

      if (i == selected) {
        renderer.drawRect(20, y - 6, screenW - 40, 30, 1, true);

        std::string label = "▶ " + display;
        renderer.drawText(NOTOSANS_16_FONT_ID, 30, y, label.c_str(), true);
      } else {
        renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, display.c_str(), true);
      }
    }
  }

  auto labels = mappedInput.mapLabels("", "Open", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}