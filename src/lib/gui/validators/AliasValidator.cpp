/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2021 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AliasValidator.h"

#include "ComputerNameValidator.h"
#include "IpAddressValidator.h"
#include "SpacesValidator.h"
#include "ValidationError.h"

#include <QLineEdit>
#include <QRegularExpression>

namespace validators {

AliasValidator::AliasValidator(QLineEdit *parent, ValidationError *error) : LineEditValidator(parent, error)
{
  addValidator(std::make_unique<SpacesValidator>(tr("电脑名称不能包含空格")));
  addValidator(std::make_unique<IpAddressValidator>(tr("别名不能是 IP 地址")));
  addValidator(std::make_unique<ComputerNameValidator>(tr("包含无效字符或长度过长")));
}

} // namespace validators
