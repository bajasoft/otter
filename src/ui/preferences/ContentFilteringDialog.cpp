/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ContentFilteringDialog.h"
#include "ContentFilteringIntervalDelegate.h"
#include "ContentFilteringProfileDialog.h"
#include "../../core/Console.h"
#include "../../core/ContentFilteringManager.h"
#include "../../core/ContentFilteringProfile.h"
#include "../../core/SessionsManager.h"
#include "../../core/SettingsManager.h"
#include "../../core/Utils.h"
#include "../../modules/contentFiltering/AdblockContentFiltering.h"

#include "ui_ContentFilteringDialog.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>

namespace Otter
{

ContentFilteringDialog::ContentFilteringDialog(QWidget *parent) : Dialog(parent),
	m_ui(new Ui::ContentFilteringDialog)
{
	m_ui->setupUi(this);

	const QStringList globalProfiles(SettingsManager::getValue(SettingsManager::ContentFiltering_ProfilesOption).toStringList());

	m_ui->profilesViewWidget->setModel(ContentFilteringManager::createModel(this, globalProfiles));
	m_ui->profilesViewWidget->setItemDelegateForColumn(1, new ContentFilteringIntervalDelegate(this));
	m_ui->profilesViewWidget->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->profilesViewWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_ui->profilesViewWidget->expandAll();
	m_ui->cosmeticFiltersComboBox->addItem(tr("All"), QLatin1String("all"));
	m_ui->cosmeticFiltersComboBox->addItem(tr("Domain specific only"), QLatin1String("domainOnly"));
	m_ui->cosmeticFiltersComboBox->addItem(tr("None"), QLatin1String("none"));

	const int cosmeticFiltersIndex(m_ui->cosmeticFiltersComboBox->findData(SettingsManager::getValue(SettingsManager::ContentFiltering_CosmeticFiltersModeOption).toString()));

	m_ui->cosmeticFiltersComboBox->setCurrentIndex((cosmeticFiltersIndex < 0) ? 0 : cosmeticFiltersIndex);
	m_ui->enableCustomRulesCheckBox->setChecked(globalProfiles.contains(QLatin1String("custom")));

	QStandardItemModel *customRulesModel(new QStandardItemModel(this));
	QFile file(SessionsManager::getWritableDataPath("ContentFiltering/custom.txt"));

	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QTextStream stream(&file);

		if (stream.readLine().trimmed().startsWith(QLatin1String("[Adblock Plus"), Qt::CaseInsensitive))
		{
			while (!stream.atEnd())
			{
				customRulesModel->appendRow(new QStandardItem(stream.readLine().trimmed()));
			}
		}
		else
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to load custom rules: invalid adblock header"), Console::OtherCategory, Console::ErrorLevel, file.fileName());
		}

		file.close();
	}

	m_ui->customRulesViewWidget->setModel(customRulesModel);
	m_ui->enableWildcardsCheckBox->setChecked(SettingsManager::getValue(SettingsManager::ContentFiltering_EnableWildcardsOption).toBool());

	connect(ContentFilteringManager::getInstance(), SIGNAL(profileModified(QString)), this, SLOT(updateProfile(QString)));
	connect(m_ui->profilesViewWidget->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(updateProfilesActions()));
	connect(m_ui->updateProfileButton, SIGNAL(clicked(bool)), this, SLOT(updateProfile()));
	connect(m_ui->addProfileButton, SIGNAL(clicked(bool)), this, SLOT(addProfile()));
	connect(m_ui->editProfileButton, SIGNAL(clicked(bool)), this, SLOT(editProfile()));
	connect(m_ui->confirmButtonBox, SIGNAL(accepted()), this, SLOT(save()));
	connect(m_ui->confirmButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui->enableCustomRulesCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateRulesActions()));
	connect(m_ui->customRulesViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateRulesActions()));
	connect(m_ui->addRuleButton, SIGNAL(clicked(bool)), this, SLOT(addRule()));
	connect(m_ui->editRuleButton, SIGNAL(clicked(bool)), this, SLOT(editRule()));
	connect(m_ui->removeRuleButton, SIGNAL(clicked(bool)), this, SLOT(removeRule()));

	updateRulesActions();
}

ContentFilteringDialog::~ContentFilteringDialog()
{
	delete m_ui;
}

void ContentFilteringDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void ContentFilteringDialog::addProfile()
{
	ContentFilteringProfileDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted)
	{
		updateModel(dialog.getProfile(), true);
	}
}

void ContentFilteringDialog::editProfile()
{
	const QModelIndex index(m_ui->profilesViewWidget->currentIndex().sibling(m_ui->profilesViewWidget->currentIndex().row(), 0));
	ContentFilteringProfile *profile(ContentFilteringManager::getProfile(index.data(Qt::UserRole).toString()));

	if (profile)
	{
		const ContentFilteringProfile::ProfileCategory category(profile->getCategory());
		ContentFilteringProfileDialog dialog(this, profile);

		if (dialog.exec() == QDialog::Accepted)
		{
			updateModel(profile, (category != profile->getCategory()));
		}
	}
}

void ContentFilteringDialog::updateProfile()
{
	const QModelIndex index(m_ui->profilesViewWidget->currentIndex().sibling(m_ui->profilesViewWidget->currentIndex().row(), 0));

	if (index.isValid())
	{
		ContentFilteringManager::updateProfile(index.data(Qt::UserRole).toString());
	}
}

void ContentFilteringDialog::updateProfilesActions()
{
	const QModelIndex index(m_ui->profilesViewWidget->currentIndex().sibling(m_ui->profilesViewWidget->currentIndex().row(), 0));

	m_ui->editProfileButton->setEnabled(index.isValid() && index.flags().testFlag(Qt::ItemNeverHasChildren));
	m_ui->updateProfileButton->setEnabled(index.isValid() && index.data(Qt::UserRole + 1).toUrl().isValid());
}

void ContentFilteringDialog::addRule()
{
	m_ui->customRulesViewWidget->insertRow();

	editRule();
}

void ContentFilteringDialog::editRule()
{
	m_ui->customRulesViewWidget->edit(m_ui->customRulesViewWidget->getIndex(m_ui->customRulesViewWidget->getCurrentRow()));
}

void ContentFilteringDialog::removeRule()
{
	m_ui->customRulesViewWidget->removeRow();
	m_ui->customRulesViewWidget->setFocus();

	updateRulesActions();
}

void ContentFilteringDialog::updateRulesActions()
{
	m_ui->tabWidget->setTabEnabled(1, m_ui->enableCustomRulesCheckBox->isChecked());

	const bool isEditable(m_ui->customRulesViewWidget->getCurrentRow() >= 0);

	m_ui->editRuleButton->setEnabled(isEditable);
	m_ui->removeRuleButton->setEnabled(isEditable);
}

void ContentFilteringDialog::updateModel(ContentFilteringProfile *profile, bool isNewOrMoved)
{
	if (isNewOrMoved)
	{
		const QModelIndexList removeList(m_ui->profilesViewWidget->model()->match(m_ui->profilesViewWidget->model()->index(0, 0), Qt::UserRole, QVariant::fromValue(profile->getName()), 2, Qt::MatchRecursive));

		if (removeList.count() > 0)
		{
			m_ui->profilesViewWidget->model()->removeRow(removeList[0].row(), removeList[0].parent());
		}

		QList<QStandardItem*> profileItems({new QStandardItem(profile->getTitle()), new QStandardItem(QString::number(profile->getUpdateInterval())), new QStandardItem(Utils::formatDateTime(profile->getLastUpdate()))});
		profileItems[0]->setData(profile->getName(), Qt::UserRole);
		profileItems[0]->setData(profile->getUpdateUrl(), (Qt::UserRole + 1));
		profileItems[0]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		profileItems[0]->setCheckable(true);
		profileItems[0]->setCheckState(Qt::Unchecked);
		profileItems[1]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		profileItems[2]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		const QModelIndexList indexList(m_ui->profilesViewWidget->model()->match(m_ui->profilesViewWidget->model()->index(0, 0), Qt::UserRole, QVariant::fromValue((int)profile->getCategory())));

		if (indexList.count() > 0)
		{
			QStandardItem *categoryItem(qobject_cast<QStandardItemModel* >(m_ui->profilesViewWidget->model())->itemFromIndex(indexList[0]));
			categoryItem->appendRow(profileItems);
			categoryItem->sortChildren(m_ui->profilesViewWidget->getSortColumn(), m_ui->profilesViewWidget->getSortOrder());
		}

		return;
	}

	const QModelIndex currentIndex(m_ui->profilesViewWidget->currentIndex());

	m_ui->profilesViewWidget->setData(QModelIndex(currentIndex.sibling(currentIndex.row(), 0)), profile->getTitle(), Qt::DisplayRole);
	m_ui->profilesViewWidget->setData(QModelIndex(currentIndex.sibling(currentIndex.row(), 0)), profile->getUpdateUrl(), (Qt::UserRole + 1));
	m_ui->profilesViewWidget->setData(QModelIndex(currentIndex.sibling(currentIndex.row(), 1)), profile->getUpdateInterval(), Qt::DisplayRole);
	m_ui->profilesViewWidget->setData(QModelIndex(currentIndex.sibling(currentIndex.row(), 2)), profile->getLastUpdate(), Qt::DisplayRole);
}

void ContentFilteringDialog::updateProfile(const QString &name)
{
	ContentFilteringProfile *profile(ContentFilteringManager::getProfile(name));

	if (!profile)
	{
		return;
	}

	for (int i = 0; i < m_ui->profilesViewWidget->getRowCount(); ++i)
	{
		const QModelIndex categoryIndex(m_ui->profilesViewWidget->getIndex(i));

		for (int j = 0; j < m_ui->profilesViewWidget->getRowCount(categoryIndex); ++j)
		{
			const QModelIndex entryIndex(m_ui->profilesViewWidget->getIndex(j, 0, categoryIndex));

			if (entryIndex.data(Qt::UserRole).toString() == name)
			{
				ContentFilteringProfile *profile(ContentFilteringManager::getProfile(name));

				ContentFilteringProfile::ProfileCategory category(profile->getCategory());
				QString title(profile->getTitle());

				if (category == ContentFilteringProfile::RegionalCategory)
				{
					const QList<QLocale::Language> languages(profile->getLanguages());
					QStringList languageNames;

					for (int k = 0; k < languages.count(); ++k)
					{
						languageNames.append(QLocale::languageToString(languages.at(k)));
					}

					title = QStringLiteral("%1 [%2]").arg(title).arg(languageNames.join(QLatin1String(", ")));
				}

				m_ui->profilesViewWidget->setData(entryIndex, title, Qt::DisplayRole);
				m_ui->profilesViewWidget->setData(entryIndex.sibling(j, 2), Utils::formatDateTime(profile->getLastUpdate()), Qt::DisplayRole);

				return;
			}
		}
	}
}

void ContentFilteringDialog::save()
{
	QStringList profiles;

	for (int i = 0; i < m_ui->profilesViewWidget->getRowCount(); ++i)
	{
		const QModelIndex categoryIndex(m_ui->profilesViewWidget->getIndex(i));

		for (int j = 0; j < m_ui->profilesViewWidget->getRowCount(categoryIndex); ++j)
		{
			const QModelIndex entryIndex(m_ui->profilesViewWidget->getIndex(j, 0, categoryIndex));
			const QModelIndex intervalIndex(m_ui->profilesViewWidget->getIndex(j, 1, categoryIndex));
			const QString name(entryIndex.data(Qt::UserRole).toString());
			ContentFilteringProfile *profile(ContentFilteringManager::getProfile(name));

			if (intervalIndex.data(Qt::EditRole).toInt() != profile->getUpdateInterval())
			{
				profile->setUpdateInterval(intervalIndex.data(Qt::EditRole).toInt());
			}

			if (entryIndex.data(Qt::CheckStateRole).toBool())
			{
				profiles.append(name);
			}
		}
	}

	if (m_ui->enableCustomRulesCheckBox->isChecked())
	{
		QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering")));

		QFile file(SessionsManager::getWritableDataPath("ContentFiltering/custom.txt"));

		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to create a file with custom rules: %1").arg(file.errorString()), Console::OtherCategory, Console::ErrorLevel, file.fileName());
		}
		else
		{
			file.write(QStringLiteral("[AdBlock Plus 2.0]\n").toUtf8());

			for (int i = 0; i < m_ui->customRulesViewWidget->getRowCount(); ++i)
			{
				file.write(m_ui->customRulesViewWidget->getIndex(i, 0).data().toString().toLatin1() + QStringLiteral("\n").toUtf8());
			}

			file.close();

			ContentFilteringProfile *profile(ContentFilteringManager::getProfile(QLatin1String("custom")));

			if (profile)
			{
				profile->clear();
			}
			else
			{
				profile = new AdblockContentFiltering(QLatin1String("custom"), tr("Custom Rules"), QLatin1String("adBlock"), QUrl(), QDateTime(), QList<QString>(), 0, ContentFilteringProfile::OtherCategory, ContentFilteringProfile::NoFlags);

				ContentFilteringManager::addProfile(profile);
			}

			profiles.append(QLatin1String("custom"));
		}
	}

	SettingsManager::setValue(SettingsManager::ContentFiltering_ProfilesOption, profiles);
	SettingsManager::setValue(SettingsManager::ContentFiltering_EnableWildcardsOption, m_ui->enableWildcardsCheckBox->isChecked());
	SettingsManager::setValue(SettingsManager::ContentFiltering_CosmeticFiltersModeOption, m_ui->cosmeticFiltersComboBox->currentData().toString());

	close();
}

}
