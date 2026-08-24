#include "UITask.h"
#include <Arduino.h>
#include <target.h>
#include <helpers/CommonCLI.h>
#include "MyMesh.h"
extern MyMesh the_mesh;

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      3600000  // 1 hour
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _prevBtnState = false;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _started_at = millis();
  _node_prefs = node_prefs;
  _display->turnOn();

  char *version = strdup(firmware_version);
  char *dash = strchr(version, '-');
  if (dash) *dash = 0;
  snprintf(_version_info, sizeof(_version_info), "%s (%s)", version, build_date);
  free(version);

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.startup();
#endif
}

void UITask::renderCurrScreen() {
  char tmp[16];
  int w = _display->width();

  if (millis() < _started_at + BOOT_SCREEN_MILLIS) {
    _display->drawTextCentered(w / 2, 3, "MeshCore");
    _display->drawTextCentered(w / 2, 14, "Repeater");
    _display->drawTextCentered(w / 2, 25, _version_info);
    return;
  }

  _display->setCursor(0, 0);
  _display->print(_node_prefs->node_name);

  _display->setCursor(0, 8);
  snprintf(tmp, sizeof(tmp), "RX:%lu", radio_driver.getPacketsRecv());
  _display->print(tmp);

  _display->setCursor(0, 16);
  snprintf(tmp, sizeof(tmp), "TX:%lu", radio_driver.getPacketsSent());
  _display->print(tmp);

  _display->setCursor(0, 24);
  snprintf(tmp, sizeof(tmp), "RSSI:%d", (int)radio_driver.getLastRSSI());
  _display->print(tmp);

  _display->setCursor(0, 32);
  snprintf(tmp, sizeof(tmp), "SNR:%d", (int)radio_driver.getLastSNR());
  _display->print(tmp);

  unsigned long air_ms = the_mesh.getTotalAirTime();
  unsigned long up_ms = millis();
  float dc = (up_ms > 0) ? 100.0f * (float)air_ms / (float)up_ms : 0.0f;
  snprintf(tmp, sizeof(tmp), "AirTX:%.1f%%", dc);
  _display->drawTextCentered(w / 2, 40, tmp);
}

void UITask::loop() {
#if defined(PIN_USER_BTN)
  if (millis() >= _next_read) {
    bool btnDown = (digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED);
    if (btnDown != _prevBtnState) {
      if (btnDown) {
#ifdef PIN_BUZZER
        buzzer.play("btn:d=64,o=6,b=200:c");
#endif
        if (_display->isOn()) {
          _display->turnOff();
        } else {
          _display->turnOn();
          _auto_off = millis() + AUTO_OFF_MILLIS;
        }
      }
      _prevBtnState = btnDown;
    }
    _next_read = millis() + 200;
  }
#elif defined(PIN_USER_BTN_READ_FUNC)
  if (millis() >= _next_read) {
    bool btnDown = PIN_USER_BTN_READ_FUNC();
    if (btnDown != _prevBtnState) {
      if (btnDown) {
#ifdef PIN_BUZZER
        buzzer.play("btn:d=64,o=6,b=200:c");
#endif
        if (_display->isOn()) {
          _display->turnOff();
        } else {
          _display->turnOn();
          _auto_off = millis() + AUTO_OFF_MILLIS;
        }
      }
      _prevBtnState = btnDown;
    }
    _next_read = millis() + 50;
  }
#endif

#ifdef PIN_BUZZER
  if (buzzer.isPlaying()) buzzer.loop();
#endif

  if (_display->isOn()) {
    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + 1000;   // refresh every second
    }
    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }

  board.tx_led_enabled = _display->isOn();
}
