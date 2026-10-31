#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include "Stream.h"

inline uint32_t g_mock_millis = 0;

using std::isnan;

inline uint32_t millis() {
  return g_mock_millis;
}

inline void delay(uint32_t ms) {
  g_mock_millis += ms;
}

// Same definition as the Arduino core macro.
#ifndef constrain
  #define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

// ---- filesystem stubs -------------------------------------------------------
class File {
public:
  operator bool() const { return false; }
  int read(uint8_t* buf, size_t len) { (void)buf; (void)len; return 0; }
  size_t write(const uint8_t* buf, size_t len) { (void)buf; (void)len; return 0; }
  void close() {}
};

class FsMock {
public:
  bool exists(const char* path) { (void)path; return false; }
  bool remove(const char* path) { (void)path; return false; }
  bool mkdir(const char* path) { (void)path; return false; }
  File open(const char* path) { (void)path; return File(); }
  File open(const char* path, const char* mode) { (void)path; (void)mode; return File(); }
  File open(const char* path, const char* mode, bool create) {
    (void)path; (void)mode; (void)create; return File();
  }
};

#ifndef FILESYSTEM
  #define FILESYSTEM FsMock
#endif
