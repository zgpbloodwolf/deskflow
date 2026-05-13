# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

if(WIN32)
  set(PLATFORM_SOURCES
    IMSWindowsClipboardFacade.h
    MSWindowsClipboard.cpp
    MSWindowsClipboard.h
    MSWindowsClipboardAnyTextConverter.cpp
    MSWindowsClipboardAnyTextConverter.h
    MSWindowsClipboardBitmapConverter.cpp
    MSWindowsClipboardBitmapConverter.h
    MSWindowsClipboardFacade.cpp
    MSWindowsClipboardFacade.h
    MSWindowsClipboardHTMLConverter.cpp
    MSWindowsClipboardHTMLConverter.h
    MSWindowsClipboardUTF16Converter.cpp
    MSWindowsClipboardUTF16Converter.h
    MSWindowsDebugOutputter.cpp
    MSWindowsDebugOutputter.h
    MSWindowsDesks.cpp
    MSWindowsDesks.h
    MSWindowsEventQueueBuffer.cpp
    MSWindowsEventQueueBuffer.h
    MSWindowsHook.cpp
    MSWindowsHook.h
    MSWindowsKeyState.cpp
    MSWindowsKeyState.h
    MSWindowsPowerManager.cpp
    MSWindowsPowerManager.h
    MSWindowsProcess.cpp
    MSWindowsProcess.h
    MSWindowsScreen.cpp
    MSWindowsScreen.h
    MSWindowsScreenSaver.cpp
    MSWindowsScreenSaver.h
    MSWindowsSession.cpp
    MSWindowsSession.h
    MSWindowsWatchdog.cpp
    MSWindowsWatchdog.h
  )
elseif(APPLE)
  set(PLATFORM_SOURCES
    IOSXKeyResource.cpp
    IOSXKeyResource.h
    OSXAutoTypes.h
    OSXClipboard.cpp
    OSXClipboard.h
    OSXClipboardAnyBitmapConverter.cpp
    OSXClipboardAnyBitmapConverter.h
    OSXClipboardAnyTextConverter.cpp
    OSXClipboardAnyTextConverter.h
    OSXClipboardBMPConverter.cpp
    OSXClipboardBMPConverter.h
    OSXClipboardHTMLConverter.cpp
    OSXClipboardHTMLConverter.h
    OSXClipboardTextConverter.cpp
    OSXClipboardTextConverter.h
    OSXClipboardUTF8Converter.cpp
    OSXClipboardUTF8Converter.h
    OSXClipboardUTF16Converter.cpp
    OSXClipboardUTF16Converter.h
    OSXCocoaApp.h
    OSXCocoaApp.m
    OSXEventQueueBuffer.cpp
    OSXEventQueueBuffer.h
    OSXKeyState.cpp
    OSXKeyState.h
    OSXMediaKeySupport.h
    OSXMediaKeySupport.m
    OSXPasteboardPeeker.h
    OSXPasteboardPeeker.m
    OSXPowerManager.cpp
    OSXPowerManager.h
    OSXScreen.h
    OSXScreen.mm
    OSXScreenSaver.cpp
    OSXScreenSaver.h
    OSXScreenSaverControl.h
    OSXScreenSaverUtil.h
    OSXScreenSaverUtil.m
    OSXUchrKeyResource.cpp
    OSXUchrKeyResource.h
  )
endif()

# wayland.h is included to check for wayland support
add_library(platform STATIC ${PLATFORM_SOURCES})

target_link_libraries(platform client ${libs})

if(APPLE)
  target_link_libraries(
    platform
    io
    net
    app
    client
    ${libs})

  find_library(COCOA_LIBRARY Cocoa)
  target_link_libraries(platform ${COCOA_LIBRARY})
endif()
