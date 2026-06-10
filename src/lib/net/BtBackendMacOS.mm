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
/*!
封装 IOBluetooth API 调用，对 C++ 层隐藏 Objective-C 类型。
*/
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
}

- (BOOL)connectToDevice:(NSString *)address channel:(int)channel;
- (BOOL)listenOnChannel:(int)channel;
- (std::unique_ptr<BtBackend>)acceptConnection;
- (int)readData:(void *)buf length:(size_t)len;
- (int)writeData:(const void *)buf length:(size_t)len;
- (void)closeConnection;
- (BOOL)isConnected;
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

  // 通过地址字符串查找设备
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

  // 打开 RFCOMM 通道
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

  // SPP UUID: 0x1101
  const uint8_t sppUUIDBytes[] = {0x11, 0x01};
  IOBluetoothSDPUUID *sppUUID = [IOBluetoothSDPUUID uuidWithBytes:sppUUIDBytes length:2];

  // L2CAP UUID: 0x0100, RFCOMM UUID: 0x0003
  const uint8_t l2capUUIDBytes[] = {0x01, 0x00};
  const uint8_t rfcommUUIDBytes[] = {0x00, 0x03};
  IOBluetoothSDPUUID *l2capUUID = [IOBluetoothSDPUUID uuidWithBytes:l2capUUIDBytes length:2];
  IOBluetoothSDPUUID *rfcommUUID = [IOBluetoothSDPUUID uuidWithBytes:rfcommUUIDBytes length:2];

  NSDictionary *serviceDict = @{
    @"ServiceClassIDList" : @[ sppUUID ],
    @"ProtocolDescriptorList" : @[
      @[ l2capUUID ],
      @[ rfcommUUID, [NSNumber numberWithInt:channel] ]
    ],
    @"ServiceName" : @"Deskflow KVM"
  };

  // 使用 publishedServiceRecordWithDictionary 注册 SDP 服务
  IOBluetoothSDPServiceRecord *sdpRecord = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:serviceDict];
  if (sdpRecord == nil) {
    LOG_ERR("蓝牙后端：注册 SDP 服务失败");
    return NO;
  }

  m_listening = YES;
  LOG_INFO("蓝牙后端：已在通道 %d 上监听", channel);
  return YES;
}

- (std::unique_ptr<BtBackend>)acceptConnection
{
  // macOS 的 IOBluetooth 通道接受通过 delegate 回调处理
  // 这里使用简化的同步等待模式
  LOG_ERR("蓝牙后端：macOS 暂不支持同步 accept，请使用回调模式");
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
  if (m_channel != nil) {
    [m_channel closeChannel];
    m_channel = nil;
  }
  m_connected = NO;
  m_listening = NO;

  // 唤醒可能在等待读取的线程
  std::lock_guard<std::mutex> lock(m_bufferMutex);
  m_bufferCV.notify_all();
}

- (BOOL)isConnected
{
  return m_connected;
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
  // 手动 retain 以持有所有权
  CFRetain(m_impl);
}

BtBackendMacOS::~BtBackendMacOS()
{
  if (m_impl != nullptr) {
    // 释放 Objective-C 对象
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

bool BtBackendMacOS::pollRead(int timeoutMs)
{
  auto *impl = (__bridge BtBackendImpl *)m_impl;
  return [impl pollRead:timeoutMs];
}

#endif // __APPLE__
