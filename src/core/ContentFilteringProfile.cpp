/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2010 - 2014 David Rosca <nowrep@gmail.com>
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
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ContentFilteringProfile.h"
#include "Console.h"
#include "NetworkManagerFactory.h"
#include "SessionsManager.h"

#include <QtCore/QDir>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Otter
{

ContentFilteringProfile::ContentFilteringProfile(const QString &name, const QString &title, const QString &type, const QUrl &updateUrl, const QDateTime lastUpdate, const QList<QString> languages, int updateInterval, const ProfileCategory &category, const ProfileFlags &flags, QObject *parent) : Addon(parent),
	m_networkReply(nullptr),
	m_name(name),
	m_title(title),
	m_updateUrl(updateUrl),
	m_lastUpdate(lastUpdate),
	m_languages({QLocale::AnyLanguage}),
	m_category(category),
	m_flags(flags),
	m_updateInterval(updateInterval),
	m_isLoaded(false)
{
	if (!languages.isEmpty())
	{
		m_languages.clear();

		for (int i = 0; i < languages.count(); ++i)
		{
			m_languages.append(QLocale(languages.at(i)).language());
		}
	}

	if (validate(getPath()) && m_updateInterval > 0 && (!m_lastUpdate.isValid() || m_lastUpdate.daysTo(QDateTime::currentDateTime()) > m_updateInterval))
	{
		update();
	}
}

void ContentFilteringProfile::updateReady()
{
	if (!m_networkReply)
	{
		return;
	}

	m_networkReply->deleteLater();

	QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering")));

	QFile file(SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering/%1.txt")).arg(m_name));

	if (parseUpdate(m_networkReply, file))
	{
		m_lastUpdate = QDateTime::currentDateTime();

		emit profileModified(m_name);
	}
}

void ContentFilteringProfile::setUpdateInterval(int interval)
{
	if (interval != m_updateInterval)
	{
		m_updateInterval = interval;

		emit profileModified(m_name);
	}
}

void ContentFilteringProfile::setUpdateUrl(const QUrl &url)
{
	if (url.isValid() && url != m_updateUrl)
	{
		m_updateUrl = url;
		m_flags |= HasCustomUpdateUrlFlag;

		emit profileModified(m_name);
	}
}

void ContentFilteringProfile::setCategory(const ProfileCategory &category)
{
	if (category != m_category)
	{
		m_category = category;

		emit profileModified(m_name);
	}
}

void ContentFilteringProfile::setTitle(const QString &title)
{
	if (title != m_title)
	{
		m_title = title;
		m_flags |= HasCustomTitleFlag;

		emit profileModified(m_name);
	}
}

QString ContentFilteringProfile::getName() const
{
	return m_name;
}

QString ContentFilteringProfile::getTitle() const
{
	return (m_title.isEmpty() ? tr("(Unknown)") : m_title);
}

QString ContentFilteringProfile::getPath() const
{
	return SessionsManager::getWritableDataPath(QLatin1String("ContentFiltering/%1.txt")).arg(m_name);
}

QString ContentFilteringProfile::getDescription() const
{
	return QString();
}

QString ContentFilteringProfile::getVersion() const
{
	return QString();
}

QDateTime ContentFilteringProfile::getLastUpdate() const
{
	return m_lastUpdate;
}

QUrl ContentFilteringProfile::getUpdateUrl() const
{
	return m_updateUrl;
}

QUrl ContentFilteringProfile::getHomePage() const
{
	return QUrl();
}

QIcon ContentFilteringProfile::getIcon() const
{
	return QIcon();
}

ContentFilteringManager::CheckResult ContentFilteringProfile::checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	return ContentFilteringManager::CheckResult();
}

bool ContentFilteringProfile::profileValidation()
{
	if (!m_isLoaded && !loadRules())
	{
		return false;
	}

	return true;
}

QStringList ContentFilteringProfile::getStyleSheet()
{
	return QStringList();
}

QStringList ContentFilteringProfile::getStyleSheetBlackList(const QString &domain)
{
	return QStringList();
}

QStringList ContentFilteringProfile::getStyleSheetWhiteList(const QString &domain)
{
	return QStringList();
}

bool ContentFilteringProfile::parseUpdate(QNetworkReply *reply, QFile &file)
{
	return false;
}

QList<QLocale::Language> ContentFilteringProfile::getLanguages() const
{
	return m_languages;
}

ContentFilteringProfile::ProfileCategory ContentFilteringProfile::getCategory() const
{
	return m_category;
}

ContentFilteringProfile::ProfileFlags ContentFilteringProfile::getFlags() const
{
	return m_flags;
}

int ContentFilteringProfile::getUpdateInterval() const
{
	return m_updateInterval;
}

bool ContentFilteringProfile::isLoaded(bool reload)
{
	if (!reload)
	{
		return m_isLoaded;
	}

	if (!m_isLoaded)
	{
		loadRules();
	}

	return m_isLoaded;
}

void ContentFilteringProfile::setLoaded(bool loaded)
{
	m_isLoaded = loaded;
}

bool ContentFilteringProfile::clear()
{
	return true;
}

bool ContentFilteringProfile::loadingValidation()
{
	if (!QFile(getPath()).exists() && !m_updateUrl.isEmpty())
	{
		update();

		return false;
	}

	setLoaded(true);

	return true;
}

bool ContentFilteringProfile::loadRules()
{
	return false;
}

bool ContentFilteringProfile::update()
{
	if (m_networkReply && m_networkReply->isRunning())
	{
		return false;
	}

	if (!m_updateUrl.isValid())
	{
		const QString path(getPath());

		if (m_updateUrl.isEmpty())
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to update content blocking profile, update URL is empty"), Console::OtherCategory, Console::ErrorLevel, path);
		}
		else
		{
			Console::addMessage(QCoreApplication::translate("main", "Failed to update content blocking profile, update URL (%1) is invalid").arg(m_updateUrl.toString()), Console::OtherCategory, Console::ErrorLevel, path);
		}

		return false;
	}

	QNetworkRequest request(m_updateUrl);
	request.setHeader(QNetworkRequest::UserAgentHeader, NetworkManagerFactory::getUserAgent());

	m_networkReply = NetworkManagerFactory::getNetworkManager()->get(request);

	connect(m_networkReply, SIGNAL(finished()), this, SLOT(updateReady()));

	return true;
}

bool ContentFilteringProfile::fileValidation(QFile &file)
{
	if (!file.exists())
	{
		return false;
	}

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		Console::addMessage(QCoreApplication::translate("main", "Failed to open content filtering profile file: %1").arg(file.errorString()), Console::OtherCategory, Console::ErrorLevel, file.fileName());

		return false;
	}

	return true;
}

void ContentFilteringProfile::loadTitle(const QString &title)
{
	if (!m_flags.testFlag(HasCustomTitleFlag))
	{
		if (!title.isEmpty())
		{
			m_title = title;
		}
	}
}

bool ContentFilteringProfile::validate(const QString &path)
{
	return true;
}

bool ContentFilteringProfile::resolveDomainExceptions(const QString &url, const QStringList &ruleList)
{
	for (int i = 0; i < ruleList.count(); ++i)
	{
		if (url.contains(ruleList.at(i)))
		{
			return true;
		}
	}

	return false;
}

}
