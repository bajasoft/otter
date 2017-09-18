/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 - 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
* Copyright (C) 2016 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "WindowsNativeEventFilter.h"
#include "WindowsPlatformIntegration.h"
#include "../../../core/Application.h"
#include "../../../ui/MainWindow.h"

#include <QtCore/QAbstractNativeEventFilter>

#include <shlobj.h>

namespace Otter
{

bool WindowsNativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	static unsigned int taskBarCreatedId = WM_NULL;

	MSG* recievedMessage = static_cast<MSG*>(message);

	if (taskBarCreatedId == WM_NULL)
	{
		taskBarCreatedId = RegisterWindowMessage(QString("TaskbarButtonCreated").toStdWString().c_str());

		return false;
	}

	if (recievedMessage->message == taskBarCreatedId && recievedMessage->hwnd == (HWND)Application::getInstance()->getWindow()->winId() /*getInstance()->m_parentWidget->winId()*/)
	{
		//HRESULT result = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList4, (LPVOID*)m_taskbar /*reinterpret_cast<void**> (&(m_taskbar))*/);

		//if (result == S_OK)
		//{
		//	result = m_taskbar->HrInit();

		//	if (result != S_OK)
		//	{
		//		m_taskbar = nullptr;
		//	}
		//}

		qobject_cast<WindowsPlatformIntegration*>(Application::getInstance()->getPlatformIntegration())->createTaskBar();

		return true;
	}

	return false;
}

}
