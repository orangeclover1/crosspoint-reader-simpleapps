#include "SimpleAppsActivity.h"
#include "storage/FileSystem.h"

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

  auto files = FileSystem::listDir("/apps");

  for (auto& file : files) {
    std::string name = file.name;

    if (name.find(".simpleapp.json") != std::string::npos) {
      apps.push_back(name);
    }
  }
}

bool SimpleAppsActivity::loadApp(const std::string& path) {
  currentApp.clear();

  auto file = FileSystem::openFile(path.c_str());

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
    JsonArray modes = currentApp["drawModes"];

    bool reversedEnabled = currentApp["allowReversed"] | false;

    if (items.size() == 0) return;

    // Default = single card
    int drawCount = 1;

    if (modes.size() > 0) {
      drawCount = modes[0]["count"] | 1;
    }

    // If more than 1 mode, use second (3-card)
    if (modes.size() > 1) {
      drawCount = modes[1]["count"] | 3;
    }

    // ===== DRAW UNIQUE CARDS =====
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

    int screenW = renderer.getScreenWidth();

    // =========================
    // SINGLE CARD (unchanged)
    // =========================
    if (drawCount == 1) {

      JsonObject card = cards[0];
      bool isReversed = reversedFlags[0];

      const char* name = card["name"];
      const char* symbol = card["symbol"];
      const char* meaning = isReversed ? card["reversed"] : card["upright"];

      renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, name, true);

      int boxW = 160;
      int boxH = 200;
      int boxX = (screenW - boxW) / 2;
      int boxY = 80;

      renderer.drawRect(boxX, boxY, boxW, boxH, 2, true);
      renderer.drawRect(boxX + 8, boxY + 8, boxW - 16, boxH - 16, true);

      int symbolWidth = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
      renderer.drawText(NOTOSANS_18_FONT_ID, boxX + (boxW - symbolWidth) / 2, boxY + 70, symbol, true);

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

      const char* labels[3] = { "Past", "Present", "Future" };

      int cardW = 120;
      int cardH = 140;
      int spacing = 20;

      int totalWidth = (cardW * 3) + (spacing * 2);
      int startX = (screenW - totalWidth) / 2;

      int topY = 90;

      // ===== DRAW CARDS =====
      for (int i = 0; i < 3; i++) {

        int x = startX + i * (cardW + spacing);

        JsonObject card = cards[i];
        bool isReversed = reversedFlags[i];

        const char* symbol = card["symbol"];

        // Label
        renderer.drawCenteredText(UI_12_FONT_ID, topY - 20 + i * 0, labels[i], true);

        // Box
        renderer.drawRect(x, topY, cardW, cardH, 2, true);
        renderer.drawRect(x + 6, topY + 6, cardW - 12, cardH - 12, true);

        // Symbol
        int sw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
        renderer.drawText(NOTOSANS_18_FONT_ID, x + (cardW - sw) / 2, topY + 50, symbol, true);

        // Reversed marker
        if (isReversed) {
          renderer.drawCenteredText(NOTOSANS_12_FONT_ID, topY + cardH - 25, "v", true);
        }
      }

      // ===== MEANINGS =====
      int y = topY + cardH + 30;

      for (int i = 0; i < 3; i++) {

        JsonObject card = cards[i];
        bool isReversed = reversedFlags[i];

        const char* name = card["name"];
        const char* meaning = isReversed ? card["reversed"] : card["upright"];

        std::string header = std::string(labels[i]) + ": " + name;

        renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);
        y += 25;

        auto lines = renderer.wrappedText(UI_12_FONT_ID, meaning, screenW - 40, 2);

        for (auto& line : lines) {
          renderer.drawText(UI_12_FONT_ID, 30, y, line.c_str(), true);
          y += 20;
        }

        y += 10;
      }
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