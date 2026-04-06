#pragma once

#if __has_include(<hls_stream.h>)
#include <hls_stream.h>
#else
#include <queue>

namespace hls {

template <typename T>
class stream {
 public:
  explicit stream(const char * = nullptr) {}

  void write(const T &v) { q_.push(v); }
  T read() {
    T v = q_.front();
    q_.pop();
    return v;
  }
  bool empty() const { return q_.empty(); }

 private:
  std::queue<T> q_;
};

}  // namespace hls
#endif
