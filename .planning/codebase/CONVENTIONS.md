# Coding Conventions

**Analysis Date:** 2026-05-12

## Language and Standard

- **C++20** with extensions off (`CMAKE_CXX_EXTENSIONS OFF`) and standard required
- Build system: CMake 3.24+
- Qt 6.7+ for GUI, networking, and test framework

## Naming Patterns

**Files:**
- PascalCase for class headers and implementations: `Clipboard.h`, `Clipboard.cpp`
- Test files use PascalCase with `Tests` suffix: `ClipboardTests.cpp`, `ClipboardTests.h`
- Mock files prefixed with `Mock`: `MockKeyState.h`, `MockEventQueue.h`
- Interface files prefixed with `I`: `IEventQueue.h`, `IClipboard.h`
- Platform-specific files prefixed with platform: `MSWindowsScreen.cpp`, `OSXKeyState.cpp`, `XWindowsClipboard.cpp`

**Classes:**
- PascalCase: `BaseException`, `SocketException`, `Clipboard`
- Interface classes prefixed with `I`: `IEventQueue`, `IClipboard`, `IScreen`
- Mock classes prefixed with `Mock`: `MockKeyMap`, `MockKeyState`, `MockEventQueue`
- Exception classes suffixed with `Exception`: `SocketException`, `IOException`, `MTException`

**Functions/Methods:**
- camelCase: `formatWithArgs()`, `findBestKey()`, `sendKeyEvent()`
- Test method names: descriptive camelCase with underscores separating scenario and expectation: `findBestKey_requiredDown_matchExactFirstItem()`, `fakeKeyRepeat_invalidKey_returnsFalse()`

**Member Variables:**
- Prefixed with `m_`: `m_open`, `m_time`, `m_data`, `m_key`, `m_arch`, `m_log`
- Static members prefixed with `s_`: `s_log`, `s_stubKeystroke`
- Constants use `k` prefix (test data): `kTestString1`, `kRetryDelay`

**Namespaces:**
- `deskflow` for core library code: `deskflow::string`, `deskflow::gui`
- `deskflow::server` for server-specific code
- `deskflow::test` for test utilities

**Enums:**
- PascalCase enum classes: `enum class SocketError`, `enum class KeyType`, `enum class Format`
- Enum values are PascalCase: `Unknown`, `NotFound`, `NoAddress`

## Code Style

**Formatting (`.clang-format`):**
- Based on: LLVM
- Column limit: 120
- Brace wrapping: Custom (see below)
  - After class/struct/enum/union/namespace(extern): true
  - After function: true
  - After control statement/Before catch/Before else: Never
- AllowShortFunctionsOnASingleLine: None
- AlignAfterOpenBracket: BlockIndent
- PackConstructorInitializers: CurrentLine

**Header Guards:**
- Use `#pragma once` exclusively (no `#ifndef` guards)

**Linting/Quality:**
- SonarCloud integration configured (`sonar-project.properties`)
- cspell for spell checking (`cspell.json`)
- No `.clang-tidy` file detected

## File Header

Every source file must begin with a comment block containing:

```cpp
/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) <year> <copyright holder>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
```

Multiple `SPDX-FileCopyrightText` lines are used to track different copyright holders over time. Use the year range appropriate for the file's history.

## Import Organization

**Order in `.cpp` files:**
1. Corresponding header (first): `#include "ServerTests.h"`
2. Project headers (quoted, alphabetical): `#include "deskflow/KeyMap.h"`
3. Mock headers (quoted): `#include "MockKeyState.h"`
4. System/stdlib headers (angle brackets): `#include <algorithm>`
5. Qt headers (angle brackets): `#include <QDir>`, `#include <QTest>`

**Order in `.h` files:**
1. `#pragma once`
2. Project headers (quoted): `#include "base/BaseException.h"`
3. System/stdlib headers (angle brackets): `#include <string>`
4. Qt headers (angle brackets): `#include <QString>`

**Include path convention:**
- Project headers use path relative to `src/lib/`: `#include "deskflow/Clipboard.h"`, `#include "base/Log.h"`
- Headers within the same library can use relative paths: `#include "Fingerprint.h"` (within net library)

## Error Handling

**Exception Hierarchy:**
- `BaseException` (extends `std::runtime_error`) -- root of all exceptions
  - Defined in `src/lib/base/BaseException.h`
  - Uses `QString` for message formatting
  - Supports `getWhat()` virtual method for lazy message formatting
  - Subclasses: `IOException`, `SocketException`, `MTException`, `ScreenException`, `ThreadExitException`
- Exception classes in their own header files per domain: `SocketException.h`, `IOException.h`

**Throwing Pattern:**
- Throw specific exception types wrapping lower-level errors:
```cpp
try {
  // arch-level operation
} catch (const ArchNetworkException &e) {
  throw SocketCreateException(e.what());
}
```

**Catching Pattern:**
- Catch by const reference: `catch (const ArchNetworkException &e)`
- Log and rethrow/wrap at appropriate layers
- Use `throw()` specifier on destructors of exception classes

**Using `using` for constructor delegation:**
```cpp
class SocketBindException : public SocketWithWhatException
{
  using SocketWithWhatException::SocketWithWhatException;
protected:
  QString getWhat() const throw() override;
};
```

## Logging

**Framework:** Custom `Log` class (`src/lib/base/Log.h`)

**Macros (preferred):**
```cpp
LOG_DEBUG("opening new socket: %08X", m_socket);
LOG_WARN("error closing socket: %s", e.what());
LOG_ERR("isFatal() not valid for non-secure connections");
LOG_INFO("...");
LOG_CRIT("...");
```

**Older macro style (still present):**
```cpp
LOG((CLOG_DEBUG "opening new socket: %08X", m_socket));
LOG((CLOG_WARN "error closing socket: %s", e.what()));
```

**Log Levels (in order):**
- `PRINT` -- unfiltered, no file/line prefix
- `CRIT`, `ERR`, `WARN`, `NOTE`, `INFO`, `DEBUG`, `DEBUG1`, `DEBUG2`

**Key behavior:**
- Singleton access via `CLOG` macro or `Log::getInstance()`
- Debug builds automatically include file/line info
- Release builds use `NDEBUG` define

## Comments

**Doxygen-style documentation:**
```cpp
//! Memory buffer clipboard
/*!
This class implements a clipboard that stores data in memory.
*/
class Clipboard : public IClipboard
```

**Method documentation:**
```cpp
//! Unmarshall clipboard data
/*!
Extract marshalled clipboard data and store it in this clipboard.
Sets the clipboard time to \c time.
*/
void unmarshall(const std::string &data, Time time);
```

**Qt-style grouping in headers:**
```cpp
//! @name manipulators
//@{
// ... methods
//@}
//! @name accessors
//@{
// ... methods
//@}
```

**Inline comments:** Use `//` for single-line explanations, `/* */` for block comments

**Member documentation:** Use `//!<` after member:
```cpp
KeyID m_id{};                  //!< KeyID
int32_t m_group{};             //!< Group for key
KeyModifierMask m_required{};  //!< Modifiers required for KeyID
```

## Function Design

**Virtual Methods:**
- Always mark overrides with `override`
- Use `final` when inheritance should stop
- Virtual destructors use `= default`

**Deleted/Defaulted:**
- Non-copyable/non-movable classes use `= delete`:
```cpp
Log(Log const &) = delete;
Log(Log &&) = delete;
Log &operator=(Log const &) = delete;
```

**Const Correctness:**
- Use `const` on methods that do not modify state
- Use `noexcept` on move constructors and destructors where appropriate
- Use `throw()` (deprecated but used here) on exception class destructors

**noexcept:**
- Used on swap operations: `void swap(KeyMap &) noexcept override`

## Module Design

**Namespaces:**
- Core library code lives in `namespace deskflow` or `namespace deskflow::string`
- GUI code lives in `namespace deskflow::gui`
- Server-specific code in `namespace deskflow::server`

**Interface Pattern:**
- Abstract interfaces use `I` prefix: `IEventQueue`, `IClipboard`, `IScreen`
- Concrete implementations have no prefix: `Clipboard`, `EventQueue`

**Qt Integration:**
- Use `Q_OBJECT` macro in QObject subclasses
- Use `Q_SIGNAL`/`Q_SLOT`/`Q_EMIT` instead of `signals`/`slots`/`emit` (enforced by `QT_NO_KEYWORDS`)
- Use `private Q_SLOTS:` for test methods in Qt Test classes

**Test class pattern (modern QTest):**
```cpp
class MyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();  // runs once before all tests
  void testName();      // individual test
private:
  // test data members
};
```

## Qt MOC and Code Generation

- `CMAKE_AUTOMOC ON`, `CMAKE_AUTOUIC ON`, `CMAKE_AUTORCC ON`
- All QObject-derived classes in headers are auto-processed by MOC

---

*Convention analysis: 2026-05-12*
