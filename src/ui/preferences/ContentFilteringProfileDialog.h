/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#ifndef ContentFilteringPROFILEDIALOG
#define ContentFilteringPROFILEDIALOG

#include "../Dialog.h"

namespace Otter
{

namespace Ui
{
	class ContentFilteringProfileDialog;
}

class ContentFilteringProfile;

class ContentFilteringProfileDialog : public Dialog
{
	Q_OBJECT

public:
	explicit ContentFilteringProfileDialog(QWidget *parent = nullptr, ContentFilteringProfile *profile = nullptr);
	~ContentFilteringProfileDialog();

	ContentFilteringProfile* getProfile();

protected:
	void changeEvent(QEvent *event);

protected slots:
	void save();

private:
	ContentFilteringProfile *m_profile;
	Ui::ContentFilteringProfileDialog *m_ui;
};

}

#endif

