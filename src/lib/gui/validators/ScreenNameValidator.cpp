/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2021 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenNameValidator.h"

#include "ComputerNameValidator.h"
#include "EmptyStringValidator.h"
#include "ScreenDuplicationsValidator.h"
#include "SpacesValidator.h"
#include "ValidationError.h"

#include "gui/config/ScreenList.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <memory>

namespace validators {

ScreenNameValidator::ScreenNameValidator(QLineEdit *lineEdit, ValidationError *error, const ScreenList *pScreens)
    : LineEditValidator(lineEdit, error)
{
  addValidator(std::make_unique<EmptyStringValidator>(tr("电脑名称不能为空")));
  addValidator(std::make_unique<SpacesValidator>(tr("电脑名称不能包含空格")));
  addValidator(std::make_unique<ComputerNameValidator>(tr("包含无效字符或长度过长")));
  addValidator(
      std::make_unique<ScreenDuplicationsValidator>(
          tr("已存在同名电脑"), lineEdit ? lineEdit->text() : "", pScreens
      )
  );
}

} // namespace validators
