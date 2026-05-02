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

void SimpleAppsActivity::renderCoinFlip() {
  int screenW = renderer.getScreenWidth();

  bool heads = random(2) == 0;

  const unsigned char* icon = heads
    ? epd_bitmap_coin_heads
    : epd_bitmap_coin_tails;

  renderer.clearScreen();

  // Title
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Coin Flip", true);

  // Icon
  renderer.drawImage(
    icon,
    (screenW - 64) / 2,
    90,
    64,
    64
  );

  // Result
  renderer.drawCenteredText(
    NOTOSANS_16_FONT_ID,
    180,
    heads ? "HEADS" : "TAILS",
    true
  );
}

//8 BALL ========
void SimpleAppsActivity::renderEightBall() {
  int screenW = renderer.getScreenWidth();

  JsonArray arr = currentApp["answers"];
  if (arr.size() == 0) return;

  const char* txt = arr[random(arr.size())];

  renderer.clearScreen();

  // Title
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Magic 8 Ball", true);

  // Icon
  renderer.drawImage(
    epd_bitmap_8ball,
    (screenW - 64) / 2,
    80,
    64,
    64
  );

  // Text
  int y = 170;

  auto lines = renderer.wrappedText(
    UI_12_FONT_ID,
    txt,
    screenW - 40,
    4
  );

  for (auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
    y += 24;
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
  int screenH = renderer.getScreenHeight();

  JsonArray items = currentApp["items"];
  bool reversedEnabled = currentApp["allowReversed"] | false;

  if (items.size() == 0) return;

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

  // =========================
  // DRAW UNIQUE CARDS
  // =========================
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

  renderer.clearScreen();

  // =========================
  // HEADER
  // =========================
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 20, "Tiny Tarot", true);
  renderer.drawCenteredText(UI_12_FONT_ID, 50, spreadNames[spreadIndex], true);
  renderer.drawLine(20, 70, screenW - 20, 70);

  // =========================
  // SINGLE CARD
  // =========================
  if (drawCount == 1) {

    JsonObject card = cards[0];
    bool rev = reversedFlags[0];

    const char* name = card["name"];
    const char* symbol = card["fallbackSymbol"] | card["symbol"];
    const char* meaning = rev ? card["reversed"] : card["upright"];
    const char* yn = card["yesNo"] | "";
    const char* element = card["element"] | "";

    renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 90, name, true);

    int boxW = 180, boxH = 200;
    int boxX = (screenW - boxW) / 2;

    renderer.drawRect(boxX, 110, boxW, boxH, 2, true);

    int sw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
    renderer.drawText(NOTOSANS_18_FONT_ID,
      boxX + (boxW - sw) / 2,
      180,
      symbol,
      true
    );

    int y = 320;

    // Metadata
    std::string meta = std::string(yn) + " • " + element;
    if (rev) meta += " • REVERSED";

    renderer.drawText(UI_10_FONT_ID, 20, y, meta.c_str(), true);
    y += 24;

    // Keywords
    if (card.containsKey("keywords")) {
      JsonArray kws = card["keywords"];
      std::string kwLine = "";
      for (int k = 0; k < kws.size(); k++) {
        kwLine += kws[k].as<const char*>();
        if (k < kws.size() - 1) kwLine += ", ";
      }
      renderer.drawText(UI_10_FONT_ID, 20, y, kwLine.c_str(), true);
      y += 24;
    }

    // Meaning
    auto lines = renderer.wrappedText(UI_12_FONT_ID, meaning, screenW - 40, 6);

    for (auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
      y += 24;
    }
  }

  // =========================
  // THREE CARD
  // =========================
  else {

    int cardW = 110, spacing = 12;
    int startX = (screenW - (cardW * 3 + spacing * 2)) / 2;
    int topY = 90;

    for (int i = 0; i < 3; i++) {
      int x = startX + i * (cardW + spacing);

      JsonObject card = cards[i];
      const char* symbol = card["fallbackSymbol"] | card["symbol"];
      const char* label = spreads[spreadIndex][i];

      int lw = renderer.getTextWidth(UI_12_FONT_ID, label);
      renderer.drawText(UI_12_FONT_ID,
        x + (cardW - lw) / 2,
        topY - 18,
        label,
        true
      );

      renderer.drawRect(x, topY, cardW, 130, 2, true);

      int sw = renderer.getTextWidth(NOTOSANS_16_FONT_ID, symbol);
      renderer.drawText(NOTOSANS_16_FONT_ID,
        x + (cardW - sw) / 2,
        topY + 50,
        symbol,
        true
      );
    }

    int y = topY + 150;

    for (int i = 0; i < 3; i++) {

      if (y > screenH - 40) break;

      JsonObject card = cards[i];
      bool rev = reversedFlags[i];

      std::string header =
        std::string(spreads[spreadIndex][i]) + ": " +
        (const char*)card["name"];

      renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);
      y += 24;

      auto lines = renderer.wrappedText(
        UI_12_FONT_ID,
        rev ? card["reversed"] : card["upright"],
        screenW - 40,
        2
      );

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

  // =========================
  // BACK BUTTON
  // =========================
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // =========================
  // MENU NAVIGATION (Apps list)
  // =========================
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

  // =========================
  // TAROT SPREAD SWITCHING
  // =========================
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    spreadIndex = (spreadIndex - 1 + 4) % 4;
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    spreadIndex = (spreadIndex + 1) % 4;
    requestUpdate();
  }

  // =========================
  // CONFIRM → RUN APP
  // =========================
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {

    if (apps.empty()) return;

    // Load selected app JSON
    if (!loadApp(apps[selected])) return;

    std::string type = currentApp["type"] | "";

    renderer.clearScreen();

    // =========================
    // ROUTING
    // =========================
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
      renderer.drawCenteredText(
        NOTOSANS_16_FONT_ID,
        200,
        "Unsupported app",
        true
      );
    }

    // =========================
    // BUTTON HINTS
    // =========================
    auto labels = mappedInput.mapLabels("", "Confirm", "Back", "");
    GUI.drawButtonHints(
      renderer,
      labels.btn1,
      labels.btn2,
      labels.btn3,
      labels.btn4
    );

    // =========================
    // DISPLAY
    // =========================
    renderer.displayBuffer();
  }
}