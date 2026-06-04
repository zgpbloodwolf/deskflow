/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSettingsDialog.h"
#include "ui_ScreenSettingsDialog.h"

#include "gui/config/Screen.h"
#include "validators/AliasValidator.h"
#include "validators/ScreenNameValidator.h"
#include "validators/ValidationError.h"

#include <QMessageBox>

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

ScreenSettingsDialog::~ScreenSettingsDialog() = default;

ScreenSettingsDialog::ScreenSettingsDialog(QWidget *parent, Screen *screen, const ScreenList *screens)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ScreenSettingsDialog>()},
      m_screen(screen)
{

  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  ui->lineNameEdit->setText(m_screen->name());

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ScreenNameValidator(ui->lineNameEdit, valNameError, screens);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  for (int i = 0; i < m_screen->aliases().count(); i++)
    new QListWidgetItem(m_screen->aliases()[i], ui->listAliases);

  ui->comboShift->setCurrentIndex(m_screen->modifier(static_cast<int>(Shift)));
  ui->comboCtrl->setCurrentIndex(m_screen->modifier(static_cast<int>(Ctrl)));
  ui->comboAlt->setCurrentIndex(m_screen->modifier(static_cast<int>(Alt)));
  ui->comboMeta->setCurrentIndex(m_screen->modifier(static_cast<int>(Meta)));
  ui->comboSuper->setCurrentIndex(m_screen->modifier(static_cast<int>(Super)));

  ui->chkDeadTopLeft->setChecked(m_screen->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_screen->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_screen->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_screen->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_screen->switchCornerSize());

  ui->chkFixCapsLock->setChecked(m_screen->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_screen->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_screen->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_screen->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ScreenSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ScreenSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ScreenSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ScreenSettingsDialog::aliasSelected);

  // 预设下拉框：选择预设时自动设置所有修饰键映射
  connect(ui->comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onPresetSelected);

  // 修饰键下拉框：手动修改时将预设切换为"自定义"
  connect(ui->comboShift, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onModifierChanged);
  connect(ui->comboCtrl, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onModifierChanged);
  connect(ui->comboAlt, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onModifierChanged);
  connect(ui->comboMeta, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onModifierChanged);
  connect(ui->comboSuper, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ScreenSettingsDialog::onModifierChanged);

  // 信号连接完成后再判断预设，避免初始化时被 onModifierChanged 覆盖
  updatePresetLabel();
}

void ScreenSettingsDialog::accept()
{
  if (ui->lineNameEdit->text().isEmpty()) {
    QMessageBox::warning(
        this, tr("屏幕名称为空"),
        tr("屏幕名称不能为空。"
           "请填写名称或取消对话框。")
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  m_screen->setName(ui->lineNameEdit->text());

  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      QMessageBox::warning(
          this, tr("屏幕名称与别名相同"),
          tr("屏幕名称不能与别名相同。"
             "请移除该别名或更改屏幕名称。")
      );
      return;
    }
    m_screen->addAlias(alias);
  }

  m_screen->setModifier(Shift, ui->comboShift->currentIndex());
  m_screen->setModifier(Ctrl, ui->comboCtrl->currentIndex());
  m_screen->setModifier(Alt, ui->comboAlt->currentIndex());
  m_screen->setModifier(Meta, ui->comboMeta->currentIndex());
  m_screen->setModifier(Super, ui->comboSuper->currentIndex());

  m_screen->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_screen->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_screen->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_screen->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_screen->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_screen->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_screen->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_screen->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_screen->setFix(XTest, ui->chkFixXTest->isChecked());

  QDialog::accept();
}

void ScreenSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(ui->lineAddAlias->text(), ui->listAliases);
    ui->lineAddAlias->clear();
  }
}

void ScreenSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ScreenSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ScreenSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}

void ScreenSettingsDialog::onPresetSelected(int index)
{
  // index 0 = "自定义"，显示所有修饰键
  if (index == 0) {
    setModifierCombosVisible(true, true);
    ui->labelPresetDesc->hide();
    return;
  }

  // 预设定义：{shift, ctrl, alt, meta, super} 的 combo index 值
  // combo index: 0=Shift, 1=Ctrl, 2=Alt, 3=⌘ Command, 4=Win 键, 5=无
  struct Preset
  {
    int shift;
    int ctrl;
    int alt;
    int meta;
    int super;
    bool showMeta;  // 显示 ⌘ Command 行
    bool showSuper; // 显示 Win 键 行
  };

  // index 1: 不映射（全部显示）
  // index 2: Mac → Windows（隐藏 Win 键，Mac 上没有）
  // index 3: Windows → Mac（隐藏 ⌘ Command，Windows 上没有）
  static const Preset presets[] = {
      {0, 1, 2, 3, 4, true, true},  // 1: 不映射
      {0, 4, 2, 1, 3, true, false}, // 2: Mac → Windows
      {0, 3, 2, 4, 1, false, true}, // 3: Windows → Mac
  };

  const int presetIndex = index - 1;
  if (presetIndex < 0 || presetIndex >= static_cast<int>(sizeof(presets) / sizeof(presets[0])))
    return;

  const auto &p = presets[presetIndex];
  m_updatingFromPreset = true;
  ui->comboShift->setCurrentIndex(p.shift);
  ui->comboCtrl->setCurrentIndex(p.ctrl);
  ui->comboAlt->setCurrentIndex(p.alt);
  ui->comboMeta->setCurrentIndex(p.meta);
  ui->comboSuper->setCurrentIndex(p.super);
  m_updatingFromPreset = false;

  // 根据预设控制哪些修饰键行可见
  setModifierCombosVisible(p.showMeta, p.showSuper);
  ui->labelPresetDesc->hide();
}

void ScreenSettingsDialog::onModifierChanged()
{
  // 由预设设置触发时，不回切到"自定义"
  if (m_updatingFromPreset)
    return;

  ui->comboPreset->setCurrentIndex(0);
}

void ScreenSettingsDialog::updatePresetLabel()
{
  // 读取当前各 combo 的值，判断是否匹配某个预设
  const int shift = ui->comboShift->currentIndex();
  const int ctrl = ui->comboCtrl->currentIndex();
  const int alt = ui->comboAlt->currentIndex();
  const int meta = ui->comboMeta->currentIndex();
  const int superKey = ui->comboSuper->currentIndex();

  // 不映射: {0, 1, 2, 3, 4}
  if (shift == 0 && ctrl == 1 && alt == 2 && meta == 3 && superKey == 4) {
    ui->comboPreset->setCurrentIndex(1);
    return;
  }

  // Mac → Windows: {0, 4, 2, 1, 3}
  if (shift == 0 && ctrl == 4 && alt == 2 && meta == 1 && superKey == 3) {
    ui->comboPreset->setCurrentIndex(2);
    return;
  }

  // Windows → Mac: {0, 3, 2, 4, 1}
  if (shift == 0 && ctrl == 3 && alt == 2 && meta == 4 && superKey == 1) {
    ui->comboPreset->setCurrentIndex(3);
    return;
  }

  // 不匹配任何预设，设为"自定义"
  ui->comboPreset->setCurrentIndex(0);
}

void ScreenSettingsDialog::setModifierCombosVisible(bool showMeta, bool showSuper)
{
  // Shift、Ctrl、Alt 始终显示
  ui->label_2->setVisible(true);
  ui->comboShift->setVisible(true);
  ui->label_3->setVisible(true);
  ui->comboCtrl->setVisible(true);
  ui->label_4->setVisible(true);
  ui->comboAlt->setVisible(true);

  // ⌘ Command 行：Mac 特有
  ui->label_5->setVisible(showMeta);
  ui->comboMeta->setVisible(showMeta);

  // Win 键 行：Windows 特有
  ui->label_6->setVisible(showSuper);
  ui->comboSuper->setVisible(showSuper);
}
