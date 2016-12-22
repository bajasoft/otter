/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2010 - 2014 David Rosca <nowrep@gmail.com>
* Copyright (C) 2014 - 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "ContentBlockingProfile.h"
#include "ContentBlockingResolver.h"
#include "ContentBlockingAdBlockResolver.h"
#include "Console.h"
#include "NetworkManagerFactory.h"
#include "SessionsManager.h"

#include <QtCore/QDir>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Otter
{

ContentBlockingProfile::ContentBlockingProfile(const QString &name, const QString &title, const QUrl &updateUrl, const QDateTime lastUpdate, const QList<QString> languages, int updateInterval, const ProfileCategory &category, const ProfileFlags &flags, QObject *parent) : QObject(parent),
	m_resolver(nullptr),
	m_networkReply(nullptr),
	m_name(name),
	m_title(title),
	m_updateUrl(updateUrl),
	m_lastUpdate(lastUpdate),
	m_languages({QLocale::AnyLanguage}),
	m_category(category),
	m_flags(flags),
	m_updateInterval(updateInterval),
	m_isUpdating(false),
	m_isEmpty(true),
	m_wasLoaded(false)
{
	if (!languages.isEmpty())
	{
		m_languages.clear();

		for (int i = 0; i < languages.count(); ++i)
		{
			m_languages.append(QLocale(languages.at(i)).language());
		}
	}

	loadHeader(getPath());
}

void ContentBlockingProfile::clear()
{
	if (!m_resolver || !m_wasLoaded)
	{
		return;
	}

	m_resolver->clear();

	m_wasLoaded = false;
}

void ContentBlockingProfile::loadHeader(const QString &path)
{
	QFile file(path);

	if (!file.exists())
	{
		return;
	}

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		Console::addMessage(QCoreApplication::translate("main", "Failed to open content blocking profile file: %1").arg(file.errorString()), Console::OtherCategory, Console::ErrorLevel, file.fileName());

		return;
	}

	QTextStream stream(&file);

	if (stream.readLine().trimmed().startsWith(QLatin1String("[Adblock Plus"), Qt::CaseInsensitive))
	{
		m_resolver = new ContentBlockingAdBlockResolver();

		while (!stream.atEnd())
		{
			QString line(stream.readLine().trimmed());

			if (!line.startsWith(QLatin1Char('!')))
			{
				m_isEmpty = false;

				break;
			}

			if (line.startsWith(QLatin1String("! Title: ")) && !m_flags.testFlag(HasCustomTitleFlag))
			{
				m_title = line.remove(QLatin1String("! Title: "));

				continue;
			}
		}
	}
	else
	{
		Console::addMessage(QCoreApplication::translate("main", "Failed to load content blocking profile file: invalid header"), Console::OtherCategory, Console::ErrorLevel, file.fileName());

		file.close();

		return;
	}

	file.close();

	if (!m_isUpdating && m_updateInterval > 0 && (!m_lastUpdate.isValid() || m_lastUpdate.daysTo(QDateTime::currentDateTime()) > m_updateInterval))
	{
		update();
	}
}

void ContentBlockingProfile::updateReady()
{
	m_isUpdating = false;

	if (!m_networkReply)
	{
		return;
	}

	m_networkReply->deleteLater();

	QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking")));

	QFile file(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking/%1.txt")).arg(m_name));

	if (m_resolver->parseUpdate(m_networkReply, file))
	{
		m_lastUpdate = QDateTime::currentDateTime();

		if (m_resolver)
		{
			m_resolver->clear();
		}

		loadHeader(getPath());

		if (m_wasLoaded)
		{
			loadRules();
		}

		emit profileModified(m_name);
	}
}

void ContentBlockingProfile::setUpdateInterval(int interval)
{
	if (interval != m_updateInterval)
	{
		m_updateInterval = interval;

		emit profileModified(m_name);
	}
}

void ContentBlockingProfile::setUpdateUrl(const QUrl &url)
{
	if (url.isValid() && url != m_updateUrl)
	{
		m_updateUrl = url;
		m_flags |= HasCustomUpdateUrlFlag;

		emit profileModified(m_name);
	}
}

void ContentBlockingProfile::setCategory(const ProfileCategory &category)
{
	if (category != m_category)
	{
		m_category = category;

		emit profileModified(m_name);
	}
}

void ContentBlockingProfile::setTitle(const QString &title)
{
	if (title != m_title)
	{
		m_title = title;
		m_flags |= HasCustomTitleFlag;

		emit profileModified(m_name);
	}
}

QString ContentBlockingProfile::getName() const
{
	return m_name;
}

QString ContentBlockingProfile::getTitle() const
{
	return (m_title.isEmpty() ? tr("(Unknown)") : m_title);
}

QString ContentBlockingProfile::getPath() const
{
	return SessionsManager::getWritableDataPath(QLatin1String("contentBlocking/%1.txt")).arg(m_name);
}

QDateTime ContentBlockingProfile::getLastUpdate() const
{
	return m_lastUpdate;
}

QUrl ContentBlockingProfile::getUpdateUrl() const
{
	return m_updateUrl;
}

ContentBlockingManager::CheckResult ContentBlockingProfile::checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	ContentBlockingManager::CheckResult result;

	if (!m_resolver || (!m_wasLoaded && !loadRules()))
	{
		return result;
	}

	return m_resolver->checkUrl(baseUrl, requestUrl, resourceType);
}

QStringList ContentBlockingProfile::getStyleSheet()
{
	if (!m_wasLoaded)
	{
		loadRules();
	}

	QStringList styleSheets;

	if (m_resolver)
	{
		styleSheets = m_resolver->getStyleSheet();
	}

	return styleSheets;
}

QStringList ContentBlockingProfile::getStyleSheetBlackList(const QString &domain)
{
	if (!m_wasLoaded)
	{
		loadRules();
	}

	QStringList styleSheets;

	if (m_resolver)
	{
		styleSheets = m_resolver->getStyleSheetBlackList(domain);
	}

	return styleSheets;
}

QStringList ContentBlockingProfile::getStyleSheetWhiteList(const QString &domain)
{
	if (!m_wasLoaded)
	{
		loadRules();
	}

	QStringList styleSheets;

	if (m_resolver)
	{
		styleSheets = m_resolver->getStyleSheetWhiteList(domain);
	}

	return styleSheets;
}

QList<QLocale::Language> ContentBlockingProfile::getLanguages() const
{
	return m_languages;
}

ContentBlockingProfile::ProfileCategory ContentBlockingProfile::getCategory() const
{
	return m_category;
}

ContentBlockingProfile::ProfileFlags ContentBlockingProfile::getFlags() const
{
	return m_flags;
}

int ContentBlockingProfile::getUpdateInterval() const
{
	return m_updateInterval;
}

bool ContentBlockingProfile::update()
{
	if (m_isUpdating)
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

	m_isUpdating = true;

	return true;
}

bool ContentBlockingProfile::loadRules()
{
	if (m_isEmpty && !m_updateUrl.isEmpty())
	{
		update();

		return false;
	}

	m_wasLoaded = true;

	QFile file(getPath());

	if (m_resolver)
	{
		return m_resolver->loadRules(file);
	}

	return false;
}

}
