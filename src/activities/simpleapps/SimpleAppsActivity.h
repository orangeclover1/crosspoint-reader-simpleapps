#pragma once
#include <cstdint>
#include <string>
#include <vector>


struct SimpleAppEntry{std::string name;std::string path;std::string type;};
struct SimpleDrawResult{int itemIndex=0;bool reversed=false;};
enum class SimpleAppsMode:uint8_t{AppList,AppMenu,Result};

class SimpleAppsActivity final:public Activity{
  ButtonNavigator buttonNavigator;
  SimpleAppsMode mode=SimpleAppsMode::AppList;
  std::vector<SimpleAppEntry> apps;
  JsonDocument currentApp;
  int selectedApp=0,selectedAction=0,selectedResult=0;
  std::vector<SimpleDrawResult> drawResults;
  uint32_t seed=1;
  uint32_t nextRandom();
  int randomIndex(int maxExclusive);
  bool randomBool();
  void loadApps();
  bool loadCurrentApp();
  void runSelectedAction();
  void renderAppList();
  void renderAppMenu();
  void renderResult();
  void renderCoinFlipResult();
  void renderRandomAnswerResult();
  void renderRandomDrawResult();
  void renderDailyComboResult();
  void drawCardBox(int x,int y,int w,int h,const char* symbol,bool reversed);
  void drawWrappedText(int x,int& y,int maxWidth,const char* text,int maxLines);
 public:
  explicit SimpleAppsActivity(GfxRenderer& renderer,MappedInputManager& mappedInput)
    :Activity("Simple Apps",renderer,mappedInput){}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
