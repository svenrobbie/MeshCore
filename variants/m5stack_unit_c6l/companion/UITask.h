#pragma once

#include <Arduino.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/SensorManager.h>
#include "../../examples/companion_radio/NodePrefs.h"
#include "../../examples/companion_radio/AbstractUITask.h"

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

class UITask : public AbstractUITask {
public:
  UITask(mesh::MainBoard* board, BaseSerialInterface* serial)
    : AbstractUITask(board, serial), _display(NULL),
      _status_timeout(0), _was_connected(false),
      _last_button_state(false), _cur_page(0), _btn_down_time(0),
      _long_handled(false) {}

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  void loop() override;
  void notify(UIEventType t) override;
  void msgRead(int msgcount) override { _msgcount = msgcount; _need_refresh = true; }
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void renderPageSystem();
  void renderPageStats();
  void renderPageInfo();

  bool isBuzzerQuiet();
  void toggleBuzzer();

private:
  void renderScreen();
  DisplayDriver* _display;
  NodePrefs* _node_prefs;
  int _msgcount;
  bool _need_refresh;
  uint32_t _next_refresh;
  uint32_t _auto_off;
  unsigned long _status_timeout;
  bool _was_connected;
  MarqueeScroller _scroller;
  bool _last_button_state;
  int _cur_page;
  unsigned long _btn_down_time;
  bool _long_handled;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
#endif
};
