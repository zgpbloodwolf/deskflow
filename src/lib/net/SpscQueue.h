/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>

/// 原子鼠标位置槽 -- 生产者写入最新位置，消费者读取最新位置并覆盖旧值
/// alignas(64) 防止 false sharing（RESEARCH.md Pitfall 2）
/// 写入端: 先 relaxed store x/y, 再 release store updated 标志
/// 读取端: 先 acquire exchange updated 标志, 再 relaxed load x/y
struct alignas(64) MousePositionSlot
{
  std::atomic<int32_t> x{0};
  std::atomic<int32_t> y{0};
  std::atomic<bool> updated{false};

  /// 存储最新鼠标位置（生产者调用，EventQueue 线程）
  void store(int32_t newX, int32_t newY) noexcept
  {
    x.store(newX, std::memory_order_relaxed);
    y.store(newY, std::memory_order_relaxed);
    updated.store(true, std::memory_order_release);
  }

  /// 读取最新鼠标位置（消费者调用，Asio io_context 线程）
  /// 返回 true 表示有新的位置数据，false 表示无新数据
  bool load(int32_t &outX, int32_t &outY) noexcept
  {
    if (!updated.exchange(false, std::memory_order_acquire)) {
      return false;
    }
    outX = x.load(std::memory_order_relaxed);
    outY = y.load(std::memory_order_relaxed);
    return true;
  }
};

/// 无锁单生产者单消费者 FIFO 环形缓冲区
/// 用于键盘事件缓冲，保证顺序和完整性（D-05）
/// head 和 tail 各 alignas(64) 防止 false sharing（RESEARCH.md Pitfall 2）
template <typename T, size_t Capacity> class alignas(64) SpscFifo
{
  static constexpr size_t kMask = Capacity - 1;
  static_assert((Capacity & kMask) == 0, "Capacity must be power of 2");

  std::array<T, Capacity> buffer_{};
  alignas(64) std::atomic<size_t> head_{0}; // 消费者读取位置
  alignas(64) std::atomic<size_t> tail_{0}; // 生产者写入位置

public:
  /// 推入元素（生产者调用，EventQueue 线程）
  /// 返回 true 成功，false 队列已满
  bool push(const T &item) noexcept
  {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t next = (tail + 1) & kMask;
    if (next == head_.load(std::memory_order_acquire)) {
      return false; // 队列满
    }
    buffer_[tail] = item;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  /// 弹出元素（消费者调用，Asio io_context 线程）
  /// 返回 true 成功，false 队列空
  bool pop(T &item) noexcept
  {
    const size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) {
      return false; // 队列空
    }
    item = buffer_[head];
    head_.store((head + 1) & kMask, std::memory_order_release);
    return true;
  }

  /// 检查队列是否为空
  bool empty() const noexcept
  {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }
};
