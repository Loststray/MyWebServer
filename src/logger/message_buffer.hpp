#ifndef MESSAGE_BUFFER_HPP_
#define MESSAGE_BUFFER_HPP_
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>
template <class T> struct MessageBuffer {
private:
  std::unique_ptr<T[]> array_;
  int capacity_, head_, tail_, size_;
  bool stop_;
  std::condition_variable producer_, consumer_, empty_;
  std::mutex mtx_;

public:
  MessageBuffer(int capacity = 1 << 10) : capacity_(capacity), stop_(false) {
    array_ = std::make_unique<T[]>(capacity);
    head_ = tail_ = size_ = 0;
  }
  ~MessageBuffer() {
    {
      std::lock_guard<std::mutex> lk(mtx_);
      stop_ = true;
    }
    producer_.notify_all();
    consumer_.notify_all();
    empty_.notify_all();
  }
  void clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    head_ = tail_ = size_ = 0;
  }
  bool push_back(T &&w) {
    std::unique_lock<std::mutex> lk(mtx_);
    if (size_ == capacity_) {
      consumer_.notify_all();
      producer_.wait(lk, [&]() { return size_ < capacity_ || stop_; });
      if (stop_) {
        return false;
      }
    }
    array_[tail_] = std::forward<T>(w);
    tail_ = (tail_ + 1) % capacity_;
    size_++;
    consumer_.notify_one();
    return true;
  }
  bool pop_front(T &w) {
    std::unique_lock<std::mutex> lk(mtx_);
    if (size_ == 0) {
      producer_.notify_all();
      consumer_.wait(lk, [&]() { return size_ > 0 || stop_; });
      if (stop_) {
        return false;
      }
    }
    w = array_[head_];
    head_ = (head_ + 1) % capacity_;
    size_--;
    if (size_ == 0) {
      empty_.notify_all();
    }
    return true;
  }
  // Block until the buffer becomes empty
  void wait_till_empty() {
    std::unique_lock<std::mutex> lk(mtx_);
    empty_.wait(lk, [&]() { return size_ == 0 || stop_; });
  }
  int size() {
    std::lock_guard<std::mutex> lk(mtx_);
    return size_;
  }
  bool empty() {
    std::lock_guard<std::mutex> lk(mtx_);
    return size_ == 0;
  }
  bool full() {
    std::lock_guard<std::mutex> lk(mtx_);
    return size_ == capacity_;
  }
  T &front() {
    std::lock_guard<std::mutex> lk(mtx_);
    return array_[head_];
  }
  T &back() {
    std::lock_guard<std::mutex> lk(mtx_);
    return array_[tail_ - 1];
  }
};

#endif
