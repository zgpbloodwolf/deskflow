/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QSysInfo>

namespace deskflow::platform {

/**
 * @brief isWindows
 * @return Returns true if we are running on windows
 */
inline bool isWindows()
{
  return QSysInfo::productType() == QStringLiteral("windows");
}

/**
 * @brief isMac
 * @return Returns true if we are running on mac os
 */
inline bool isMac()
{
  return QSysInfo::productType() == QStringLiteral("macos");
}

} // namespace deskflow::platform
