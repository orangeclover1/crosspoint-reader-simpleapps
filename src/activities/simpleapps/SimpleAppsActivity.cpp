#include "SimpleAppsActivity.h"


#include <FS.h>
#include <SD.h>
#include <ArduinoJson.hpp>
#include <HalStorage.h>
#include "components/UITheme.h"

#include "fontIds.h"

#include "images/AppIcons.h"

using namespace ArduinoJson;


static int spreadIndex = 0;
static bool inAppView = false;

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

// App Functions ==================

// COIN

if (type == "coin_flip") {

  bool heads = random(2) == 0;

  const unsigned char* icon = heads
    ? epd_bitmap_coin_heads
    : epd_bitmap_coin_tails;

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 40, "Coin Flip", true);

  renderer.drawImage(
    icon,
    (renderer.getScreenWidth() - 64) / 2,
    100,
    64,
    64
  );

  renderer.drawCenteredText(
    NOTOSANS_16_FONT_ID,
    200,
    heads ? "HEADS" : "TAILS",
    true
  );
}

// 8 BALL
else if (type == "random_answer") {

  JsonArray arr = currentApp["answers"];
  if (arr.size() == 0) return;

  int idx = random(arr.size());
  const char* txt = arr[idx];

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Magic 8 Ball", true);

  renderer.drawImage(
    epd_bitmap_8ball,
    (renderer.getScreenWidth() - 64) / 2,
    70,
    64,
    64
  );

  int y = 160;

  auto lines = renderer.wrappedText(
    UI_12_FONT_ID,
    txt,
    renderer.getScreenWidth() - 40,
    4
  );

  for (auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
    y += 22;
  }
}

// Daily Journal Prompt
void SimpleAppsActivity::renderDaily() {
  int screenW = renderer.getScreenWidth();

  JsonArray p = currentApp["prompts"];
  JsonArray g = currentApp["goals"];
  JsonArray t = currentApp["gratitude"];

  const char* pt = p[random(p.size())];
  const char* gt = g[random(g.size())];
  const char* tt = t[random(t.size())];

  renderer.drawImage(epd_bitmap_journal, 20, 30, 32, 32);
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Daily", true);

  int y = 90;

  auto drawSection = [&](const char* title, const char* text) {
    renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, title, true);
    y += 28;

    auto lines = renderer.wrappedText(UI_12_FONT_ID, text, screenW - 40, 3);
    for (auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
      y += 22;
    }
    y += 16;
  };

  drawSection("Prompt", pt);
  drawSection("Goal", gt);
  drawSection("Gratitude", tt);
}

//Tarot
void SimpleAppsActivity::renderTarot() {
  int screenW = renderer.getScreenWidth();

  JsonArray items = currentApp["items"];
  bool reversedEnabled = currentApp["allowReversed"] | false;

  const char* spreadNames[] = {
    "Single Card",
    "Past / Present / Future",
    "You / Relationship / Partner",
    "Mind / Body / Spirit"
  };

  const char* spreads[][3] = {
    {"","",""},
    {"Past","Present","Future"},
    {"You","Relationship","Partner"},
    {"Mind","Body","Spirit"}
  };

  int drawCount = (spreadIndex == 0) ? 1 : 3;

  // draw cards
  std::vector<int> used;
  std::vector<JsonObject> cards;
  std::vector<bool> reversedFlags;

  for (int i = 0; i < drawCount; i++) {
    int idx = random(items.size());
    while (std::find(used.begin(), used.end(), idx) != used.end())
      idx = random(items.size());

    used.push_back(idx);
    cards.push_back(items[idx]);
    reversedFlags.push_back(reversedEnabled && random(2));
  }

  // header
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 20, "Tiny Tarot", true);
  renderer.drawCenteredText(UI_12_FONT_ID, 50, spreadNames[spreadIndex], true);
  renderer.drawLine(20, 70, screenW - 20, 70);

  // SINGLE CARD
  if (drawCount == 1) {
    JsonObject card = cards[0];
    bool rev = reversedFlags[0];

    const char* name = card["name"];
    const char* symbol = card["fallbackSymbol"] | card["symbol"];
    const char* meaning = rev ? card["reversed"] : card["upright"];

    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 90, name, true);

    int boxW = 180, boxH = 220;
    int boxX = (screenW - boxW) / 2;

    renderer.drawRect(boxX, 120, boxW, boxH, 2, true);

    int sw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
    renderer.drawText(NOTOSANS_18_FONT_ID,
      boxX + (boxW - sw)/2, 200, symbol, true);

    int y = 360;

    auto lines = renderer.wrappedText(UI_12_FONT_ID,
      meaning, screenW - 40, 6);

    for (auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
      y += 24;
    }
  }

  // THREE CARD
  else {
    int cardW = 120, spacing = 12;
    int startX = (screenW - (cardW*3 + spacing*2)) / 2;
    int topY = 100;

    for (int i=0;i<3;i++) {
      int x = startX + i*(cardW+spacing);

      JsonObject card = cards[i];
      bool rev = reversedFlags[i];

      const char* symbol = card["fallbackSymbol"] | card["symbol"];
      const char* label = spreads[spreadIndex][i];

      int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
      renderer.drawText(UI_12_FONT_ID,
        x + (cardW - lw)/2, topY - 18, label, true);

      renderer.drawRect(x, topY, cardW, 140, 2, true);

      int sw = renderer.getTextWidth(NOTOSANS_16_FONT_ID, symbol);
      renderer.drawText(NOTOSANS_16_FONT_ID,
        x + (cardW - sw)/2, topY + 55, symbol, true);
    }

    int y = topY + 180;

    for (int i=0;i<3;i++) {
      JsonObject card = cards[i];
      bool rev = reversedFlags[i];

      std::string header =
        std::string(spreads[spreadIndex][i]) + ": " + (const char*)card["name"];

      renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);
      y += 24;

      auto lines = renderer.wrappedText(UI_12_FONT_ID,
        rev ? card["reversed"] : card["upright"],
        screenW - 40, 2);

      for (auto& line : lines) {
        renderer.drawText(UI_12_FONT_ID, 30, y, line.c_str(), true);
        y += 20;
      }

      y += 16;
    }
  }
}

// App Loop ===============================

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
  // Back
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // List navigation
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

  // Spread switching (for Tarot)
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    spreadIndex = (spreadIndex - 1 + 4) % 4;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    spreadIndex = (spreadIndex + 1) % 4;
    requestUpdate();
  }

  // Confirm = run current app
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (apps.empty()) return;
    if (!loadApp(apps[selected])) return;

    std::string type = currentApp["type"] | "";

    renderer.clearScreen();

    if (type == "coin_flip") {
      renderCoinFlip();
    }
    else if (type == "random_answer") {
      renderEightBall();
    }
    else if (type == "daily_combo") {
      renderDaily();
    }
    else if (type == "random_draw") {
      renderTarot();
    }
    else {
      renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 200, "Unsupported app", true);
    }

    auto labels = mappedInput.mapLabels("", "Again", "Back", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
  }
}
