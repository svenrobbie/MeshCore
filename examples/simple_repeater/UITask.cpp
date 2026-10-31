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
#define PAGE_COUNT           2      // main + packet filter
#define LONG_PRESS_MILLIS    1000

static uint32_t filterTotalDrops(const Counters& c) {
  uint32_t total = c.channel + c.hash + c.malformed + c.advert + c.path + c.sender + c.text + c.age;
  for (uint8_t i = 0; i < PAYLOAD_TYPE_COUNT; i++) {
    total += c.hops[i] + c.rate[i];
  }
  return total;
}

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _prevBtnState = false;
  _cur_page = 0;
  _btn_down_at = 0;
  _long_handled = false;
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
  int w = _display->width();

  if (millis() < _started_at + BOOT_SCREEN_MILLIS) {
    _display->drawTextCentered(w / 2, 3, "MeshCore");
    _display->drawTextCentered(w / 2, 14, "Repeater");
    _display->drawTextCentered(w / 2, 25, _version_info);
    return;
  }

  if (_cur_page == 1) {
    renderFilterPage();
  } else {
    renderMainPage();
  }
}

void UITask::renderMainPage() {
  char tmp[16];
  int w = _display->width();

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

void UITask::renderFilterPage() {
  char tmp[48];
  int w = _display->width();
  const Counters& c = the_mesh.getFilterCounters();
  bool on = the_mesh.isFilterEnabled();
  bool dry = the_mesh.isFilterDryRun();

  uint32_t hops = 0, rate = 0;
  for (uint8_t i = 0; i < PAYLOAD_TYPE_COUNT; i++) {
    hops += c.hops[i];
    rate += c.rate[i];
  }
  uint32_t total = filterTotalDrops(c);

  if (w >= 100 && _display->height() >= 56) {
    // Wide display: readable two-column layout.
    _display->setCursor(0, 0);
    snprintf(tmp, sizeof(tmp), "Filter: %s%s", on ? "ON" : "OFF", dry ? " (dry-run)" : "");
    _display->print(tmp);

    _display->setCursor(0, 8);
    snprintf(tmp, sizeof(tmp), "Blocked: %lu", (unsigned long)total);
    _display->print(tmp);

    _display->setCursor(0, 16);
    snprintf(tmp, sizeof(tmp), "Hops: %lu   Rate: %lu", (unsigned long)hops, (unsigned long)rate);
    _display->print(tmp);

    _display->setCursor(0, 24);
    snprintf(tmp, sizeof(tmp), "Chan: %lu   Hash: %lu", (unsigned long)c.channel, (unsigned long)c.hash);
    _display->print(tmp);

    _display->setCursor(0, 32);
    snprintf(tmp, sizeof(tmp), "Malformed: %lu   Advert: %lu", (unsigned long)c.malformed, (unsigned long)c.advert);
    _display->print(tmp);

    _display->setCursor(0, 40);
    snprintf(tmp, sizeof(tmp), "Path: %lu   Sender: %lu", (unsigned long)c.path, (unsigned long)c.sender);
    _display->print(tmp);

    _display->setCursor(0, 48);
    snprintf(tmp, sizeof(tmp), "Text: %lu   Age: %lu", (unsigned long)c.text, (unsigned long)c.age);
    _display->print(tmp);
  } else {
    // Narrow display (e.g. 64x48): status + total, then two counters per line.
    _display->setCursor(0, 0);
    snprintf(tmp, sizeof(tmp), "%s %lu", dry ? "DRY" : (on ? "ON" : "OFF"), (unsigned long)total);
    _display->print(tmp);

    _display->setCursor(0, 8);
    snprintf(tmp, sizeof(tmp), "H:%lu R:%lu", (unsigned long)hops, (unsigned long)rate);
    _display->print(tmp);

    _display->setCursor(0, 16);
    snprintf(tmp, sizeof(tmp), "C:%lu K:%lu", (unsigned long)c.channel, (unsigned long)c.hash);
    _display->print(tmp);

    _display->setCursor(0, 24);
    snprintf(tmp, sizeof(tmp), "M:%lu A:%lu", (unsigned long)c.malformed, (unsigned long)c.advert);
    _display->print(tmp);

    _display->setCursor(0, 32);
    snprintf(tmp, sizeof(tmp), "P:%lu S:%lu", (unsigned long)c.path, (unsigned long)c.sender);
    _display->print(tmp);

    _display->setCursor(0, 40);
    snprintf(tmp, sizeof(tmp), "T:%lu G:%lu", (unsigned long)c.text, (unsigned long)c.age);
    _display->print(tmp);
  }
}

void UITask::handleButton(bool btnDown) {
  if (btnDown && !_prevBtnState) {
    _btn_down_at = millis();
    _long_handled = false;
  } else if (btnDown && _prevBtnState) {
    if (!_long_handled && millis() - _btn_down_at >= LONG_PRESS_MILLIS) {
      _long_handled = true;
      if (_display->isOn()) {
        _cur_page = (_cur_page + 1) % PAGE_COUNT;
        _auto_off = millis() + AUTO_OFF_MILLIS;
      } else {
        _display->turnOn();
        _cur_page = 0;
        _auto_off = millis() + AUTO_OFF_MILLIS;
      }
#ifdef PIN_BUZZER
      buzzer.play("page:d=8,o=6,b=200:e,g");
#endif
    }
  } else if (!btnDown && _prevBtnState) {
    if (!_long_handled) {
      if (_display->isOn()) {
        _display->turnOff();
      } else {
        _display->turnOn();
        _cur_page = 0;
        _auto_off = millis() + AUTO_OFF_MILLIS;
      }
#ifdef PIN_BUZZER
      buzzer.play("btn:d=64,o=6,b=200:c");
#endif
    }
  }
  _prevBtnState = btnDown;
}

void UITask::loop() {
#if defined(PIN_USER_BTN)
  if (millis() >= _next_read) {
    handleButton(digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED);
    _next_read = millis() + 50;
  }
#elif defined(PIN_USER_BTN_READ_FUNC)
  if (millis() >= _next_read) {
    handleButton(PIN_USER_BTN_READ_FUNC());
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

  if (_powering_off_at > 0) { // power off timer armed
#if defined(LED_PIN) && defined(LED_STATE_ON)
    digitalWrite(LED_PIN, LED_STATE_ON); // switch on the led until poweroff
    delay(1000);
    digitalWrite(LED_PIN, !LED_STATE_ON); // off LED
#endif
    if (millis() > _powering_off_at) {
      _board->powerOff();  // should not return
    }
  }
}
