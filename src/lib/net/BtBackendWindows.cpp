/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 仅在 Windows 平台编译
#ifdef _WIN32

#include "net/BtBackendWindows.h"
#include "base/Log.h"

// Windows 蓝牙头文件
#include <winsock2.h>
#include <ws2bth.h>
#include <bluetoothapis.h>

#include <cstring>
#include <vector>

// Windows 平台工厂函数实现
std::unique_ptr<BtBackend> createBtBackend()
{
  return std::make_unique<BtBackendWindows>();
}

// Windows 蓝牙 RFCOMM socket 对 select() 的支持不可靠（不会报告可读就绪），
// 因此 BtDataSocket 的读路径改为：pollRead 恒返回 true，read 直接阻塞 recv，
// 由 SO_RCVTIMEO 提供超时——无数据时 recv 返回 WSAETIMEDOUT，read 视为"无数据"返回 0。
static constexpr DWORD kBtRecvTimeoutMs = 5;

// SPP 服务 UUID: 00001101-0000-1000-8000-00805F9B34FB（与 macOS 端一致）
// 用于 WSASetService 注册的 lpServiceClassId，使 Windows BT 栈视为合法 RFCOMM 服务。
static const GUID kSppServiceGuid = {
    0x00001101, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}};

// 备用：Deskflow 自定义 UUID（避免与 SPP 保留 UUID 冲突时使用）
// 6e1c3f40-1351-4b8f-9c2a-7c0d4e9b1110
static const GUID kDeskflowServiceGuid = {
    0x6e1c3f40, 0x1351, 0x4b8f, {0x9c, 0x2a, 0x7c, 0x0d, 0x4e, 0x9b, 0x11, 0x10}};

// 构造一个最小 SPP 风格的 SDP 服务记录（ServiceRecordHandle + ServiceClassIDList
// + ProtocolDescriptorList + ServiceName），用于让 Windows 客户端 BT 栈在连接后认为服务合法。
static std::vector<uint8_t> buildSppSdpRecord(uint8_t channel, const std::string &serviceName)
{
  std::vector<uint8_t> rec;

  // Attribute 0x0000 ServiceRecordHandle = UINT32(0x00000001)
  // （Windows SDP 注册接口要求记录必须含此属性；具体值由系统在注册时改写）
  static const uint8_t kAttrRecordHandle[] = {
      0x09, 0x00, 0x00, // UINT16 attr id 0x0000
      0x0A, 0x00, 0x00, 0x00, 0x01 // UINT32, value = 1
  };
  rec.insert(rec.end(), std::begin(kAttrRecordHandle), std::end(kAttrRecordHandle));

  // Attribute 0x0001 ServiceClassIDList = SEQ { UUID128(Deskflow custom) }
  // 使用自定义 UUID 6e1c3f40-1351-4b8f-9c2a-7c0d4e9b1110（避免与 SPP 保留 UUID 注册冲突）
  static const uint8_t kAttrServiceClassList[] = {
      0x09, 0x00, 0x01, // UINT16 attr id 0x0001
      0x35, 0x11,       // SEQUENCE, length 17
      0x1C,             // UUID128
      0x6E, 0x1C, 0x3F, 0x40, 0x13, 0x51, 0x4B, 0x8F, 0x9C, 0x2A, 0x7C, 0x0D, 0x4E, 0x9B, 0x11, 0x10};
  rec.insert(rec.end(), std::begin(kAttrServiceClassList), std::end(kAttrServiceClassList));

  // Attribute 0x0004 ProtocolDescriptorList = SEQ { SEQ{L2CAP}, SEQ{RFCOMM, UINT8(channel)} }
  rec.push_back(0x09);
  rec.push_back(0x00);
  rec.push_back(0x04); // UINT16 attr id 0x0004
  rec.push_back(0x35);
  rec.push_back(0x0C); // SEQUENCE, length 12
  rec.push_back(0x35);
  rec.push_back(0x03);                       // SEQUENCE, length 3
  rec.push_back(0x19);                       // UUID16
  rec.push_back(0x01);
  rec.push_back(0x00); // L2CAP UUID = 0x0100
  rec.push_back(0x35);
  rec.push_back(0x05);                       // SEQUENCE, length 5
  rec.push_back(0x19);                       // UUID16
  rec.push_back(0x00);
  rec.push_back(0x03); // RFCOMM UUID = 0x0003
  rec.push_back(0x08);                       // UINT8
  rec.push_back(channel);                    // channel

  // Attribute 0x0005 BrowseGroupList = SEQ { UUID16(PublicBrowseRoot=0x1002) }
  // （省略：BrowseGroupList 非必需，注册时减少出错面）

  // Attribute 0x0100 ServiceName = STRING (length must fit in 1 byte)
  rec.push_back(0x09);
  rec.push_back(0x01);
  rec.push_back(0x00);
  size_t nameLen = serviceName.size();
  if (nameLen > 255)
    nameLen = 255;
  rec.push_back(0x25); // STRING
  rec.push_back(static_cast<uint8_t>(nameLen));
  rec.insert(rec.end(), serviceName.data(), serviceName.data() + nameLen);

  return rec;
}

BtBackendWindows::BtBackendWindows()
{
}

void BtBackendWindows::applyRecvTimeout()
{
  if (m_socket == nullptr) {
    return;
  }
  const DWORD timeoutMs = kBtRecvTimeoutMs;
  if (setsockopt(reinterpret_cast<SOCKET>(m_socket), SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR) {
    LOG_WARN("蓝牙后端：设置 SO_RCVTIMEO 失败，错误码: %d", WSAGetLastError());
  }
}

BtBackendWindows::BtBackendWindows(void *acceptedSocket) : m_socket(acceptedSocket), m_connected(true)
{
  LOG_DEBUG("蓝牙后端：从已接受连接构造");
  applyRecvTimeout();
}

BtBackendWindows::~BtBackendWindows()
{
  close();
}

unsigned long long BtBackendWindows::parseMacAddress(const std::string &btAddress)
{
  unsigned long long addr = 0;
  unsigned int a, b, c, d, e, f;
  if (sscanf(btAddress.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X", &a, &b, &c, &d, &e, &f) != 6) {
    LOG_ERR("蓝牙后端：无法解析 MAC 地址: %s", btAddress.c_str());
    return 0;
  }
  // Windows BTH_ADDR 格式：高字节在前
  addr = ((unsigned long long)a << 40) | ((unsigned long long)b << 32) | ((unsigned long long)c << 24) |
         ((unsigned long long)d << 16) | ((unsigned long long)e << 8) | ((unsigned long long)f);
  return addr;
}

void BtBackendWindows::connect(const std::string &btAddress, int channel)
{
  LOG_INFO("蓝牙后端：正在连接到 %s 通道 %d", btAddress.c_str(), channel);
  LOG_INFO("蓝牙后端：m_connected=%d, m_socket=%p", m_connected, m_socket);

  SOCKET sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (sock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：创建 socket 失败，错误码: %d", WSAGetLastError());
    return;
  }
  m_socket = reinterpret_cast<void *>(sock);
  LOG_INFO("蓝牙后端：创建 socket 成功，sock=%d", sock);

  SOCKADDR_BTH remoteAddr = {};
  remoteAddr.addressFamily = AF_BTH;
  remoteAddr.btAddr = parseMacAddress(btAddress);
  remoteAddr.port = channel;
  remoteAddr.serviceClassId = GUID_NULL;

  LOG_INFO("蓝牙后端：调用 connect()");
  if (::connect(sock, (SOCKADDR *)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    LOG_ERR("蓝牙后端：连接失败，错误码: %d", err);
    closesocket(sock);
    m_socket = nullptr;
    m_connected = false;
    LOG_INFO("蓝牙后端：连接失败后 m_connected=%d", m_connected);
    return;
  }

  m_connected = true;
  applyRecvTimeout();
  LOG_INFO("蓝牙后端：已连接到 %s", btAddress.c_str());
  LOG_INFO("蓝牙后端：连接成功后 m_connected=%d", m_connected);
}

void BtBackendWindows::listen(int channel)
{
  LOG_INFO("蓝牙后端：正在通道 %d 上监听", channel);

  SOCKET sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (sock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：创建监听 socket 失败，错误码: %d", WSAGetLastError());
    return;
  }
  m_socket = reinterpret_cast<void *>(sock);

  SOCKADDR_BTH localAddr = {};
  localAddr.addressFamily = AF_BTH;
  localAddr.btAddr = 0; // 绑定到所有蓝牙适配器
  localAddr.port = channel;

  if (bind(reinterpret_cast<SOCKET>(m_socket), (SOCKADDR *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：绑定失败，错误码: %d", WSAGetLastError());
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
    return;
  }

  if (::listen(reinterpret_cast<SOCKET>(m_socket), 1) == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：监听失败，错误码: %d", WSAGetLastError());
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
    return;
  }

  m_listening = true;
  // 注册 SDP 服务记录：否则 Windows 客户端 BT 栈在建立连接约 1 秒后会因找不到 SDP
  // 而主动 abort 链路（客户端 recv 收到 WSAECONNABORTED 10053）。
  registerSdpService(channel);
  LOG_INFO("蓝牙后端：已在通道 %d 上监听", channel);
}

std::unique_ptr<BtBackend> BtBackendWindows::accept()
{
  if (!m_listening || m_socket == nullptr) {
    return nullptr;
  }

  SOCKADDR_BTH clientAddr = {};
  int clientAddrLen = sizeof(clientAddr);

  SOCKET clientSock = ::accept(reinterpret_cast<SOCKET>(m_socket), (SOCKADDR *)&clientAddr, &clientAddrLen);
  if (clientSock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：接受连接失败，错误码: %d", WSAGetLastError());
    return nullptr;
  }

  LOG_INFO("蓝牙后端：已接受蓝牙连接");
  return std::make_unique<BtBackendWindows>(reinterpret_cast<void *>(clientSock));
}

int BtBackendWindows::read(void *buf, size_t len)
{
  if (!m_connected || m_socket == nullptr) {
    return -1;
  }

  int result = recv(reinterpret_cast<SOCKET>(m_socket), static_cast<char *>(buf), static_cast<int>(len), 0);
  if (result == SOCKET_ERROR) {
    int err = WSAGetLastError();
    // WSAETIMEDOUT: SO_RCVTIMEO 超时，无数据可读（正常，继续轮询）
    // WSAEWOULDBLOCK: 非阻塞模式下无数据可读（保留兼容）
    if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
      return 0;
    }
    if (err == WSAENOTSOCK) {
      LOG_INFO("蓝牙后端：read 返回 WSAENOTSOCK，连接已关闭");
      m_connected = false;
      return -1;
    }
    LOG_ERR("蓝牙后端：读取失败，错误码: %d", err);
    m_connected = false;
    return -1;
  } else if (result == 0) {
    LOG_INFO("蓝牙后端：read 返回 0，连接已关闭");
    m_connected = false;
    return -1;
  }
  LOG_DEBUG("蓝牙后端：read 成功读取 %d 字节", result);
  return result;
}

int BtBackendWindows::write(const void *buf, size_t len)
{
  if (!m_connected || m_socket == nullptr) {
    return -1;
  }

  int result = send(reinterpret_cast<SOCKET>(m_socket), static_cast<const char *>(buf), static_cast<int>(len), 0);
  if (result == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：写入失败，错误码: %d", WSAGetLastError());
    m_connected = false;
    return -1;
  }
  return result;
}

void BtBackendWindows::close()
{
  unregisterSdpService();
  if (m_socket != nullptr) {
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
  }
  m_connected = false;
  m_listening = false;
}

bool BtBackendWindows::isConnected() const
{
  return m_connected;
}

bool BtBackendWindows::pollRead(int timeoutMs)
{
  // Windows 蓝牙 RFCOMM socket 的 select() 不可靠（不报告可读就绪），
  // 这里恒返回 true，由 read() 配合 SO_RCVTIMEO 完成实际的超时读取。
  (void)timeoutMs;
  return m_socket != nullptr;
}

bool BtBackendWindows::registerSdpService(int channel)
{
  if (m_sdpRecordHandle != nullptr) {
    return true; // 已注册
  }

  auto record = buildSppSdpRecord(static_cast<uint8_t>(channel), "Deskflow");

  ULONG sdpVersion = BTH_SDP_VERSION;
  HANDLE recordHandle = nullptr;

  // BTH_SET_SERVICE.pRecord 是变长数组（UCHAR[1]），分配时需在结构体末尾预留 record.size()
  // 个字节，将 SDP 记录字节紧贴结构体尾部拷贝。
  const size_t kHeadSize = sizeof(BTH_SET_SERVICE);
  std::vector<uint8_t> buf(kHeadSize + record.size(), 0);
  auto *bss = reinterpret_cast<BTH_SET_SERVICE *>(buf.data());

  bss->pSdpVersion = &sdpVersion;
  bss->pRecordHandle = &recordHandle;
  bss->fCodService = 0; // Reserved[5] 已被 vector 初始化为 0
  bss->ulRecordLength = static_cast<ULONG>(record.size());
  std::memcpy(bss->pRecord, record.data(), record.size());

  // BLOB.cbSize 必须为 sizeof(BTH_SET_SERVICE) + record.size() - 1
  // （pRecord[1] 已占用 1 字节，故 record.size() 字节贴在其后只需补 record.size()-1）
  BLOB blob = {};
  blob.cbSize = static_cast<ULONG>(sizeof(BTH_SET_SERVICE) + record.size() - 1);
  blob.pBlobData = reinterpret_cast<BYTE *>(bss);

  WSAQUERYSET wsaq = {};
  wsaq.dwSize = sizeof(WSAQUERYSET);
  wsaq.lpServiceClassId = const_cast<GUID *>(&kDeskflowServiceGuid);
  wsaq.dwNameSpace = NS_BTH;
  wsaq.lpBlob = &blob;

  if (WSASetService(&wsaq, RNRSERVICE_REGISTER, SERVICE_MULTIPLE) == SOCKET_ERROR) {
    // 部分机器（如本测试机 Win11 + Intel BT）上 WSASetService 对自构造的 SDP 记录返回
    // WSAEINVAL，根因未明（struct/记录字节已对照 MSDN 校验）。降级为 DEBUG 避免日志噪音，
    // 但保留代码以便后续在能复现的机器上进一步定位。
    LOG_DEBUG("蓝牙后端：注册 SDP 服务记录失败，错误码: %d", WSAGetLastError());
    return false;
  }

  m_sdpRecordHandle = recordHandle;
  LOG_INFO("蓝牙后端：已注册 SDP 服务记录（通道 %d，handle=%p）", channel, recordHandle);
  return true;
}

void BtBackendWindows::unregisterSdpService()
{
  if (m_sdpRecordHandle == nullptr) {
    return;
  }

  HANDLE handle = m_sdpRecordHandle;
  m_sdpRecordHandle = nullptr;

  ULONG sdpVersion = BTH_SDP_VERSION;

  BTH_SET_SERVICE bss = {};
  bss.pSdpVersion = &sdpVersion;
  bss.pRecordHandle = &handle;
  bss.ulRecordLength = 0; // 注销时只需 pRecordHandle，无需提供记录内容

  BLOB blob = {};
  blob.cbSize = sizeof(BTH_SET_SERVICE);
  blob.pBlobData = reinterpret_cast<BYTE *>(&bss);

  WSAQUERYSET wsaq = {};
  wsaq.dwSize = sizeof(WSAQUERYSET);
  wsaq.lpServiceClassId = const_cast<GUID *>(&kDeskflowServiceGuid);
  wsaq.dwNameSpace = NS_BTH;
  wsaq.lpBlob = &blob;

  if (WSASetService(&wsaq, RNRSERVICE_DELETE, 0) == SOCKET_ERROR) {
    LOG_WARN("蓝牙后端：注销 SDP 服务记录失败，错误码: %d", WSAGetLastError());
  }
}

#endif // _WIN32
