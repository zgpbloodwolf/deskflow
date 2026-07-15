/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 仅在 macOS 平台编译
#ifdef __APPLE__

#include "net/BtBackendMacOS.h"
#include "base/Log.h"

#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <deque>

// macOS 平台工厂函数实现
std::unique_ptr<BtBackend> createBtBackend()
{
  return std::make_unique<BtBackendMacOS>();
}

//! Objective-C++ 内部实现类
@interface BtBackendImpl : NSObject <IOBluetoothRFCOMMChannelDelegate>
{
  @public
  IOBluetoothRFCOMMChannel *m_channel;
  IOBluetoothDevice *m_device;
  BOOL m_connected;
  BOOL m_listening;

  // 读取缓冲区和同步机制
  std::mutex m_bufferMutex;
  std::condition_variable m_bufferCV;
  std::deque<uint8_t> m_readBuffer;

  // 服务端：等待传入连接
  std::mutex m_acceptMutex;
  std::condition_variable m_acceptCV;
  IOBluetoothRFCOMMChannel *m_pendingChannel;
  BOOL m_hasPendingConnection;

  // SDP 服务记录（监听模式）
  IOBluetoothSDPServiceRecord *m_sdpRecord;

  // 通道打开通知对象（需要保持引用以维持注册）
  IOBluetoothUserNotification *m_channelNotification;

  // 数据转发：监听 impl 作为 delegate 接收数据后转发给数据 impl
  BtBackendImpl *m_dataChild;
}

- (BOOL)connectToDevice:(NSString *)address channel:(int)channel;
- (BOOL)listenOnChannel:(int)channel;
- (BOOL)publishSDPServiceRecord:(int)channel;
- (std::unique_ptr<BtBackend>)acceptConnection;
- (int)readData:(void *)buf length:(size_t)len;
- (int)writeData:(const void *)buf length:(size_t)len;
- (void)closeConnection;
- (BOOL)isConnected;
- (BOOL)isListening;
- (BOOL)pollRead:(int)timeoutMs;

// IOBluetoothRFCOMMChannelDelegate
// 注意：签名必须精确匹配 IOBluetoothRFCOMMChannelDelegate 协议
// （IOBluetoothRFCOMMChannel.h 的 rfcommChannelData:data:length:），
// 否则 @optional 方法不会被调用，导致收不到数据。
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                     data:(void *)dataPointer
                   length:(size_t)dataLength;
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel;
- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel
                           refCon:(void *)refCon
                            status:(IOReturn)status;

// 新连接通知（selector 签名必须匹配 IOBluetoothRFCOMMChannel 的回调格式）
- (void)rfcommChannelOpened:(IOBluetoothUserNotification *)notification channel:(IOBluetoothRFCOMMChannel *)newChannel;

@end

@implementation BtBackendImpl

- (instancetype)init
{
  self = [super init];
  if (self) {
    m_channel = nil;
    m_device = nil;
    m_connected = NO;
    m_listening = NO;
    m_pendingChannel = nil;
    m_hasPendingConnection = NO;
    m_sdpRecord = nil;
    m_channelNotification = nil;
    m_dataChild = nil;
  }
  return self;
}

- (void)dealloc
{
  [self closeConnection];
  [super dealloc];
}

- (BOOL)connectToDevice:(NSString *)address channel:(int)channel
{
  LOG_INFO("蓝牙后端：正在连接到 %s 通道 %d", [address UTF8String], channel);

  BluetoothDeviceAddress btAddr;
  if (IOBluetoothNSStringToDeviceAddress(address, &btAddr) != kIOReturnSuccess) {
    LOG_ERR("蓝牙后端：无法解析蓝牙地址: %s", [address UTF8String]);
    return NO;
  }

  m_device = [IOBluetoothDevice deviceWithAddress:&btAddr];
  if (m_device == nil) {
    LOG_ERR("蓝牙后端：无法创建设备对象");
    return NO;
  }

  IOBluetoothRFCOMMChannel *newChannel = nil;
  IOReturn result = [m_device openRFCOMMChannelSync:&newChannel
                                         withChannelID:channel
                                             delegate:self];
  // 以 newChannel 是否建立为准：IOBluetooth 可能返回非成功码但 channel 实际已建立
  // （数据可正常收发）。若严格按 result 判断，会把"已建立但带警告码"的连接误判为
  // 失败，导致不启动 ioThread、收到的数据无人读取、握手超时。
  if (newChannel == nil) {
    LOG_ERR("蓝牙后端：打开 RFCOMM 通道失败，newChannel=nil，错误码: %d", result);
    return NO;
  }
  if (result != kIOReturnSuccess) {
    LOG_WARN("蓝牙后端：openRFCOMM 通道返回非成功码 %d，但 channel 已建立，继续使用", result);
  }

  m_channel = newChannel;
  [m_channel setDelegate:self];
  m_connected = YES;

  LOG_INFO("蓝牙后端：已连接到 %s", [address UTF8String]);
  return YES;
}

- (BOOL)listenOnChannel:(int)channel
{
  LOG_INFO("蓝牙后端：正在通道 %d 上监听", channel);

  // 检查蓝牙适配器是否可用（这是安全的只读操作）
  IOBluetoothHostController *hc = [IOBluetoothHostController defaultController];
  if (hc == nil) {
    LOG_ERR("蓝牙后端：没有找到蓝牙适配器");
    return NO;
  }

  BluetoothHCIPowerState powerState = [hc powerState];
  if (powerState != kBluetoothHCIPowerStateON) {
    LOG_ERR("蓝牙后端：蓝牙未开启，当前状态: %d", powerState);
    return NO;
  }

  LOG_INFO("蓝牙后端：蓝牙适配器已就绪");

  // 注册 RFCOMM 通道通知，监听传入连接
  m_channelNotification = [IOBluetoothRFCOMMChannel
      registerForChannelOpenNotifications:self
                                selector:@selector(rfcommChannelOpened:channel:)
                              withChannelID:channel
                                direction:kIOBluetoothUserNotificationChannelDirectionIncoming];

  if (m_channelNotification == nil) {
    LOG_ERR("蓝牙后端：注册 RFCOMM 通道通知失败");
    return NO;
  }

  // 发布 SDP 服务记录，使 Windows 等平台客户端能够发现并连接
  // Windows 蓝牙栈依赖 SDP 来发现 RFCOMM 服务，缺少 SDP 记录会导致连接失败
  if (![self publishSDPServiceRecord:channel]) {
    LOG_WARN("蓝牙后端：发布 SDP 服务记录失败，连接可能受限");
    // 不返回 NO，因为通知注册已成功，某些客户端仍可能直接连接
  }

  m_listening = YES;
  LOG_INFO("蓝牙后端：已在通道 %d 上监听", channel);
  return YES;
}

- (BOOL)publishSDPServiceRecord:(int)channel
{
  // 使用标准 SPP UUID: 00001101-0000-1000-8000-00805F9B34FB
  uint8_t sppUUIDBytes[] = {
      0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
  };
  IOBluetoothSDPUUID *sppUUID = [IOBluetoothSDPUUID uuidWithBytes:sppUUIDBytes length:16];
  if (sppUUID == nil) {
    LOG_ERR("蓝牙后端：创建 SPP UUID 失败");
    return NO;
  }

  // 直接使用原始类型构建 SDP 服务字典
  NSDictionary *serviceDict = @{
      @"0001" : @[ sppUUID ],
      @"0004" : @[
          @[ [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16L2CAP] ],
          @[ [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16RFCOMM], @(channel) ],
      ],
      @"0100" : @"Deskflow",
  };

  m_sdpRecord = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:serviceDict];
  if (m_sdpRecord == nil) {
    LOG_ERR("蓝牙后端：创建 SDP 服务记录失败");
    return NO;
  }

  LOG_INFO("蓝牙后端：已发布 SDP 服务记录，通道 %d", channel);
  return YES;
}

- (void)rfcommChannelOpened:(IOBluetoothUserNotification *)notification channel:(IOBluetoothRFCOMMChannel *)newChannel
{
  LOG_INFO("蓝牙后端：收到新的蓝牙连接，channel=%p, thread=%p", newChannel, [[NSThread currentThread] description].UTF8String);

  std::lock_guard<std::mutex> lock(m_acceptMutex);
  if (m_hasPendingConnection && m_pendingChannel != nil) {
    // 已有待处理的连接，拒绝新连接
    [newChannel closeChannel];
    LOG_WARN("蓝牙后端：已有待处理连接，拒绝新连接");
    return;
  }

  // 设置当前 impl 为 channel 的 delegate，使读写都通过当前 impl 进行
  // writeSync 要求 channel 已设置 delegate 才能正常工作
  [newChannel setDelegate:self];

  // retain channel，由当前 impl 的 m_channel 持有
  m_pendingChannel = newChannel;
  CFRetain((__bridge CFTypeRef)m_pendingChannel);

  m_hasPendingConnection = YES;

  // 打印连接信息
  IOBluetoothDevice *device = [newChannel getDevice];
  if (device != nil) {
    NSString *addr = [device addressString];
    LOG_INFO("蓝牙后端：连接设备地址: %s", [addr UTF8String]);
  }

  m_acceptCV.notify_one();
}

- (std::unique_ptr<BtBackend>)acceptConnection
{
  // 非阻塞：仅检查是否有待处理连接。由 BtListenSocket 的 RunLoop 定时器周期性调用，
  // 不能在此等待，否则会阻塞 RunLoop 线程导致 IOBluetooth 回调无法派发。
  std::lock_guard<std::mutex> lock(m_acceptMutex);
  if (!m_hasPendingConnection || m_pendingChannel == nil) {
    return nullptr;
  }

  BtBackendImpl *child = [[BtBackendImpl alloc] init];
  child->m_channel = m_pendingChannel;
  child->m_connected = YES;
  [child->m_channel setDelegate:child];

  m_pendingChannel = nil;
  m_hasPendingConnection = NO;

  std::unique_ptr<BtBackendMacOS> backend(new BtBackendMacOS((__bridge void *)child));

  LOG_INFO("蓝牙后端：已接受蓝牙连接");
  return backend;
}

- (int)readData:(void *)buf length:(size_t)len
{
  std::lock_guard<std::mutex> lock(m_bufferMutex);

  if (m_readBuffer.empty()) {
    return 0;
  }

  size_t toRead = std::min(len, m_readBuffer.size());
  auto *dest = static_cast<uint8_t *>(buf);
  for (size_t i = 0; i < toRead; ++i) {
    dest[i] = m_readBuffer.front();
    m_readBuffer.pop_front();
  }
  return static_cast<int>(toRead);
}

- (int)writeData:(const void *)buf length:(size_t)len
{
  if (!m_connected || m_channel == nil) {
    return -1;
  }

  // RFCOMM 单次写入受 MTU 限制，且 writeSync:length: 的 length 参数为 UInt16（上限 65535），
  // 直接传入大于 MTU/64KB 的包会被截断，导致实际写入量与返回值不一致、协议流错位。
  // 这里按实际 MTU 分片写入，保证全部字节可靠发送。
  BluetoothRFCOMMMTU mtu = [m_channel getMTU];
  if (mtu == 0) {
    mtu = 127; // RFCOMM 默认最小 MTU
  }

  const auto *p = static_cast<const uint8_t *>(buf);
  size_t written = 0;
  while (written < len) {
    size_t chunk = std::min(static_cast<size_t>(mtu), len - written);
    IOReturn result = [m_channel writeSync:const_cast<uint8_t *>(p + written)
                                    length:static_cast<UInt16>(chunk)];
    if (result != kIOReturnSuccess) {
      LOG_ERR("蓝牙后端：写入失败（已写 %zu/%zu 字节），错误码: %d", written, len, result);
      m_connected = NO;
      return -1;
    }
    written += chunk;
  }

  return static_cast<int>(written);
}

- (void)closeConnection
{
  // 取消通道打开通知注册
  if (m_channelNotification != nil) {
    [m_channelNotification unregister];
    m_channelNotification = nil;
  }

  // 移除 SDP 服务记录
  if (m_sdpRecord != nil) {
    [m_sdpRecord removeServiceRecord];
    m_sdpRecord = nil;
  }

  if (m_channel != nil) {
    [m_channel closeChannel];
    m_channel = nil;
  }
  m_connected = NO;
  m_listening = NO;

  std::lock_guard<std::mutex> lock(m_bufferMutex);
  m_bufferCV.notify_all();
  m_acceptCV.notify_all();
}

- (BOOL)isConnected
{
  return m_connected;
}

- (BOOL)isListening
{
  return m_listening;
}

- (BOOL)pollRead:(int)timeoutMs
{
  std::unique_lock<std::mutex> lock(m_bufferMutex);
  if (!m_readBuffer.empty()) {
    return YES;
  }
  m_bufferCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return !m_readBuffer.empty(); });
  return !m_readBuffer.empty();
}

#pragma mark - IOBluetoothRFCOMMChannelDelegate

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                     data:(void *)dataPointer
                   length:(size_t)dataLength
{
  auto *bytes = static_cast<const uint8_t *>(dataPointer);

  // 如果是监听模式且有数据子节点，转发数据给子节点（已接受的连接）
  if (m_dataChild != nil) {
    std::lock_guard<std::mutex> lock(m_dataChild->m_bufferMutex);
    for (NSUInteger i = 0; i < dataLength; ++i) {
      m_dataChild->m_readBuffer.push_back(bytes[i]);
    }
    m_dataChild->m_bufferCV.notify_one();
    return;
  }

  // 否则写入自己的缓冲区
  std::lock_guard<std::mutex> lock(m_bufferMutex);
  for (NSUInteger i = 0; i < dataLength; ++i) {
    m_readBuffer.push_back(bytes[i]);
  }
  m_bufferCV.notify_one();
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
  LOG_INFO("蓝牙后端：RFCOMM 通道已关闭");
  m_connected = NO;
  m_bufferCV.notify_all();
  m_acceptCV.notify_all();
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel
                           refCon:(void *)refCon
                            status:(IOReturn)status
{
  if (status != kIOReturnSuccess) {
    LOG_ERR("蓝牙后端：写入完成回调错误: %d", status);
  }
}

@end

// ============================================================================
// BtBackendMacOS C++ 实现
// ============================================================================

BtBackendMacOS::BtBackendMacOS()
{
  m_impl = (__bridge void *)[[BtBackendImpl alloc] init];
  CFRetain(m_impl);
}

BtBackendMacOS::BtBackendMacOS(void *impl) : m_impl(impl), m_connected(true)
{
  // 从已接受的连接构造，impl 已被 retain
}

BtBackendMacOS::~BtBackendMacOS()
{
  close(); // 确保 RunLoop 线程停止后再释放 impl，避免 use-after-free
  if (m_impl != nullptr) {
    CFRelease(m_impl);
    m_impl = nullptr;
  }
}

void BtBackendMacOS::connect(const std::string &btAddress, int channel)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  NSString *nsAddr = [NSString stringWithUTF8String:btAddress.c_str()];

  // 启动 RunLoop 线程：openRFCOMMChannelSync 注册的 delegate 回调
  //（rfcommChannelData: 等）需要注册线程持续运行 RunLoop 才能派发。
  // EventQueue 主线程运行的是 EventQueue::loop()（非 RunLoop），
  // 若在主线程注册 delegate，数据回调永远不会触发，客户端收不到服务端数据。
  m_runLoopThread = std::thread([this, impl, nsAddr, channel]() {
    @autoreleasepool {
      [impl connectToDevice:nsAddr channel:channel];

      {
        std::lock_guard<std::mutex> lk(m_connectMutex);
        m_connected = impl->m_connected;
        if (m_connected) {
          CFRunLoopRef rl = CFRunLoopGetCurrent();
          // 加保活源：CFRunLoopRun 在 RunLoop 无任何源（input/timer/observer）时
          // 会立即返回，导致线程退出、current runloop 随线程释放（m_runLoop 悬空），
          // 且 IOBluetooth delegate 回调无法派发（客户端收不到服务端数据）。
          // 加一个空 source 让 RunLoop 持续运行。
          CFRunLoopSourceContext ctx = {};
          ctx.perform = +[](void *) {};
          m_runLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
          if (m_runLoopSource != nullptr) {
            CFRunLoopAddSource(rl, static_cast<CFRunLoopSourceRef>(m_runLoopSource), kCFRunLoopDefaultMode);
          }
          // retain runloop，避免线程退出后 current runloop 被释放导致 close() 悬空崩溃
          CFRetain(rl);
          m_runLoop = rl;
        }
        m_connectDone = true;
      }
      m_connectCV.notify_one();

      if (m_connected) {
        // 持续运行 RunLoop（已有保活源不会立即退出），派发 IOBluetooth 数据回调。
        // CFRunLoopRun 在 CFRunLoopStop 被调用后退出（见 close()）。
        CFRunLoopRun();
      }
    }
  });

  // 等待连接完成（openRFCOMMChannelSync 是同步的，连接结果在此可见）
  std::unique_lock<std::mutex> lk(m_connectMutex);
  m_connectCV.wait(lk, [this] { return m_connectDone; });
}

void BtBackendMacOS::listen(int channel)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  [impl listenOnChannel:channel];
  m_listening = impl->m_listening;
}

std::unique_ptr<BtBackend> BtBackendMacOS::accept()
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl acceptConnection];
}

int BtBackendMacOS::read(void *buf, size_t len)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl readData:buf length:len];
}

int BtBackendMacOS::write(const void *buf, size_t len)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl writeData:buf length:len];
}

void BtBackendMacOS::close()
{
  // 先关闭蓝牙连接：此时 RunLoop 线程尚未退出，connectToDevice 创建的 IOBluetooth
  // 对象（m_channel 等）尚未被 RunLoop 线程的 @autoreleasepool drain 释放。
  // 若先 CFRunLoopStop+join（RunLoop 线程退出会 drain autorelease），这些对象会先
  // 被释放，再 [impl closeConnection] 访问 m_channel 会野指针崩溃（objc_msgSend）。
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  [impl closeConnection];
  m_connected = false;
  m_listening = false;

  // 关闭连接后再停止 RunLoop 线程，确保不再有 IOBluetooth 回调访问 impl
  if (m_runLoopThread.joinable()) {
    if (m_runLoop != nullptr) {
      // 停止 RunLoop，使 CFRunLoopRun 返回、RunLoop 线程退出
      CFRunLoopStop(static_cast<CFRunLoopRef>(m_runLoop));
    }
    m_runLoopThread.join();
    // RunLoop 线程已退出，安全清理 RunLoop 资源（对应 connect 中的 Create/Retain）
    if (m_runLoopSource != nullptr) {
      CFRelease(m_runLoopSource);
      m_runLoopSource = nullptr;
    }
    if (m_runLoop != nullptr) {
      CFRelease(m_runLoop);
      m_runLoop = nullptr;
    }
  }
}

bool BtBackendMacOS::isConnected() const
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl isConnected];
}

bool BtBackendMacOS::isListening() const
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl isListening];
}

bool BtBackendMacOS::pollRead(int timeoutMs)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl pollRead:timeoutMs];
}

#endif // __APPLE__
