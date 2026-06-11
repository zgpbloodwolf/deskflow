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
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                   device:(IOBluetoothDevice *)device
                 channelID:(BluetoothRFCOMMChannelID)channelID
                     data:(void *)dataPointer
               dataLength:(NSUInteger)dataLength;
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
  }
  return self;
}

- (void)dealloc
{
  [self closeConnection];
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
  if (result != kIOReturnSuccess || newChannel == nil) {
    LOG_ERR("蓝牙后端：打开 RFCOMM 通道失败，错误码: %d", result);
    return NO;
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
  LOG_INFO("蓝牙后端：收到新的蓝牙连接");

  std::lock_guard<std::mutex> lock(m_acceptMutex);
  if (m_hasPendingConnection && m_pendingChannel != nil) {
    // 已有待处理的连接，拒绝新连接
    [newChannel closeChannel];
    LOG_WARN("蓝牙后端：已有待处理连接，拒绝新连接");
    return;
  }

  m_pendingChannel = newChannel;
  [m_pendingChannel setDelegate:self];
  m_hasPendingConnection = YES;
  m_acceptCV.notify_one();
}

- (std::unique_ptr<BtBackend>)acceptConnection
{
  // 等待传入连接（超时 500ms，以便线程可以检查 m_running）
  std::unique_lock<std::mutex> lock(m_acceptMutex);
  if (m_acceptCV.wait_for(lock, std::chrono::milliseconds(500), [&] { return m_hasPendingConnection; })) {
    if (m_pendingChannel != nil) {
      // 创建新的 impl 对象并设置通道
      BtBackendImpl *newImpl = [[BtBackendImpl alloc] init];
      newImpl->m_channel = m_pendingChannel;
      newImpl->m_connected = YES;
      [newImpl->m_channel setDelegate:newImpl];

      m_pendingChannel = nil;
      m_hasPendingConnection = NO;

      // 必须显式 retain，防止 ARC 在当前 autorelease pool 释放 newImpl
      // BtBackendMacOS 的析构函数会通过 CFRelease 释放
      CFRetain((__bridge CFTypeRef)newImpl);

      // 创建 C++ 后端实例，转移所有权
      std::unique_ptr<BtBackendMacOS> backend(new BtBackendMacOS((__bridge void *)newImpl));

      LOG_INFO("蓝牙后端：已接受蓝牙连接");
      return backend;
    }
  }
  return nullptr;
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

  IOReturn result = [m_channel writeSync:(void *)buf length:static_cast<UInt16>(len)];
  if (result != kIOReturnSuccess) {
    LOG_ERR("蓝牙后端：写入失败，错误码: %d", result);
    m_connected = NO;
    return -1;
  }
  return static_cast<int>(len);
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
                   device:(IOBluetoothDevice *)device
                 channelID:(BluetoothRFCOMMChannelID)channelID
                     data:(void *)dataPointer
               dataLength:(NSUInteger)dataLength
{
  std::lock_guard<std::mutex> lock(m_bufferMutex);
  auto *bytes = static_cast<const uint8_t *>(dataPointer);
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
  if (m_impl != nullptr) {
    CFRelease(m_impl);
    m_impl = nullptr;
  }
}

void BtBackendMacOS::connect(const std::string &btAddress, int channel)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  NSString *nsAddr = [NSString stringWithUTF8String:btAddress.c_str()];
  [impl connectToDevice:nsAddr channel:channel];
  m_connected = impl->m_connected;
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
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  [impl closeConnection];
  m_connected = false;
  m_listening = false;
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
