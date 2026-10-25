/*
 * Copyright (C) 2026 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#ifndef ART_RUNTIME_BASE_DARWIN_FUTEX_H_
#define ART_RUNTIME_BASE_DARWIN_FUTEX_H_

#include <cerrno>
#include <climits>
#include <cstdint>
#include <ctime>
#include <limits>

// The ulock API is exported by libSystem but absent from the public SDK headers.
// Declarations and operation values: apple-oss-distributions/xnu, bsd/sys/ulock.h.
extern "C" int __ulock_wait(uint32_t, void*, uint64_t, uint32_t);
extern "C" int __ulock_wake(uint32_t, void*, uint64_t);

namespace art {

constexpr int FUTEX_WAIT_PRIVATE = 128;
constexpr int FUTEX_WAKE_PRIVATE = 129;
constexpr int FUTEX_WAIT_BITSET_PRIVATE = 137;
constexpr int FUTEX_CLOCK_REALTIME = 256;
constexpr int FUTEX_BITSET_MATCH_ANY = -1;

static inline bool DarwinClockNanos(clockid_t clock, uint64_t* result) {
  timespec now;
  if (clock_gettime(clock, &now) != 0) return false;
  *result = static_cast<uint64_t>(now.tv_sec) * 1000000000 +
            static_cast<uint64_t>(now.tv_nsec);
  return true;
}

// The private wait/wake subset used by ART. Darwin condition variables wake
// their waiters directly rather than requeueing them onto another kernel queue.
static inline int futex(volatile int* address,
                        int operation,
                        int value,
                        const timespec* timeout,
                        [[maybe_unused]] volatile int* second_address,
                        int third_value) {
  constexpr uint32_t kCompareAndWait = 1;
  constexpr uint32_t kUlockWakeAll = 0x100;
  void* pointer = const_cast<int*>(address);
  if (operation == FUTEX_WAKE_PRIVATE) {
    if (value == 0) return 0;
    if (value != 1 && value != INT_MAX) { errno = EINVAL; return -1; }
    int result = __ulock_wake(kCompareAndWait | (value == 1 ? 0 : kUlockWakeAll), pointer, 0);
    if (result >= 0 || errno == ENOENT) return 0;
    return -1;
  }
  bool absolute = operation == FUTEX_WAIT_BITSET_PRIVATE ||
                  operation == (FUTEX_WAIT_BITSET_PRIVATE | FUTEX_CLOCK_REALTIME);
  if (operation != FUTEX_WAIT_PRIVATE && !absolute) { errno = ENOSYS; return -1; }
  if (absolute && (timeout == nullptr || third_value != FUTEX_BITSET_MATCH_ANY)) {
    errno = EINVAL;
    return -1;
  }
  clockid_t wait_clock = (operation & FUTEX_CLOCK_REALTIME) != 0
      ? CLOCK_REALTIME : CLOCK_MONOTONIC;
  uint64_t deadline = 0;
  if (timeout != nullptr) {
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000) {
      errno = EINVAL;
      return -1;
    }
    uint64_t now;
    if (!DarwinClockNanos(wait_clock, &now)) return -1;
    if (absolute) now = 0;
    uint64_t max_duration = std::numeric_limits<uint64_t>::max() - now;
    uint64_t seconds = static_cast<uint64_t>(timeout->tv_sec);
    uint64_t duration = seconds > (max_duration - static_cast<uint64_t>(timeout->tv_nsec)) /
                                     1000000000
        ? max_duration
        : seconds * 1000000000 + static_cast<uint64_t>(timeout->tv_nsec);
    deadline = now + duration;
  }
  for (;;) {
    if (__atomic_load_n(address, __ATOMIC_RELAXED) != value) {
      errno = EAGAIN;
      return -1;
    }
    uint32_t micros = 0;
    if (timeout != nullptr) {
      uint64_t now;
      if (!DarwinClockNanos(wait_clock, &now)) return -1;
      if (now >= deadline) { errno = ETIMEDOUT; return -1; }
      uint64_t remaining = deadline - now;
      uint64_t rounded = remaining / 1000 + (remaining % 1000 != 0);
      // Recheck realtime deadlines periodically so wall-clock changes are observed.
      uint32_t max_slice = wait_clock == CLOCK_REALTIME ? 100000 : UINT32_MAX;
      micros = rounded > max_slice ? max_slice : static_cast<uint32_t>(rounded);
    }
    int result = __ulock_wait(kCompareAndWait, pointer, static_cast<uint32_t>(value), micros);
    if (result >= 0) return 0;
    if (errno != ETIMEDOUT || timeout == nullptr) return -1;
    // Long timeouts can exceed ulock_wait's 32-bit microsecond limit.
    uint64_t now;
    if (!DarwinClockNanos(wait_clock, &now) || now >= deadline) return -1;
  }
}
}  // namespace art
#endif  // ART_RUNTIME_BASE_DARWIN_FUTEX_H_
