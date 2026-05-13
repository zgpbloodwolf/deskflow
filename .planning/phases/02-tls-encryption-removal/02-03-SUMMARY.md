---
phase: 02-tls-encryption-removal
plan: 03
subsystem: gui, common
tags: [tls-removal, settings, gui, ipc, statusbar, cleanup]

# 依赖图
requires:
  - phase: 02-02
    provides: "TLS 文件已删除，需要清理引用这些文件的代码"
provides:
  - "Settings 无 Security struct 和 TLS 方法，GUI 无 TLS 引用"
  - "StatusBar 无安全图标/指纹按钮，CoreProcess 无 TLS IPC"
  - "SettingsDialog 无 TLS 控件，Logger 无 TLS 过滤条目"
affects: [03-platform-feature-cleanup, "编译清理完成，GUI 可构建"]

# 技术追踪
tech-stack:
  added: []
  patterns:
    - "从 Settings 中移除整个 Security struct 及其所有键"
    - "从 StatusBar 中移除安全图标和指纹按钮，简化为纯状态显示"
    - "从 CoreProcess 中移除 secureSocket/peerFingerprint IPC 处理"
    - "从 SettingsDialog 中移除 TLS 组框及全部相关控件"

key-files:
  created: []
  modified:
    - src/lib/common/Settings.h
    - src/lib/common/Settings.cpp
    - src/lib/gui/MainWindow.h
    - src/lib/gui/MainWindow.cpp
    - src/lib/gui/widgets/StatusBar.h
    - src/lib/gui/widgets/StatusBar.cpp
    - src/lib/gui/core/CoreProcess.h
    - src/lib/gui/core/CoreProcess.cpp
    - src/lib/gui/dialogs/SettingsDialog.h
    - src/lib/gui/dialogs/SettingsDialog.cpp
    - src/lib/gui/dialogs/SettingsDialog.ui
    - src/lib/gui/Messages.cpp
    - src/lib/gui/Logger.cpp

decisions:
  - "StatusBar 保留 updateFound() 信号用于版本更新通知"
  - "Messages::showNewClientPrompt() 简化为总是显示完整对话框（含 ignore 选项）"
  - "CoreProcess 保留 missingKeyboardLayouts 信号（非 TLS 相关）"

metrics:
  duration: 5min
  completed: 2026-05-13
---

# Phase 02 Plan 03: Strip TLS References from Settings, GUI, and IPC Summary

从 Settings、GUI 组件和 IPC 消息处理中移除所有 TLS/加密引用，完成端到端 TLS 清理。

## Changes Made

### Settings (common)
- 移除 `Settings::Security` struct（含 TlsEnabled、CheckPeers、Certificate、KeySize 四个键）
- 从 `m_validKeys` 列表中移除所有 4 个 Security 键
- 从 `m_defaultTrueValues` 列表中移除 TlsEnabled 和 CheckPeers
- 移除 `tlsDir()`、`tlsTrustedServersDb()`、`tlsTrustedClientsDb()` 方法声明和实现
- 从 `defaultValue()` 中移除 Security::Certificate 和 Security::KeySize 默认值分支

### MainWindow (gui)
- 移除 `#include "net/Fingerprint.h"`、`#include "dialogs/FingerprintDialog.h"`、`#include "gui/TlsUtility.h"`、`#include "net/FingerprintDatabase.h"`
- 移除构造函数中的 TLS 证书生成块和 `m_statusBar->setSecurityIcon()` 调用
- 移除 `secureSocket(false)` 初始调用
- 移除 3 个 TLS signal connections（secureSocket、securityLevelChanged、peerFingerprint）
- 移除 requestShowMyFingerprints 信号连接
- 移除 settingsChanged 中的 Security key 处理块
- 移除方法实现：updateSecurityIcon()、secureSocket()、handlePeerFingerprint()、trustedFingerprintDatabase()、generateCertificate()、showMyFingerprint()、updateLocalFingerprint()
- 移除成员：m_secureSocket、m_fingerprint、m_checkedClients、m_checkedServers

### StatusBar (gui/widgets)
- 移除安全图标（m_lblSecurityIcon）和指纹按钮（m_btnFingerprint）的创建和布局
- 移除方法：setSecurityIconVisible()、securityIconVisible()、updateSecurityInfo()、setSecurityIcon()、setSecurityLevel()、setBtnFingerprintVisible()
- 移除信号：requestShowMyFingerprints
- 移除成员：m_btnFingerprint、m_lblSecurityIcon、m_encrypted、m_securityLevel
- setStatus() 中移除所有 setSecurityIconVisible() 调用

### CoreProcess (gui/core)
- 移除信号：secureSocket(bool)、securityLevelChanged(QString)、peerFingerprint(QString)
- 移除 IPC 处理器：secureSocket 和 peerFingerprint 命令分支
- 移除方法：checkSecureSocket() 及 tlsCheckString 常量
- 移除成员：m_secureSocketVersion 和 secureSocketVersion() getter

### SettingsDialog (gui/dialogs)
- 移除 `#include "gui/TlsUtility.h"`
- 移除方法：updateTlsControls()、updateTlsControlsEnabled()、updateRequestedKeySize()、updateKeyLengthOnFile()、regenCertificates()、browseCertificatePath()
- 移除 TLS 控件的信号连接（groupSecurity、btnTlsRegenCert、comboTlsKeyLength、btnTlsCertPath、cbRequireClientCert、lineTlsCertPath）
- 从 accept() 中移除 TLS 设置保存
- 从 isModified() 中移除 TLS dirty check
- 从 isDefault() 中移除 TLS defaults check
- 从 updateControls() 中移除 TLS 控件启用/禁用逻辑

### SettingsDialog.ui
- 移除整个 groupSecurity QGroupBox 及其所有子控件（lineTlsCertPath、comboTlsKeyLength、btnTlsRegenCert、cbRequireClientCert、lblTlsCertInfo、lblTlsCert、widgetTlsCert、btnTlsCertPath、lblTlsKeyLength）

### Messages.cpp
- 简化 showNewClientPrompt()：移除 TLS peer auth 条件分支，始终显示含 "Ignore" 和 "Add client" 按钮的完整对话框

### Logger.cpp
- 从 kForceDebugMessages 中移除 3 个 TLS 相关条目：
  - "No functional TLS backend was found"
  - "No TLS backend is available"
  - "QSslSocket::connectToHostEncrypted: TLS initialization failed"

## Deviations from Plan

None - plan executed exactly as written.

## Verification Results

All grep checks passed with zero matches for TLS-related patterns across all 13 modified files.

## Self-Check: PASSED

- All 13 modified files exist on disk
- Commit edd051f found in git log
- SUMMARY.md created at expected path
