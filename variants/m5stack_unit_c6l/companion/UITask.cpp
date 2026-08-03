#include "UITask.h"
#include <target.h>
#include "../../examples/companion_radio/MyMesh.h"

#define AUTO_OFF_MILLIS    3600000
#define BOOT_SCREEN_MILLIS 4000
#define PAGE_COUNT         3
#define SCROLL_SPEED_MS    150
#define SCROLL_PAUSE_MS    2000

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _node_prefs = node_prefs;
  _need_refresh = true;
  _msgcount = 0;
  _next_refresh = 0;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _status_timeout = millis() + 10000;
  _was_connected = false;
  _scroller.reset();

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif
}

void UITask::notify(UIEventType t) {
#ifdef PIN_BUZZER
  switch (t) {
    case UIEventType::contactMessage:
      buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
      break;
    case UIEventType::channelMessage:
      buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
      break;
    case UIEventType::ack:
      buzzer.play("ack:d=32,o=8,b=120:c");
      break;
    default:
      break;
  }
#endif
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;
  _need_refresh = true;
  _status_timeout = millis() + 10000;
}

void UITask::renderScreen() {
  if (_display == NULL) return;

  int w = _display->width();

  if (millis() < BOOT_SCREEN_MILLIS) {
    _display->setTextSize(1);
    _display->drawTextCentered(w / 2, 3, "MeshCore");
    _display->drawTextCentered(w / 2, 20, FIRMWARE_VERSION);
    _display->drawTextCentered(w / 2, 34, "Companion");
    return;
  }

  _display->setTextSize(1);

  switch (_cur_page) {
    case 0: renderPageSystem(); break;
    case 1: renderPageStats();  break;
    case 2: renderPageInfo();   break;
  }
}

void UITask::renderPageSystem() {
  char tmp[32];
  int w = _display->width();

#ifdef BLE_PIN_CODE
  uint32_t pin = the_mesh.getBLEPin();
  if (_connected) {
    if (millis() < _status_timeout) {
      _display->drawTextCentered(w / 2, 0, "Connected");
    } else {
      int nameW = _display->getTextWidth(_node_prefs->node_name);
      if (nameW <= w) {
        _display->setCursor(0, 0);
        _display->print(_node_prefs->node_name);
      } else {
        _display->setCursor(-_scroller.offset, 0);
        _display->print(_node_prefs->node_name);
      }
    }
  } else if (pin != 0) {
    snprintf(tmp, sizeof(tmp), "PIN:%06d", pin);
    _display->drawTextCentered(w / 2, 0, tmp);
  } else {
    if (millis() < _status_timeout) {
      _display->drawTextCentered(w / 2, 0, "Ready");
    }
  }
#else
  if (millis() < _status_timeout) {
    _display->drawTextCentered(w / 2, 0, "USB Ready");
  } else {
    int nameW = _display->getTextWidth(_node_prefs->node_name);
    if (nameW <= w) {
      _display->setCursor(0, 0);
      _display->print(_node_prefs->node_name);
    } else {
      _display->setCursor(-_scroller.offset, 0);
      _display->print(_node_prefs->node_name);
    }
  }
#endif

  snprintf(tmp, sizeof(tmp), "Noise:%d", radio_driver.getNoiseFloor());
  _display->setCursor(0, 8);
  _display->print(tmp);

  unsigned long ms = millis();
  unsigned long h = ms / 3600000;
  unsigned long m = (ms % 3600000) / 60000;
  snprintf(tmp, sizeof(tmp), "Up: %luh%02lu", h, m);
  _display->setCursor(0, 16);
  _display->print(tmp);

  if (_msgcount > 0) {
    snprintf(tmp, sizeof(tmp), "%d unread", _msgcount);
    _display->setCursor(0, 24);
    _display->print(tmp);
  }
}

void UITask::renderPageStats() {
  char tmp[32];

  snprintf(tmp, sizeof(tmp), "RSSI:%d", (int)radio_driver.getLastRSSI());
  _display->setCursor(0, 0);
  _display->print(tmp);

  snprintf(tmp, sizeof(tmp), "SNR:%+d", (int)radio_driver.getLastSNR());
  _display->setCursor(0, 8);
  _display->print(tmp);

  snprintf(tmp, sizeof(tmp), "R:%lu", radio_driver.getPacketsRecv());
  _display->setCursor(0, 16);
  _display->print(tmp);

  snprintf(tmp, sizeof(tmp), "T:%lu", radio_driver.getPacketsSent());
  _display->setCursor(0, 24);
  _display->print(tmp);

  unsigned long air_ms = the_mesh.getTotalAirTime();
  unsigned long up_ms = millis();
  float dc = (up_ms > 0) ? 100.0f * (float)air_ms / (float)up_ms : 0.0f;
  snprintf(tmp, sizeof(tmp), "AirTX:%.1f%%", dc);
  _display->setCursor(0, 32);
  _display->print(tmp);
}

void UITask::renderPageInfo() {
  char tmp[32];
  int w = _display->width();

  snprintf(tmp, sizeof(tmp), "FW %s", FIRMWARE_VERSION);
  _display->drawTextCentered(w / 2, 0, tmp);

  _display->drawTextCentered(w / 2, 8, "by");

  _display->drawTextCentered(w / 2, 40, "SvenRobbie");
}

bool UITask::isBuzzerQuiet() {
#ifdef PIN_BUZZER
  return buzzer.isQuiet();
#else
  return true;
#endif
}

void UITask::toggleBuzzer() {
#ifdef PIN_BUZZER
  if (buzzer.isQuiet()) {
    buzzer.quiet(false);
    notify(UIEventType::ack);
  } else {
    buzzer.quiet(true);
  }
  _node_prefs->buzzer_quiet = buzzer.isQuiet();
  the_mesh.savePrefs();
#endif
}

void UITask::loop() {
  if (_display == NULL) return;

  bool btn = board.isButtonPressed();
  if (btn && !_last_button_state) {
    _btn_down_time = millis();
    _long_handled = false;
  }
  if (btn && _last_button_state) {
    if (!_long_handled && millis() - _btn_down_time >= 1000) {
      _cur_page = (_cur_page + 1) % PAGE_COUNT;
      _need_refresh = true;
      _auto_off = millis() + AUTO_OFF_MILLIS;
      _long_handled = true;
#ifdef PIN_BUZZER
      buzzer.play("page:d=8,o=6,b=200:e,g");
#endif
    }
  }
  if (!btn && _last_button_state) {
    if (!_long_handled) {
      if (_display->isOn()) {
        _display->turnOff();
      } else {
        _display->turnOn();
        _cur_page = 0;
        _need_refresh = true;
        _auto_off = millis() + AUTO_OFF_MILLIS;
      }
#ifdef PIN_BUZZER
      buzzer.play("btn:d=64,o=6,b=200:c");
#endif
    }
  }
  _last_button_state = btn;

  if (_connected != _was_connected) {
    if (_connected) {
      _status_timeout = millis() + 10000;
    }
    _was_connected = _connected;
  }

#ifdef PIN_BUZZER
  if (buzzer.isPlaying()) buzzer.loop();
#endif

  if (_display->isOn()) {
    if (_node_prefs != NULL) {
      _scroller.update(_display->getTextWidth(_node_prefs->node_name),
                       _display->width(), millis(), SCROLL_SPEED_MS, SCROLL_PAUSE_MS);
    }

    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderScreen();
      _display->endFrame();
      _next_refresh = millis() + 200;
    }

    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }
}
