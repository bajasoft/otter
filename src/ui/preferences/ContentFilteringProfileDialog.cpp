/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ContentFilteringProfileDialog.h"
#include "../../core/ContentFilteringManager.h"
#include "../../core/ContentFilteringProfile.h"
#include "../../core/SessionsManager.h"
#include "../../core/Utils.h"

#include "ui_ContentFilteringProfileDialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QMessageBox>

namespace Otter
{

ContentFilteringProfileDialog::ContentFilteringProfileDialog(QWidget *parent, ContentFilteringProfile *profile) : Dialog(parent),
	m_profile(profile),
	m_ui(new Ui::ContentFilteringProfileDialog)
{
	m_ui->setupUi(this);
	m_ui->categoryComboBox->addItem(tr("Advertisements"), ContentFilteringProfile::AdvertisementsCategory);
	m_ui->categoryComboBox->addItem(tr("Annoyance"), ContentFilteringProfile::AnnoyanceCategory);
	m_ui->categoryComboBox->addItem(tr("Privacy"), ContentFilteringProfile::PrivacyCategory);
	m_ui->categoryComboBox->addItem(tr("Social"), ContentFilteringProfile::SocialCategory);
	m_ui->categoryComboBox->addItem(tr("Regional"), ContentFilteringProfile::RegionalCategory);
	m_ui->categoryComboBox->addItem(tr("Other"), ContentFilteringProfile::OtherCategory);

	if (profile)
	{
		m_ui->titleEdit->setText(profile->getTitle());
		m_ui->updateUrlEdit->setText(profile->getUpdateUrl().url());
		m_ui->categoryComboBox->setCurrentIndex(m_ui->categoryComboBox->findData(profile->getCategory()));
		m_ui->lastUpdateTextLabel->setText(Utils::formatDateTime(profile->getLastUpdate()));
		m_ui->updateIntervalSpinBox->setValue(profile->getUpdateInterval());
	}

	connect(m_ui->confirmButtonBox, SIGNAL(accepted()), this, SLOT(save()));
	connect(m_ui->confirmButtonBox, SIGNAL(rejected()), this, SLOT(close()));
}

ContentFilteringProfileDialog::~ContentFilteringProfileDialog()
{
	delete m_ui;
}

ContentFilteringProfile* ContentFilteringProfileDialog::getProfile()
{
	return m_profile;
}

void ContentFilteringProfileDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void ContentFilteringProfileDialog::save()
{
	const ContentFilteringProfile::ProfileCategory category(static_cast<ContentFilteringProfile::ProfileCategory>(m_ui->categoryComboBox->itemData(m_ui->categoryComboBox->currentIndex()).toInt()));

	const QUrl url(m_ui->updateUrlEdit->text());

	if (!url.isValid())
	{
		QMessageBox::critical(this, tr("Error"), tr("Valid update URL is required."), QMessageBox::Close);

		return;
	}

	if (m_profile)
	{
		m_profile->setCategory(category);
		m_profile->setTitle(m_ui->titleEdit->text());
		m_profile->setUpdateUrl(url);
		m_profile->setUpdateInterval(m_ui->updateIntervalSpinBox->value());
	}
	else
	{
		QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering")));

		const QString fileName(QFileInfo(url.fileName()).completeBaseName());
		QFile file(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering/%1.txt")).arg(fileName));

		if (file.exists())
		{
			QMessageBox::critical(this, tr("Error"), tr("Profile with name %1.txt already exists.").arg(fileName), QMessageBox::Close);

			return;
		}

		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			QMessageBox::critical(this, tr("Error"), tr("Failed to create profile file: %1.").arg(file.errorString()), QMessageBox::Close);

			return;
		}

		file.write(QStringLiteral("[AdBlock Plus 2.0]\n").toUtf8());
		file.close();

		ContentFilteringProfile *profile(new ContentFilteringProfile(fileName, m_ui->titleEdit->text(), QLatin1String("adBlock"), url, QDateTime(), QList<QString>(), m_ui->updateIntervalSpinBox->value(), category, (ContentFilteringProfile::HasCustomTitleFlag | ContentFilteringProfile::HasCustomUpdateUrlFlag | ContentFilteringProfile::HasTypeFlag)));

		ContentFilteringManager::addProfile(profile);

		m_profile = profile;
	}

	accept();
}

}
