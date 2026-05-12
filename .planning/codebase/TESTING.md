# Testing Patterns

**Analysis Date:** 2026-05-12

## Test Framework

**Primary Runner (modern tests):**
- Qt Test (QTest) via `Qt6::Test` component
- Config: `src/unittests/CMakeLists.txt`

**Legacy Runner:**
- Google Test (gtest 1.15.2) + Google Mock (gmock)
- Fetched via CMake FetchContent in `src/unittests/legacytests/CMakeLists.txt`

**Assertion Libraries:**
- Modern tests: QTest macros (`QCOMPARE`, `QVERIFY`, `QSignalSpy`)
- Legacy tests: gtest/gmock macros (`EXPECT_CALL`, `ON_CALL`, `EXPECT_EQ`, `ASSERT_TRUE`)

**Run Commands:**
```bash
# Build and run all tests (post-build, default)
cmake --build build

# Run tests manually via ctest
ctest --test-dir build/src/unittests --output-on-failure

# Skip build-time tests
cmake -DSKIP_BUILD_TESTS=ON ..

# Disable tests entirely
cmake -DBUILD_TESTS=OFF ..
```

## Test Organization

### Two Test Systems

The project has two coexisting test systems:

1. **Modern Qt Test-based tests** (preferred for new tests):
   - Location: `src/unittests/{base,common,deskflow,gui,net,platform,server}/`
   - Each test is a standalone executable built with the `create_test()` CMake function
   - Each has a `.h` header declaring the test class and a `.cpp` with implementations

2. **Legacy Google Test-based tests**:
   - Location: `src/unittests/legacytests/`
   - Single monolithic executable (`legacytests`) linked against all test files
   - Uses shared test infrastructure in `src/unittests/legacytests/shared/` and mocks in `src/unittests/legacytests/mock/`

### Directory Layout

```
src/unittests/
├── CMakeLists.txt                 # Top-level test config, create_test() function
├── base/                          # Tests for src/lib/base
│   ├── CMakeLists.txt
│   ├── StringTests.h/.cpp
│   ├── UnicodeTests.h/.cpp
│   ├── LogTests.h/.cpp
│   └── BaseExceptionTests.h/.cpp
├── common/                        # Tests for src/lib/common
│   ├── CMakeLists.txt
│   ├── SettingsTests.h/.cpp
│   └── I18NTests.h/.cpp
├── deskflow/                      # Tests for src/lib/deskflow
│   ├── CMakeLists.txt
│   ├── ClipboardTests.h/.cpp
│   ├── ClipboardChunksTests.h/.cpp
│   ├── KeyMapTests.h/.cpp
│   ├── KeyStateTests.h/.cpp
│   ├── IKeyStateTests.h/.cpp
│   ├── KeyboardLayoutManagerTests.h/.cpp
│   ├── X11LayoutParserTests.h/.cpp
│   ├── MockEventQueue.h
│   ├── MockKeyMap.h
│   └── MockKeyState.h
├── gui/                           # Tests for src/lib/gui
│   ├── CMakeLists.txt
│   ├── KeySequenceTests.h/.cpp
│   ├── LoggerTests.h/.cpp
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   └── NetworkMonitorTests.h/.cpp
│   └── config/
│       ├── CMakeLists.txt
│       └── ScreenTests.h/.cpp
├── net/                           # Tests for src/lib/net
│   ├── CMakeLists.txt
│   ├── SecureUtilsTests.h/.cpp
│   ├── FingerprintTests.h/.cpp
│   └── FingerprintDatabaseTests.h/.cpp
├── platform/                      # Tests for src/lib/platform
│   ├── CMakeLists.txt
│   ├── MSWindowsClipboardTests.h/.cpp
│   ├── OSXClipboardTests.h/.cpp
│   ├── OSXKeyStateTests.h/.cpp
│   ├── WlClipboardTests.h/.cpp
│   └── XWindowsClipboardTests.h/.cpp
├── server/                        # Tests for src/lib/server
│   ├── CMakeLists.txt
│   ├── ServerTests.h/.cpp
│   └── ServerConfigTests.h/.cpp
└── legacytests/                   # Legacy Google Test suite
    ├── CMakeLists.txt
    ├── legacytests/
    ├── mock/
    └── shared/
```

## Test Structure (Modern Qt Test)

### Header Pattern

Every modern test class follows this structure:

```cpp
// StringTests.h
#include <QTest>

class StringTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  // Test are run in order top to bottom
  void initTestCase();    // Optional: runs once before all tests
  void testName1();       // Individual test methods
  void testName2();
private:
  // Test data members (constants, mock objects, log instances)
};
```

Key points:
- Test classes inherit `QObject` (not a custom test base)
- Use `Q_OBJECT` macro
- Test methods go in `private Q_SLOTS:` section
- `initTestCase()` is the only supported lifecycle hook (no `cleanupTestCase()` or per-test `init()`/`cleanup()` observed)

### Implementation Pattern

```cpp
// StringTests.cpp
#include "StringTests.h"
#include "base/String.h"

void StringTests::formatWithArgs()
{
  const char *format = "%%%{1}=%{2}";
  const char *arg1 = "answer";
  const char *arg2 = "42";

  std::string result = deskflow::string::format(format, arg1, arg2);
  QCOMPARE(result, "%answer=42");
}

QTEST_MAIN(StringTests)
```

Key points:
- Include own header first
- End file with `QTEST_MAIN(ClassName)` macro
- Tests run in declaration order (top to bottom)
- No explicit test registration beyond declaring in `private Q_SLOTS`

### CMake Registration

Use the `create_test()` function defined in `src/unittests/CMakeLists.txt`:

```cmake
create_test(
  NAME ClipboardTests           # Test executable name (PascalCase + "Tests")
  DEPENDS app                   # Library being tested
  LIBS arch base io ${extra_libs}  # Additional dependencies
  SOURCE ClipboardTests.cpp     # Single source file (includes .h)
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/deskflow"  # Run dir for file access
)
```

Parameters:
- `NAME`: Executable name and CTest test name
- `DEPENDS`: The library under test (linked first)
- `LIBS`: Additional libraries needed (always excludes `Qt::Test` which is added automatically)
- `SOURCE`: Single `.cpp` file (the `.h` is included via this)
- `WORKING_DIRECTORY`: Where the test runs (typically the binary output dir of the library under test)

Platform-specific library handling:
```cmake
if(WIN32)
  set(extra_libs version)
endif()
```

Conditional test registration:
```cmake
if(BUILD_X11_SUPPORT)
  create_test(
    NAME X11LayoutParserTests
    ...
  )
endif()
```

## Test Structure (Legacy Google Test)

### Test Pattern

```cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

TEST(KeyStateTests, sendKeyEvent_halfDuplexAndRepeat_addEventNotCalled)
{
  NiceMock<MockKeyMap> keyMap;
  NiceMock<MockEventQueue> eventQueue;
  KeyStateImpl keyState(eventQueue, keyMap);

  ON_CALL(keyMap, isHalfDuplex(_, _)).WillByDefault(Return(true));
  EXPECT_CALL(eventQueue, addEvent(_)).Times(0);

  keyState.sendKeyEvent(nullptr, false, true, kKeyCapsLock, 0, 0, 0);
}
```

### Main Entry Point

Legacy tests use a single `main.cpp` (`src/unittests/legacytests/legacytests/main.cpp`) that initializes Google Test and runs all tests:
```cpp
::testing::GTEST_FLAG(throw_on_failure) = true;
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();
```

## Assertions Reference

### QTest Assertions (Modern Tests)

| Macro | Purpose |
|-------|---------|
| `QCOMPARE(actual, expected)` | Compare two values for equality |
| `QVERIFY(condition)` | Assert condition is true |
| `QVERIFY2(condition, message)` | Assert with failure message |
| `qInfo() << value` | Log informational output during tests |

### QTest Signal Testing

```cpp
QSignalSpy spy(Logger::instance(), &Logger::newLine);
QVERIFY(spy.isValid());

// trigger signal
Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");

QCOMPARE(spy.count(), 1);
QVERIFY(qvariant_cast<QString>(spy.takeFirst().at(0)).contains("test"));
```

### Google Test Assertions (Legacy Tests)

| Macro | Purpose |
|-------|---------|
| `EXPECT_CALL(mock, method(args))` | Set expectation on mock method |
| `ON_CALL(mock, method(args)).WillByDefault(Return(value))` | Set default mock behavior |
| `EXPECT_EQ(a, b)` | Non-fatal equality check |
| `ASSERT_TRUE(condition)` | Fatal assertion |

## Mocking

### Modern Tests -- Hand-written Mocks

Modern tests do NOT use a mocking framework. Mocks are hand-written classes that implement abstract interfaces with stub/no-op behavior:

```cpp
// MockEventQueue.h -- implements IEventQueue with no-ops
class MockEventQueue : public IEventQueue
{
public:
  EventQueueTimer *newOneShotTimer(double duration, void *target) override
  {
    return nullptr;
  }
  bool getEvent(Event &, double) override
  {
    return true;
  }
  int loop() override
  {
    return s_exitSuccess;
  }
  // ... remaining pure virtual methods as no-ops
};
```

Key convention:
- Comment in mocks: `// NOTE: do not mock methods that are not pure virtual. this mock exists only to provide an implementation of the [ClassName] abstract class.`
- Mock files live alongside the test that uses them in the same directory
- Reused mocks are in `src/unittests/deskflow/` (e.g., `MockEventQueue.h`, `MockKeyState.h`, `MockKeyMap.h`)

### Legacy Tests -- Google Mock

Legacy tests use Google Mock with `NiceMock<>` wrappers:

```cpp
NiceMock<MockKeyMap> keyMap;
NiceMock<MockEventQueue> eventQueue;
```

Mock files for legacy tests are in `src/unittests/legacytests/mock/`.

**What to Mock:**
- Abstract interfaces (IEventQueue, IKeyState, KeyMap)
- External system dependencies

**What NOT to Mock:**
- Concrete classes being tested
- Qt framework classes (use real instances)

## Fixtures and Test Data

### Test Constants Pattern

Test constants are defined as private members of the test class:

```cpp
class ClipboardTests : public QObject
{
  // ...
private:
  const std::string kTestString1 = "deskflow rocks";
  const std::string kTestString2 = "String 020";
  Log m_log;
};
```

Or as inline static const for Qt types:
```cpp
private:
  inline static const QString m_settingsPath = QStringLiteral("tmp/test");
  inline static const QString m_settingsFile = QStringLiteral("%1/Deskflow.conf").arg(m_settingsPath);
```

### Test Setup (initTestCase)

Use `initTestCase()` for one-time setup:
```cpp
void ClipboardTests::initTestCase()
{
  m_log.setFilter(LogLevel::Debug2);
}

void LoggerTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));
  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}
```

### Test Data Construction

Build test data inline within test methods:
```cpp
void SecureUtilsTests::checkHex()
{
  const QByteArray fingerprint(
      "\x28\xFD\x0A\x98\x8A\x0E\xA1\x6C\xD7\xE8\x6C\xA7\xEE\x58\x41\x71\xCA\xB2\x8E\x49\x25\x94\x90\x25\x26\x05\x8D\xAF"
      "\x63\xED\x2E\x30",
      32
  );
  // ...
}
```

## Coverage

**Requirements:** Optional, off by default

**Enable coverage:**
```bash
cmake -DENABLE_COVERAGE=ON ..
```

**Coverage tool:** gcovr (XML output)
- Configured in `cmake/Libraries.cmake`
- Uses `cmake/CodeCoverage.cmake` module
- Per-test coverage targets: `coverage-{TestName}`
- Legacy test coverage: `coverage-legacytests`

**Exclusions from coverage:**
- `subprojects/*`
- `build/*`
- `src/unittests/*`
- `src/apps/deskflow-gui/**`
- `src/apps/res/**`
- `translations/**`

**SonarCloud integration:**
- Config: `sonar-project.properties`
- Coverage exclusions match gcovr exclusions
- Uses `compile_commands.json` from build directory
- Duplications excluded for test files: `sonar.cpd.exclusions=**/*Test*.cpp`

## Test Types

**Unit Tests:**
- Scope: Individual class/function testing
- Approach: Instantiate class directly, call methods, assert results
- Location: `src/unittests/{base,common,deskflow,gui,net,server}/`
- Pattern: One test class per source class, one test executable per test class

**Platform Tests:**
- Scope: Platform-specific clipboard, key state, screen tests
- Location: `src/unittests/platform/`
- Conditional compilation: Platform-specific tests only compile on their target platform
- Some tests interact with OS APIs (macOS pasteboard, Windows clipboard, X11)

**Legacy Integration Tests:**
- Scope: Cross-component testing with full library linking
- Location: `src/unittests/legacytests/`
- Approach: Google Test with Google Mock, monolithic executable
- Timeout: 1-minute exit timeout configured (`ExitTimeout` class)

**E2E Tests:**
- Not present in this codebase

## Common Patterns

### Testing Value Equality

```cpp
QCOMPARE(result, "%answer=42");
QCOMPARE(actual.substr(12), kTestString1);
QCOMPARE((int)actual[0], 0);
```

### Testing Boolean Conditions

```cpp
QVERIFY(clipboard.open(0));
QVERIFY(!clipboard.has(Text));
QVERIFY(keyState.getKeyState(1));
```

### Testing Return Values

```cpp
auto actual = new Server::SwitchToScreenInfo("test");
QCOMPARE(actual->m_screen, "test");
delete actual;
```

### Testing with Mocks

```cpp
MockEventQueue eventQueue;
MockKeyState keyState(eventQueue, m_keymap);
keyState.onKey(1, true, KeyModifierAlt);
QVERIFY(keyState.getKeyState(1));
```

### Testing Qt Signals

```cpp
QSignalSpy spy(Logger::instance(), &Logger::newLine);
QVERIFY(spy.isValid());

Logger::instance()->handleMessage(QtDebugMsg, "stub", "test");
QCOMPARE(spy.count(), 1);
```

### Error Testing

```cpp
// Testing that a method returns false for invalid input
QVERIFY(!keyState.fakeKeyRepeat(0, 0, 0, 0, "en"));
QVERIFY(!keyState.fakeKeyUp(0));
```

### Async Testing

No async test patterns observed. Tests are synchronous.

### Platform-Conditional Tests

```cmake
if(BUILD_X11_SUPPORT)
  create_test(
    NAME X11LayoutParserTests
    DEPENDS app
    LIBS arch base ${extra_libs}
    SOURCE X11LayoutParserTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/deskflow"
  )
endif()
```

## Adding New Tests

### For a new test (modern QTest pattern):

1. Create header `src/unittests/{module}/NewFeatureTests.h`:
```cpp
#include <QTest>

class NewFeatureTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void testBehavior_scenario_expectedResult();
};
```

2. Create implementation `src/unittests/{module}/NewFeatureTests.cpp`:
```cpp
#include "NewFeatureTests.h"
#include "module/NewFeature.h"

void NewFeatureTests::initTestCase() { /* setup */ }
void NewFeatureTests::testBehavior_scenario_expectedResult()
{
  // arrange, act, assert
  QCOMPARE(actual, expected);
}
QTEST_MAIN(NewFeatureTests)
```

3. Register in `src/unittests/{module}/CMakeLists.txt`:
```cmake
create_test(
  NAME NewFeatureTests
  DEPENDS {library_under_test}
  LIBS {additional_libs}
  SOURCE NewFeatureTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/{module}"
)
```

### Naming Convention for Test Methods

Use descriptive names with the pattern: `methodUnderTest_scenario_expectedResult`

Examples from the codebase:
- `findBestKey_requiredDown_matchExactFirstItem`
- `fakeKeyRepeat_invalidKey_returnsFalse`
- `isKeyDown_noKeysDown_returnsFalse`
- `updateKeyState_pollDoesNothing_keyNotSet`
- `parseModifiers_plusKey_keepsPlusAsKey`

---

*Testing analysis: 2026-05-12*
