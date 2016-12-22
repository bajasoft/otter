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

#include "ContentBlockingResolver.h"

namespace Otter
{

ContentBlockingResolver::ContentBlockingResolver(QObject *parent) : QObject(parent)
{
}

void ContentBlockingResolver::clear()
{
}

void ContentBlockingResolver::parseRuleLine(QString line)
{
	Q_UNUSED(line);
}

ContentBlockingManager::CheckResult ContentBlockingResolver::checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	Q_UNUSED(baseUrl);
	Q_UNUSED(requestUrl);
	Q_UNUSED(resourceType);

	return ContentBlockingManager::CheckResult();
}

QStringList ContentBlockingResolver::getStyleSheet()
{
	return QStringList();
}

QStringList ContentBlockingResolver::getStyleSheetBlackList(const QString &domain)
{
	Q_UNUSED(domain);

	return QStringList();
}

QStringList ContentBlockingResolver::getStyleSheetWhiteList(const QString &domain)
{
	Q_UNUSED(domain);

	return QStringList();
}

bool ContentBlockingResolver::loadRules(QFile &file)
{
	return false;
}

bool ContentBlockingResolver::parseUpdate(QNetworkReply *reply, QFile &file)
{
	return true;
}

bool ContentBlockingResolver::resolveDomainExceptions(const QString &url, const QStringList &ruleList)
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
