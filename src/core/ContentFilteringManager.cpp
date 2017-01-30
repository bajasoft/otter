/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
* Copyright (C) 2015 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ContentFilteringManager.h"
#include "Console.h"
#include "ContentFilteringProfile.h"
#include "../modules/contentFiltering/AdblockContentFiltering.h"
#include "SettingsManager.h"
#include "SessionsManager.h"
#include "Utils.h"

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QStandardItemModel>

namespace Otter
{

ContentFilteringManager* ContentFilteringManager::m_instance(nullptr);
QVector<ContentFilteringProfile*> ContentFilteringManager::m_profiles;
ContentFilteringManager::CosmeticFiltersMode ContentFilteringManager::m_cosmeticFiltersMode(AllFiltersMode);
bool ContentFilteringManager::m_areWildcardsEnabled(true);

ContentFilteringManager::ContentFilteringManager(QObject *parent) : QObject(parent),
	m_saveTimer(0)
{
	m_areWildcardsEnabled = SettingsManager::getValue(SettingsManager::ContentFiltering_EnableWildcardsOption).toBool();

	optionChanged(SettingsManager::ContentFiltering_CosmeticFiltersModeOption, SettingsManager::getValue(SettingsManager::ContentFiltering_CosmeticFiltersModeOption).toString());

	connect(SettingsManager::getInstance(), SIGNAL(valueChanged(int,QVariant)), this, SLOT(optionChanged(int,QVariant)));
}

void ContentFilteringManager::createInstance(QObject *parent)
{
	if (!m_instance)
	{
		m_instance = new ContentFilteringManager(parent);
	}
}

void ContentFilteringManager::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_saveTimer)
	{
		killTimer(m_saveTimer);

		m_saveTimer = 0;

		QJsonObject settings;
		const QHash<ContentFilteringProfile::ProfileCategory, QString> categoryTitles({{ContentFilteringProfile::AdvertisementsCategory, QLatin1String("advertisements")}, {ContentFilteringProfile::AnnoyanceCategory, QLatin1String("annoyance")}, {ContentFilteringProfile::PrivacyCategory, QLatin1String("privacy")}, {ContentFilteringProfile::SocialCategory, QLatin1String("social")}, {ContentFilteringProfile::RegionalCategory, QLatin1String("regional")}, {ContentFilteringProfile::OtherCategory, QLatin1String("other")}});

		for (int i = 0; i < m_profiles.count(); ++i)
		{
			ContentFilteringProfile *profile(m_profiles.at(i));

			if (profile == nullptr || profile->getName() == QLatin1String("custom"))
			{
				continue;
			}

			QJsonObject profileSettings;
			const int updateInterval(profile->getUpdateInterval());

			if (updateInterval > 0)
			{
				profileSettings[QLatin1String("updateInterval")] = updateInterval;
			}

			const QDateTime lastUpdate(profile->getLastUpdate());

			if (lastUpdate.isValid())
			{
				profileSettings[QLatin1String("lastUpdate")] = lastUpdate.toString(Qt::ISODate);
			}

			if (profile->getFlags().testFlag(ContentFilteringProfile::HasCustomTitleFlag))
			{
				profileSettings[QLatin1String("title")] = profile->getTitle();
			}

			if (profile->getFlags().testFlag(ContentFilteringProfile::HasCustomUpdateUrlFlag))
			{
				profileSettings[QLatin1String("updateUrl")] = profile->getUpdateUrl().url();
			}

			if (profile->getFlags().testFlag(ContentFilteringProfile::HasTypeFlag))
			{
				profileSettings[QLatin1String("type")] = QLatin1String("adBlock");
			}

			profileSettings[QLatin1String("category")] = categoryTitles.value(profile->getCategory());

			const QList<QLocale::Language> languages(m_profiles.at(i)->getLanguages());

			if (!languages.contains(QLocale::AnyLanguage))
			{
				QJsonArray languageNames;

				for (int j = 0; j < languages.count(); ++j)
				{
					languageNames.append(QLocale(languages.at(j)).name());
				}

				profileSettings[QLatin1String("languages")] = languageNames;
			}

			settings[profile->getName()] = profileSettings;
		}

		QFile file(SessionsManager::getWritableDataPath(QLatin1String("contentFiltering.json")));

		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			return;
		}

		file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented));
		file.close();
	}
}

void ContentFilteringManager::optionChanged(int identifier, const QVariant &value)
{
	switch (identifier)
	{
		case SettingsManager::ContentFiltering_EnableWildcardsOption:
			m_areWildcardsEnabled = value.toBool();

			break;
		case SettingsManager::ContentFiltering_CosmeticFiltersModeOption:
			{
				const QString cosmeticFiltersMode(value.toString());

				if (cosmeticFiltersMode == QLatin1String("none"))
				{
					m_cosmeticFiltersMode = NoFiltersMode;
				}
				else if (cosmeticFiltersMode == QLatin1String("domainOnly"))
				{
					m_cosmeticFiltersMode = DomainOnlyFiltersMode;
				}
				else
				{
					m_cosmeticFiltersMode = AllFiltersMode;
				}
			}

			break;
		default:
			return;
	}

	for (int i = 0; i < m_profiles.count(); ++i)
	{
		m_profiles[i]->clear();
	}
}

void ContentFilteringManager::scheduleSave()
{
	if (m_saveTimer == 0)
	{
		m_saveTimer = startTimer(1000);
	}
}

void ContentFilteringManager::addProfile(ContentFilteringProfile *profile)
{
	if (profile)
	{
		m_profiles.append(profile);

		getInstance()->scheduleSave();

		connect(profile, SIGNAL(profileModified(QString)), m_instance, SLOT(scheduleSave()));
	}
}

QStandardItemModel* ContentFilteringManager::createModel(QObject *parent, const QStringList &profiles)
{
	QHash<ContentFilteringProfile::ProfileCategory, QMultiMap<QString, QList<QStandardItem*> > > categoryEntries;
	QStandardItemModel *model(new QStandardItemModel(parent));
	model->setHorizontalHeaderLabels(QStringList({tr("Title"), tr("Update Interval"), tr("Last Update")}));

	if (m_profiles.isEmpty())
	{
		getProfiles();
	}

	for (int i = 0; i < m_profiles.count(); ++i)
	{
		const QString name(m_profiles.at(i)->getName());

		if (name == QLatin1String("custom"))
		{
			continue;
		}

		ContentFilteringProfile::ProfileCategory category(m_profiles.at(i)->getCategory());
		QString title(m_profiles.at(i)->getTitle());

		if (category == ContentFilteringProfile::RegionalCategory)
		{
			const QList<QLocale::Language> languages(m_profiles.at(i)->getLanguages());
			QStringList languageNames;

			for (int j = 0; j < languages.count(); ++j)
			{
				languageNames.append(QLocale::languageToString(languages.at(j)));
			}

			title = QStringLiteral("%1 [%2]").arg(title).arg(languageNames.join(QLatin1String(", ")));
		}

		QList<QStandardItem*> profileItems({new QStandardItem(title), new QStandardItem(QString::number(m_profiles.at(i)->getUpdateInterval())), new QStandardItem(Utils::formatDateTime(m_profiles.at(i)->getLastUpdate()))});
		profileItems[0]->setData(name, Qt::UserRole);
		profileItems[0]->setData(m_profiles.at(i)->getUpdateUrl(), (Qt::UserRole + 1));
		profileItems[0]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		profileItems[0]->setCheckable(true);
		profileItems[0]->setCheckState(profiles.contains(name) ? Qt::Checked : Qt::Unchecked);
		profileItems[1]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
		profileItems[2]->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		if (!categoryEntries.contains(category))
		{
			categoryEntries[category] = QMultiMap<QString, QList<QStandardItem*> >();
		}

		categoryEntries[category].insert(title, profileItems);
	}

	QList<QPair<ContentFilteringProfile::ProfileCategory, QString> > categoryTitles({qMakePair(ContentFilteringProfile::AdvertisementsCategory, tr("Advertisements")), qMakePair(ContentFilteringProfile::AnnoyanceCategory, tr("Annoyance")), qMakePair(ContentFilteringProfile::PrivacyCategory, tr("Privacy")), qMakePair(ContentFilteringProfile::SocialCategory, tr("Social")), qMakePair(ContentFilteringProfile::RegionalCategory, tr("Regional")), qMakePair(ContentFilteringProfile::OtherCategory, tr("Other"))});

	for (int i = 0; i < categoryTitles.count(); ++i)
	{
		if (!categoryEntries.contains(categoryTitles.at(i).first))
		{
			continue;
		}

		const QList<QList<QStandardItem*> > profileItems(categoryEntries[categoryTitles.at(i).first].values());
		QList<QStandardItem*> categoryItems({new QStandardItem(categoryTitles.at(i).second), new QStandardItem(), new QStandardItem()});
		categoryItems[0]->setData(categoryTitles.at(i).first, Qt::UserRole);
		categoryItems[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		categoryItems[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		categoryItems[2]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		for (int j = 0; j < profileItems.count(); ++j)
		{
			categoryItems[0]->appendRow(profileItems.at(j));
		}

		model->appendRow(categoryItems);
	}

	return model;
}

ContentFilteringManager* ContentFilteringManager::getInstance()
{
	return m_instance;
}

ContentFilteringProfile* ContentFilteringManager::getProfile(const QString &profile)
{
	for (int i = 0; i < m_profiles.count(); ++i)
	{
		if (m_profiles[i]->getName() == profile)
		{
			return m_profiles[i];
		}
	}

	return nullptr;
}

ContentFilteringManager::CheckResult ContentFilteringManager::checkUrl(const QVector<int> &profiles, const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	if (profiles.isEmpty())
	{
		return CheckResult();
	}

	const QString scheme(requestUrl.scheme());

	if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
	{
		return CheckResult();
	}

	CheckResult result;

	for (int i = 0; i < profiles.count(); ++i)
	{
		if (profiles[i] >= 0 && profiles[i] < m_profiles.count())
		{
			const CheckResult currentResult(m_profiles.at(profiles[i])->checkUrl(baseUrl, requestUrl, resourceType));

			if (currentResult.isBlocked)
			{
				result = currentResult;
			}
			else if (currentResult.isException)
			{
				return currentResult;
			}
		}
	}

	return result;
}

QStringList ContentFilteringManager::createSubdomainList(const QString &domain)
{
	QStringList subdomainList;
	int dotPosition(domain.lastIndexOf(QLatin1Char('.')));
	dotPosition = domain.lastIndexOf(QLatin1Char('.'), dotPosition - 1);

	while (dotPosition != -1)
	{
		subdomainList.append(domain.mid(dotPosition + 1));

		dotPosition = domain.lastIndexOf(QLatin1Char('.'), dotPosition - 1);
	}

	subdomainList.append(domain);

	return subdomainList;
}

QStringList ContentFilteringManager::getStyleSheet(const QVector<int> &profiles)
{
	QStringList styleSheet;

	for (int i = 0; i < profiles.count(); ++i)
	{
		if (profiles[i] >= 0 && profiles[i] < m_profiles.count())
		{
			styleSheet += m_profiles.at(profiles[i])->getStyleSheet();
		}
	}

	return styleSheet;
}

QStringList ContentFilteringManager::getStyleSheetBlackList(const QString &domain, const QVector<int> &profiles)
{
	QStringList data;

	for (int i = 0; i < profiles.count(); ++i)
	{
		if (profiles[i] >= 0 && profiles[i] < m_profiles.count())
		{
			data.append(m_profiles.at(profiles[i])->getStyleSheetBlackList(domain));
		}
	}

	return data;
}

QStringList ContentFilteringManager::getStyleSheetWhiteList(const QString &domain, const QVector<int> &profiles)
{
	QStringList data;

	for (int i = 0; i < profiles.count(); ++i)
	{
		if (profiles[i] >= 0 && profiles[i] < m_profiles.count())
		{
			data.append(m_profiles.at(profiles[i])->getStyleSheetWhiteList(domain));
		}
	}

	return data;
}

QVector<ContentFilteringProfile*> ContentFilteringManager::getProfiles()
{
	if (m_profiles.isEmpty())
	{
		QStringList profiles;
		const QList<QFileInfo> existingProfiles(QDir(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering"))).entryInfoList(QStringList(QLatin1String("*.txt")), QDir::Files));
		QJsonObject bundledSettings;
		QFile bundledFile(SessionsManager::getReadableDataPath(QLatin1String("contentFiltering.json"), true));

		if (bundledFile.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			bundledSettings = QJsonDocument::fromJson(bundledFile.readAll()).object();

			bundledFile.close();
		}

		for (int i = 0; i < existingProfiles.count(); ++i)
		{
			const QString name(existingProfiles.at(i).completeBaseName());

			if (!profiles.contains(name))
			{
				profiles.append(name);
			}
		}

		QJsonObject::const_iterator iterator;

		for (iterator = bundledSettings.constBegin(); iterator != bundledSettings.constEnd(); ++iterator)
		{
			const QString name(iterator.key());

			if (!profiles.contains(name))
			{
				profiles.append(name);
			}
		}

		profiles.sort();

		m_profiles.reserve(profiles.count());

		QJsonObject settings;
		QFile file(SessionsManager::getWritableDataPath(QLatin1String("contentFiltering.json")));

		if (file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			settings = QJsonDocument::fromJson(file.readAll()).object();

			file.close();
		}

		const QHash<QString, ContentFilteringProfile::ProfileCategory> categoryTitles({{QLatin1String("advertisements"), ContentFilteringProfile::AdvertisementsCategory}, {QLatin1String("annoyance"), ContentFilteringProfile::AnnoyanceCategory}, {QLatin1String("privacy"), ContentFilteringProfile::PrivacyCategory}, {QLatin1String("social"), ContentFilteringProfile::SocialCategory}, {QLatin1String("regional"), ContentFilteringProfile::RegionalCategory}, {QLatin1String("other"), ContentFilteringProfile::OtherCategory}});

		for (int i = 0; i < profiles.count(); ++i)
		{
			QJsonObject profileSettings(settings.value(profiles.at(i)).toObject());
			const QJsonObject bundledProfileSettings(bundledSettings.value(profiles.at(i)).toObject());
			QString title;
			QString type;
			QUrl updateUrl;
			ContentFilteringProfile::ProfileFlags flags(ContentFilteringProfile::NoFlags);

			if (profiles.at(i) == QLatin1String("custom"))
			{
				title = tr("Custom Rules");
				type = QLatin1String("adBlock");
			}
			else if (profileSettings.isEmpty())
			{
				profileSettings = bundledProfileSettings;
				updateUrl = QUrl(profileSettings.value(QLatin1String("updateUrl")).toString());
				title = profileSettings.value(QLatin1String("title")).toString();
				type = profileSettings.value(QLatin1String("type")).toString();
			}
			else
			{
				updateUrl = QUrl(profileSettings.value(QLatin1String("updateUrl")).toString());
				title = profileSettings.value(QLatin1String("title")).toString();
				type = profileSettings.value(QLatin1String("type")).toString();

				if (updateUrl.isEmpty())
				{
					updateUrl = QUrl(bundledProfileSettings.value(QLatin1String("updateUrl")).toString());
				}
				else
				{
					flags |= ContentFilteringProfile::HasCustomUpdateUrlFlag;
				}

				if (title.isEmpty())
				{
					title = bundledProfileSettings.value(QLatin1String("title")).toString();
				}
				else
				{
					flags |= ContentFilteringProfile::HasCustomTitleFlag;
				}

				if (type.isEmpty())
				{
					type = bundledProfileSettings.value(QLatin1String("type")).toString();
				}
				else
				{
					flags |= ContentFilteringProfile::HasTypeFlag;
				}
			}

			const QJsonArray languages(profileSettings.value(QLatin1String("languages")).toArray());
			QList<QString> parsedLanguages;

			for (int j = 0; j < languages.count(); ++j)
			{
				parsedLanguages.append(languages.at(j).toString());
			}

			ContentFilteringProfile *profile(new AdblockContentFiltering(profiles.at(i), title, type, updateUrl, QDateTime::fromString(profileSettings.value(QLatin1String("lastUpdate")).toString(), Qt::ISODate), parsedLanguages, profileSettings.value(QLatin1String("updateInterval")).toInt(), categoryTitles.value(profileSettings.value(QLatin1String("category")).toString()), flags, m_instance));

			m_profiles.append(profile);

			connect(profile, SIGNAL(profileModified(QString)), m_instance, SIGNAL(profileModified(QString)));
			connect(profile, SIGNAL(profileModified(QString)), m_instance, SLOT(scheduleSave()));
		}
	}

	return m_profiles;
}

QVector<int> ContentFilteringManager::getProfileList(const QStringList &names)
{
	QVector<int> profiles;
	profiles.reserve(names.count());

	if (m_profiles.isEmpty())
	{
		getProfiles();
	}

	for (int i = 0; i < m_profiles.count(); ++i)
	{
		if (names.contains(m_profiles.at(i)->getName()))
		{
			profiles.append(i);
		}
	}

	return profiles;
}

ContentFilteringManager::CosmeticFiltersMode ContentFilteringManager::getCosmeticFiltersMode()
{
	return m_cosmeticFiltersMode;
}

bool ContentFilteringManager::areWildcardsEnabled()
{
	return m_areWildcardsEnabled;
}

bool ContentFilteringManager::updateProfile(const QString &profile)
{
	for (int i = 0; i < m_profiles.count(); ++i)
	{
		if (m_profiles.at(i)->getName() == profile)
		{
			return m_profiles[i]->update();
		}
	}

	return false;
}

}
