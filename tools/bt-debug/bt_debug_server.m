/*
 * bt_debug_server.m - macOS 蓝牙 RFCOMM 调试服务端
 *
 * 用途：在 macOS 上启动一个蓝牙 RFCOMM 监听服务，用于诊断 Windows 客户端连接问题。
 * 编译：clang -fobjc-arc -framework Foundation -framework IOBluetooth -o bt_debug_server bt_debug_server.m
 * 运行：./bt_debug_server [通道号]
 * 示例：./bt_debug_server 10
 */

#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

static volatile int g_running = 1;

static void signalHandler(int sig) {
    printf("\n[信号] 收到信号 %d，正在关闭...\n", sig);
    g_running = 0;
}

// ============================================================================
// 蓝牙调试服务端实现
// ============================================================================
@interface BtDebugServer : NSObject <IOBluetoothRFCOMMChannelDelegate>
{
    IOBluetoothRFCOMMChannel *_channel;
    IOBluetoothUserNotification *_channelNotification;
    IOBluetoothSDPServiceRecord *_sdpRecord;
    BOOL _connected;
    int _channelId;
}
@end

@implementation BtDebugServer

- (instancetype)initWithChannel:(int)channel
{
    self = [super init];
    if (self) {
        _channel = nil;
        _channelNotification = nil;
        _sdpRecord = nil;
        _connected = NO;
        _channelId = channel;
    }
    return self;
}

// ============================================================================
// 第一步：检查蓝牙适配器
// ============================================================================
- (BOOL)checkAdapter
{
    printf("\n=== 第 1 步：检查蓝牙适配器 ===\n");

    IOBluetoothHostController *hc = [IOBluetoothHostController defaultController];
    if (hc == nil) {
        printf("[错误] 未找到蓝牙适配器！\n");
        return NO;
    }

    // 获取适配器地址
    NSString *addrStr = [hc addressAsString];
    if (addrStr) {
        printf("[OK] 蓝牙适配器地址: %s\n", [addrStr UTF8String]);
    } else {
        printf("[警告] 无法获取适配器地址\n");
    }

    // 检查电源状态
    BluetoothHCIPowerState powerState = [hc powerState];
    const char *stateStr = "未知";
    switch (powerState) {
        case kBluetoothHCIPowerStateOFF: stateStr = "关闭"; break;
        case kBluetoothHCIPowerStateON: stateStr = "开启"; break;
        default: break;
    }
    printf("[OK] 蓝牙电源状态: %s (%d)\n", stateStr, powerState);

    if (powerState != kBluetoothHCIPowerStateON) {
        printf("[错误] 蓝牙未开启！请在系统设置中打开蓝牙\n");
        return NO;
    }

    printf("[OK] 蓝牙适配器已就绪\n");
    return YES;
}

// ============================================================================
// 第二步：列出已配对设备
// ============================================================================
- (void)listPairedDevices
{
    printf("\n=== 第 2 步：已配对的蓝牙设备 ===\n");

    NSArray *devices = [IOBluetoothDevice pairedDevices];
    if (devices == nil || devices.count == 0) {
        printf("[警告] 没有找到已配对的蓝牙设备\n");
        printf("  -> Windows 端需要先与 Mac 配对才能连接\n");
        printf("  -> 配对步骤：Windows 设置 -> 蓝牙 -> 添加设备 -> 选择 Mac\n");
        return;
    }

    printf("[OK] 找到 %lu 个已配对设备：\n", (unsigned long)devices.count);
    for (IOBluetoothDevice *device in devices) {
        printf("  - %s (地址: %s, 已连接: %s, 已配对: %s)\n",
               [[device nameOrAddress] UTF8String],
               [[device addressString] UTF8String],
               [device isConnected] ? "是" : "否",
               [device isPaired] ? "是" : "否");
    }
}

// ============================================================================
// 第三步：注册 RFCOMM 通道通知
// ============================================================================
- (BOOL)registerChannelNotification
{
    printf("\n=== 第 3 步：注册 RFCOMM 通道通知 ===\n");
    printf("[信息] 正在通道 %d 上注册 RFCOMM 连接通知...\n", _channelId);

    _channelNotification = [IOBluetoothRFCOMMChannel
        registerForChannelOpenNotifications:self
                                  selector:@selector(rfcommChannelOpened:channel:)
                                withChannelID:_channelId
                                  direction:kIOBluetoothUserNotificationChannelDirectionIncoming];

    if (_channelNotification == nil) {
        printf("[错误] 注册 RFCOMM 通道通知失败！\n");
        printf("  -> 可能原因：通道 %d 已被其他程序占用\n", _channelId);
        printf("  -> 建议：尝试使用其他通道号（如 5, 10, 15）\n");
        return NO;
    }

    printf("[OK] RFCOMM 通道通知注册成功\n");
    return YES;
}

// ============================================================================
// 第四步：发布 SDP 服务记录
// ============================================================================
- (BOOL)publishSDPService
{
    printf("\n=== 第 4 步：发布 SDP 服务记录 ===\n");
    printf("[信息] 正在发布 SDP 服务记录...\n");

    // 使用标准 SPP UUID: 00001101-0000-1000-8000-00805F9B34FB
    uint8_t sppUUIDBytes[] = {
        0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    };
    IOBluetoothSDPUUID *sppUUID = [IOBluetoothSDPUUID uuidWithBytes:sppUUIDBytes length:16];
    if (sppUUID == nil) {
        printf("[错误] 创建 SPP UUID 失败\n");
        return NO;
    }

    IOBluetoothSDPUUID *l2capUUID = [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16L2CAP];
    IOBluetoothSDPUUID *rfcommUUID = [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16RFCOMM];

    // 直接使用原始类型构建 SDP 字典（不需要 IOBluetoothSDPDataElement 包装）
    NSDictionary *serviceDict = @{
        @"0001" : @[ sppUUID ],
        @"0004" : @[
            @[ l2capUUID ],
            @[ rfcommUUID, @(_channelId) ],
        ],
        @"0100" : @"Deskflow Debug Service",
    };

    _sdpRecord = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:serviceDict];
    if (_sdpRecord == nil) {
        printf("[错误] 创建 SDP 服务记录失败！\n");
        return NO;
    }

    printf("[OK] SDP 服务记录发布成功（通道 %d）\n", _channelId);
    return YES;
}

// ============================================================================
// 第五步：开始监听
// ============================================================================
- (BOOL)start
{
    // 步骤 1：检查适配器
    if (![self checkAdapter]) {
        return NO;
    }

    // 步骤 2：列出设备
    [self listPairedDevices];

    // 步骤 3：注册通知
    if (![self registerChannelNotification]) {
        return NO;
    }

    // 步骤 4：发布 SDP
    if (![self publishSDPService]) {
        printf("[警告] SDP 发布失败，但通知已注册，继续监听...\n");
    }

    printf("\n========================================\n");
    printf("  蓝牙调试服务端已启动！\n");
    printf("  监听通道: %d\n", _channelId);
    printf("  等待 Windows 客户端连接...\n");
    printf("========================================\n\n");

    printf("[提示] 现在请在 Windows 端尝试连接\n");
    printf("[提示] Windows 端连接参数：\n");
    printf("       - Mac 蓝牙地址: 见上方适配器地址\n");
    printf("       - RFCOMM 通道: %d\n", _channelId);
    printf("[提示] 按 Ctrl+C 退出\n\n");

    return YES;
}

- (void)stop
{
    printf("[信息] 正在停止调试服务...\n");

    if (_channelNotification != nil) {
        [_channelNotification unregister];
        _channelNotification = nil;
        printf("[OK] 已取消通道通知注册\n");
    }

    if (_sdpRecord != nil) {
        [_sdpRecord removeServiceRecord];
        _sdpRecord = nil;
        printf("[OK] 已移除 SDP 服务记录\n");
    }

    if (_channel != nil) {
        [_channel closeChannel];
        _channel = nil;
        printf("[OK] 已关闭 RFCOMM 通道\n");
    }

    _connected = NO;
}

// ============================================================================
// RFCOMM 通道打开回调（新连接）
// ============================================================================
- (void)rfcommChannelOpened:(IOBluetoothUserNotification *)notification
                    channel:(IOBluetoothRFCOMMChannel *)newChannel
{
    printf("\n!!! 收到新的蓝牙连接 !!!\n");
    printf("========================================\n");

    IOBluetoothDevice *device = [newChannel getDevice];
    if (device) {
        printf("[连接] 设备名称: %s\n", [[device nameOrAddress] UTF8String]);
        printf("[连接] 设备地址: %s\n", [[device addressString] UTF8String]);
    }

    printf("[连接] RFCOMM 通道 ID: %d\n", [newChannel getChannelID]);
    printf("[连接] MTU: %d\n", (int)[newChannel getMTU]);

    _channel = newChannel;
    [_channel setDelegate:self];
    _connected = YES;

    printf("[OK] 连接已建立！\n");
    printf("========================================\n\n");

    // 发送欢迎消息
    const char *welcome = "Hello from macOS Deskflow Debug Server!\n";
    [_channel writeSync:(void *)welcome length:(UInt16)strlen(welcome)];
    printf("[发送] 已发送欢迎消息到客户端\n");
}

// ============================================================================
// RFCOMM 数据接收回调
// ============================================================================
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                   device:(IOBluetoothDevice *)device
                 channelID:(BluetoothRFCOMMChannelID)channelID
                     data:(void *)dataPointer
               dataLength:(NSUInteger)dataLength
{
    const uint8_t *bytes = (const uint8_t *)dataPointer;

    printf("[接收] 收到 %zu 字节数据：", dataLength);

    // 打印十六进制
    NSUInteger printLen = dataLength < 128 ? dataLength : 128;
    for (NSUInteger i = 0; i < printLen; ++i) {
        if (i > 0) printf(" ");
        printf("%02X", bytes[i]);
    }
    if (dataLength > 128) printf(" ...");
    printf("\n");

    // 如果是可打印文本，也打印文本
    if (dataLength > 0 && dataLength < 1024) {
        printf("[接收] 文本: ");
        for (NSUInteger i = 0; i < dataLength; ++i) {
            if (bytes[i] >= 0x20 && bytes[i] < 0x7F) {
                printf("%c", bytes[i]);
            } else {
                printf("\\x%02X", bytes[i]);
            }
        }
        printf("\n");
    }

    // 回显数据
    if (_connected && _channel != nil) {
        [_channel writeSync:(void *)dataPointer length:(UInt16)dataLength];
        printf("[发送] 已回显 %zu 字节\n", dataLength);
    }
}

// ============================================================================
// RFCOMM 通道关闭回调
// ============================================================================
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel
{
    printf("\n[断开] RFCOMM 通道已关闭\n");
    _connected = NO;
    _channel = nil;
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel
                           refCon:(void *)refCon
                            status:(IOReturn)status
{
    if (status != kIOReturnSuccess) {
        printf("[错误] 写入完成回调错误: 0x%X\n", status);
    }
}

@end

// ============================================================================
// 主函数
// ============================================================================
int main(int argc, const char *argv[])
{
    @autoreleasepool {
        // 注册信号处理
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        // 解析通道号
        int channel = 10;
        if (argc > 1) {
            channel = atoi(argv[1]);
            if (channel < 1 || channel > 30) {
                printf("[错误] 通道号必须在 1-30 范围内，当前: %d\n", channel);
                return 1;
            }
        }

        printf("╔══════════════════════════════════════════════╗\n");
        printf("║   Deskflow 蓝牙 RFCOMM 调试服务端           ║\n");
        printf("║   版本: 1.0                                 ║\n");
        printf("╚══════════════════════════════════════════════╝\n");
        printf("\n[信息] 使用通道: %d\n", channel);

        // 创建服务端
        BtDebugServer *server = [[BtDebugServer alloc] initWithChannel:channel];

        // 启动服务
        if (![server start]) {
            printf("\n[错误] 服务启动失败，请检查上方错误信息\n");
            return 1;
        }

        // 运行事件循环
        printf("[信息] 进入事件循环（主线程）\n");
        while (g_running) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                      beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.5]];
        }

        // 停止服务
        [server stop];
        printf("\n[信息] 调试服务已停止\n");
    }
    return 0;
}
