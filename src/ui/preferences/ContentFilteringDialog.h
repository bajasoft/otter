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

#ifndef OTTER_ContentFilteringDIALOG_H
#define OTTER_ContentFilteringDIALOG_H

#include "../Dialog.h"

namespace Otter
{

namespace Ui
{
	class ContentFilteringDialog;
}

class ContentFilteringProfile;

class ContentFilteringDialog : public Dialog
{
	Q_OBJECT

public:
	explicit ContentFilteringDialog(QWidget *parent = nullptr);
	~ContentFilteringDialog();

protected:
	void changeEvent(QEvent *event);
	void updateModel(ContentFilteringProfile *profile, bool isNewOrMoved);

protected slots:
	void addProfile();
	void editProfile();
	void updateProfile();
	void updateProfilesActions();
	void addRule();
	void editRule();
	void removeRule();
	void updateRulesActions();
	void updateProfile(const QString &name);
	void save();

private:
	Ui::ContentFilteringDialog *m_ui;
};

}

#endif
