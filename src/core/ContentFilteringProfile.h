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

#ifndef OTTER_ContentFilteringPROFILE_H
#define OTTER_ContentFilteringPROFILE_H

#include "AddonsManager.h"
#include "ContentFilteringManager.h"

namespace Otter
{

class ContentFilteringProfile : public Addon
{
	Q_OBJECT

public:
	enum ProfileFlag
	{
		NoFlags = 0,
		HasCustomTitleFlag = 1,
		HasCustomUpdateUrlFlag = 2,
		HasTypeFlag = 4
	};

	Q_DECLARE_FLAGS(ProfileFlags, ProfileFlag)

	enum ProfileCategory
	{
		OtherCategory = 0,
		AdvertisementsCategory = 1,
		AnnoyanceCategory = 2,
		PrivacyCategory = 4,
		SocialCategory = 8,
		RegionalCategory = 16
	};

	explicit ContentFilteringProfile(const QString &name, const QString &title, const QString &type, const QUrl &updateUrl, const QDateTime lastUpdate, const QList<QString> languages, int updateInterval, const ProfileCategory &category, const ProfileFlags &flags, QObject *parent = nullptr);

	void setCategory(const ProfileCategory &category);
	void setTitle(const QString &title);
	void setUpdateInterval(int interval);
	void setUpdateUrl(const QUrl &url);
	QString getName() const;
	virtual QString getTitle() const;
	QUrl getUpdateUrl() const;
	QDateTime getLastUpdate() const;
	virtual ContentFilteringManager::CheckResult checkUrl(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType);
	virtual QStringList getStyleSheet();
	virtual QStringList getStyleSheetBlackList(const QString &domain);
	virtual QStringList getStyleSheetWhiteList(const QString &domain);
	QList<QLocale::Language> getLanguages() const;
	ProfileCategory getCategory() const;
	ProfileFlags getFlags() const;
	int getUpdateInterval() const;
	virtual bool clear();
	bool update();

protected:
	void loadTitle(const QString &title);
	QString getPath() const;
	virtual bool loadRules();
	virtual bool validate(const QString &path);
	bool checkFile(QFile &file);
	bool checkLoadingState();
	bool beforeCheckUrl();
	bool loadRulesCheck();
	bool resolveDomainExceptions(const QString &url, const QStringList &ruleList);
	//virtual bool loadRules(QFile &file);
	//virtual bool parseUpdate(QNetworkReply *reply, QFile &file);
	//virtual bool validate(QFile &file);

protected slots:
	void updateReady();

private:
	//ContentFiltering *m_resolver;
	QNetworkReply *m_networkReply;
	QString m_name;
	QString m_title;
	QUrl m_updateUrl;
	QDateTime m_lastUpdate;
	QList<QLocale::Language> m_languages;
	ProfileCategory m_category;
	ProfileFlags m_flags;
	int m_updateInterval;
	bool m_wasLoaded;

signals:
	void profileModified(const QString &profile);
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Otter::ContentFilteringProfile::ProfileFlags)

#endif
