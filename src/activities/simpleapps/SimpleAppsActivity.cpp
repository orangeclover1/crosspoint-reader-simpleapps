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

static std::vector<ArduinoJson::JsonObject> tarotCards;
static std::vector<bool> tarotReversed;
static bool tarotHasDraw = false;
static int scrollOffset = 0;
static bool tarotSelecting = true;

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

  renderer.clearScreen();

  // ICON
  renderer.drawImage(
    epd_bitmap_resized_image,
    (screenW - 48) / 2,
    20,
    48,
    48
  );

  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 80, "Daily", true);

  int y = 120;

  auto drawSection = [&](const char* title, const char* text) {

    renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, title, true);
    y += 30;

    auto lines = renderer.wrappedText(UI_12_FONT_ID, text, screenW - 40, 4);

    for (auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, 20, y, line.c_str(), true);
      y += 26;
    }

    y += 20;
  };

  drawSection("Prompt", pt);
  drawSection("Goal", gt);
  drawSection("Gratitude", tt);
}


//Tarot
void SimpleAppsActivity::drawTarot() {

  JsonArray items = currentApp["items"].as<JsonArray>();
  if (items.isNull()) return;
  bool reversedEnabled = currentApp["allowReversed"] | false;

  tarotCards.clear();
  tarotReversed.clear();

  int drawCount = (spreadIndex == 0) ? 1 : 3;

  std::vector<int> used;

  for (int i = 0; i < drawCount; i++) {

    int idx = random(items.size());

    while (std::find(used.begin(), used.end(), idx) != used.end())
      idx = random(items.size());

    used.push_back(idx);

    tarotCards.push_back(items[idx]);
    tarotReversed.push_back(reversedEnabled && random(2));
  }

  tarotHasDraw = true;
  scrollOffset = 0;
}

void SimpleAppsActivity::renderTarot() {

  int screenW = renderer.getScreenWidth();
  int screenH = renderer.getScreenHeight();

  const char* spreadNames[] = {
    "Single Card",
    "Past / Present / Future",
    "You / Relationship / Partner",
    "Mind / Body / Spirit",
    "Celtic Cross"
  };

  const char* spreads[][3] = {
    {"","",""},
    {"Past","Present","Future"},
    {"You","Relationship","Partner"},
    {"Mind","Body","Spirit"}
  };

  const char* celticLabels[10] = {
    "Present",
    "Challenge",
    "Past",
    "Future",
    "Above",
    "Below",
    "Advice",
    "External",
    "Hopes/Fears",
    "Outcome"
  };

  // =========================
  // DETERMINE DRAW COUNT
  // =========================
  int drawCount;
  if (spreadIndex == 0) drawCount = 1;
  else if (spreadIndex == 4) drawCount = 10;
  else drawCount = 3;

  renderer.clearScreen();

  // =========================
  // SELECTION SCREEN (VERTICAL LIST)
  // =========================
  if (tarotSelecting) {

    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 30, "Select Spread", true);

    int startY = 90;
    int itemHeight = 36;

    for (int i = 0; i < 5; i++) {

      int y = startY + i * itemHeight;

      std::string label = spreadNames[i];

      if (i == spreadIndex) {
        std::string line = "> " + label;
        renderer.drawText(NOTOSANS_16_FONT_ID, 30, y, line.c_str(), true);
      } else {
        renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, label.c_str(), true);
      }
    }

    renderer.drawCenteredText(
      UI_12_FONT_ID,
      screenH - 20,
      "Up/Down move • Confirm select",
      true
    );

    return;
  }

  // =========================
  // SAFETY CHECK
  // =========================
  if (!tarotHasDraw || tarotCards.empty()) {
    return;
  }

  // =========================
  // HEADER
  // =========================
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 24, "Tiny Tarot", true);
  renderer.drawCenteredText(NOTOSANS_16_FONT_ID, 50, spreadNames[spreadIndex], true);
  renderer.drawLine(20, 70, screenW - 20, 70);

  // =========================
  // CARD AREA
  // =========================
  int cardAreaBottom = 0;

  if (drawCount == 1) {

    auto card = tarotCards[0];
    const char* symbol = card["fallbackSymbol"] | card["symbol"];

    int boxW = 180, boxH = 200;
    int boxX = (screenW - boxW) / 2;
    int boxY = 90;

    renderer.drawRect(boxX, boxY, boxW, boxH, 2, true);

    int sw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, symbol);
    renderer.drawText(NOTOSANS_18_FONT_ID,
      boxX + (boxW - sw) / 2,
      boxY + 70,
      symbol,
      true
    );

    cardAreaBottom = boxY + boxH;
  }
  else if (drawCount == 3) {

    int cardW = 110;
    int spacing = 12;
    int topY = 90;

    int totalW = (cardW * 3) + (spacing * 2);
    int startX = (screenW - totalW) / 2;

    for (int i = 0; i < 3; i++) {

      int x = startX + i * (cardW + spacing);

      auto card = tarotCards[i];
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

    cardAreaBottom = topY + 140;
  }
  else {
    // Celtic Cross (no visuals)
    cardAreaBottom = 80;
  }

  // =========================
  // TEXT AREA (SCROLL)
  // =========================
  int baseY = cardAreaBottom + 35;
  int y = baseY - scrollOffset;
  int maxY = screenH - 20;

  renderer.drawLine(20, baseY - 10, screenW - 20, baseY - 10);

  for (int i = 0; i < drawCount; i++) {

    auto card = tarotCards[i];
    bool rev = tarotReversed[i];

    const char* name = card["name"];
    const char* meaning = rev ? card["reversed"] : card["upright"];
    const char* yn = card["yesNo"] | "";
    const char* element = card["element"] | "";

    std::string header;

    if (drawCount == 1) {
      header = std::string(name);
    }
    else if (drawCount == 3) {
      header = std::string(spreads[spreadIndex][i]) + ": " + name;
    }
    else {
      header = std::string(celticLabels[i]) + ": " + name;
    }

    if (y > 0 && y < maxY)
      renderer.drawText(NOTOSANS_16_FONT_ID, 20, y, header.c_str(), true);

    y += 30;

    std::string meta = std::string(yn) + " • " + element;
    if (rev) meta += " • REVERSED";

    if (y > 0 && y < maxY)
      renderer.drawText(UI_10_FONT_ID, 30, y, meta.c_str(), true);

    y += 24;

    if (card["keywords"].is<JsonArray>()) {
      JsonArray kws = card["keywords"];

      std::string kwLine = "";
      for (int k = 0; k < kws.size(); k++) {
        kwLine += kws[k].as<const char*>();
        if (k < kws.size() - 1) kwLine += ", ";
      }

      if (y > 0 && y < maxY)
        renderer.drawText(UI_10_FONT_ID, 30, y, kwLine.c_str(), true);

      y += 24;
    }

    auto lines = renderer.wrappedText(
      UI_12_FONT_ID,
      meaning,
      screenW - 50,
      (drawCount == 1) ? 6 : 2
    );

    for (auto& line : lines) {
      if (y > 0 && y < maxY)
        renderer.drawText(UI_12_FONT_ID, 30, y, line.c_str(), true);
      y += 26;
    }

    if (i < drawCount - 1) {
      y += 12;
      if (y > 0 && y < maxY)
        renderer.drawLine(20, y, screenW - 20, y);
      y += 18;
    }
  }

  renderer.drawCenteredText(
    UI_10_FONT_ID,
    screenH - 10,
    "↑ ↓ scroll • Confirm redraw • Back",
    true
  );
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

    // Tarot: go back to selection
    if (tarotHasDraw && !tarotSelecting) {
      tarotSelecting = true;
      tarotHasDraw = false;
      requestUpdate();
      return;
    }

    finish();
    return;
  }

  // =========================
  // MENU NAVIGATION
  // =========================
  nav.onNext([this] {
    if (!apps.empty()) {
      selected = (selected + 1) % apps.size();

      tarotHasDraw = false;
      tarotSelecting = true;
      scrollOffset = 0;

      requestUpdate();
    }
  });

  nav.onPrevious([this] {
    if (!apps.empty()) {
      selected = (selected - 1 + apps.size()) % apps.size();

      tarotHasDraw = false;
      tarotSelecting = true;
      scrollOffset = 0;

      requestUpdate();
    }
  });

  // =========================
  // TAROT SPREAD SELECTION (Up/Down)
  // =========================
  if (tarotSelecting) {

    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      spreadIndex = (spreadIndex - 1 + 5) % 5;
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      spreadIndex = (spreadIndex + 1) % 5;
      requestUpdate();
    }
  }

  // =========================
  // SCROLL (only after draw)
  // =========================
  if (tarotHasDraw && !tarotSelecting) {

    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      scrollOffset -= 20;
      if (scrollOffset < 0) scrollOffset = 0;
      requestUpdate();
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      scrollOffset += 20;
      if (scrollOffset > 600) scrollOffset = 600;
      requestUpdate();
    }
  }

  // =========================
  // CONFIRM → RUN APP
  // =========================
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

      // First confirm → leave selection
      if (tarotSelecting) {
        tarotSelecting = false;
        tarotHasDraw = false;
        drawTarot();
      }
      else {
        // redraw
        tarotHasDraw = false;
        drawTarot();
      }

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
        std::string label = ">" + display;
        renderer.drawText(NOTOSANS_16_FONT_ID, 30, y, label.c_str(), true);
      } else {
        renderer.drawText(NOTOSANS_16_FONT_ID, 40, y, display.c_str(), true);
      }
    }
  }

  // ===== BUTTON HINTS =====
  auto labels = mappedInput.mapLabels("", "Open", "Back", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}