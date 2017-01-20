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

#include "ContentFiltering.h"

namespace Otter
{

ContentFiltering::ContentFiltering(QObject *parent) : QObject(parent)
{
}

void ContentFiltering::clear()
{
}

ContentFilteringManager::CheckResult ContentFiltering::checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	Q_UNUSED(baseUrl);
	Q_UNUSED(requestUrl);
	Q_UNUSED(resourceType);

	return ContentFilteringManager::CheckResult();
}

QString ContentFiltering::getTitle() const
{
	return QString();
}

QStringList ContentFiltering::getStyleSheet()
{
	return QStringList();
}

QStringList ContentFiltering::getStyleSheetBlackList(const QString &domain)
{
	Q_UNUSED(domain);

	return QStringList();
}

QStringList ContentFiltering::getStyleSheetWhiteList(const QString &domain)
{
	Q_UNUSED(domain);

	return QStringList();
}

bool ContentFiltering::loadRules(QFile &file)
{
	return false;
}

bool ContentFiltering::parseUpdate(QNetworkReply *reply, QFile &file)
{
	return true;
}

bool ContentFiltering::resolveDomainExceptions(const QString &url, const QStringList &ruleList)
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

bool ContentFiltering::validate(QFile &file)
{
	return false;
}

}
