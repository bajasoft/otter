/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2010 - 2014 David Rosca <nowrep@gmail.com>
* Copyright (C) 2014 - 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
* Copyright (C) 2015 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>

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

#ifndef OTTER_CONTENTBLOCKINGRESOLVER_H
#define OTTER_CONTENTBLOCKINGRESOLVER_H

#include "ContentBlockingManager.h"

#include <QtCore/QFile>

namespace Otter
{

class ContentBlockingResolver : public QObject
{
	Q_OBJECT

public:
	explicit ContentBlockingResolver(QObject *parent = nullptr);

	virtual void clear();
	virtual ContentBlockingManager::CheckResult checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType);
	virtual QStringList getStyleSheet();
	virtual QStringList getStyleSheetBlackList(const QString &domain);
	virtual QStringList getStyleSheetWhiteList(const QString &domain);
	virtual bool loadRules(QFile &file);
	virtual bool parseUpdate(QNetworkReply *reply, QFile &file);

protected:
	virtual void parseRuleLine(QString line);
	virtual bool resolveDomainExceptions(const QString &url, const QStringList &ruleList);
};

}

#endif
