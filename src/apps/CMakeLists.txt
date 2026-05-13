# SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

set(WIN32_PRE_EXCLUDE_REGEXES
  "api-ms-win-.*"
  "ext-ms-.*"
  "^hvsifiletrust\\.dll$"
  "^icu.*\\.dll$"
)

set(WIN32_POST_EXCLUDE_REGEXES ".*system32.*")

add_subdirectory(deskflow-core)
add_subdirectory(deskflow-daemon) #Only used on windows
add_subdirectory(deskflow-gui)
