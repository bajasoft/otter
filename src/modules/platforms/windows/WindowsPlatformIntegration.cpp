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

#include "WindowsPlatformIntegration.h"
#include "WindowsPlatformStyle.h"
#include "../../../core/Application.h"
#include "../../../core/Console.h"
#include "../../../core/NotificationsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/TransfersManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/MainWindow.h"
#include "../../../ui/NotificationDialog.h"
#include "../../../ui/TrayIcon.h"
#include "../../../ui/Window.h"

#include <windows.h>
//#include <qwinfunctions.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QMimeData>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtMath>
#include <QtGui/QDesktopServices>
#include <QtGui/QDrag>
#include <QtWidgets/QFileIconProvider>
//#include <QtWinExtras/QtWin>
#include <QtWinExtras/QWinJumpList>
#include <QtWinExtras/QWinJumpListCategory>

namespace Otter
{

extern "C"
{
	typedef HRESULT(WINAPI *t_DwmInvalidateIconicBitmaps)(HWND hwnd);
	typedef HRESULT(WINAPI *t_DwmSetIconicThumbnail)(HWND hwnd, HBITMAP hbmp, DWORD dwSITFlags);
	typedef HRESULT(WINAPI *t_DwmSetIconicLivePreviewBitmap)(HWND hwnd, HBITMAP hbmp, POINT *pptClient, DWORD dwSITFlags);
	typedef HRESULT(WINAPI *t_DwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
}

// Window attributes
enum DWMWINDOWATTRIBUTE
{
	DWMWA_NCRENDERING_ENABLED = 1,      // [get] Is non-client rendering enabled/disabled
	DWMWA_NCRENDERING_POLICY,           // [set] Non-client rendering policy
	DWMWA_TRANSITIONS_FORCEDISABLED,    // [set] Potentially enable/forcibly disable transitions
	DWMWA_ALLOW_NCPAINT,                // [set] Allow contents rendered in the non-client area to be visible on the DWM-drawn frame.
	DWMWA_CAPTION_BUTTON_BOUNDS,        // [get] Bounds of the caption button area in window-relative space.
	DWMWA_NONCLIENT_RTL_LAYOUT,         // [set] Is non-client content RTL mirrored
	DWMWA_FORCE_ICONIC_REPRESENTATION,  // [set] Force this window to display iconic thumbnails.
	DWMWA_FLIP3D_POLICY,                // [set] Designates how Flip3D will treat the window.
	DWMWA_EXTENDED_FRAME_BOUNDS,        // [get] Gets the extended frame bounds rectangle in screen space
	DWMWA_HAS_ICONIC_BITMAP,            // [set] Indicates an available bitmap when there is no better thumbnail representation.
	DWMWA_DISALLOW_PEEK,                // [set] Don't invoke Peek on the window.
	DWMWA_EXCLUDED_FROM_PEEK,           // [set] LivePreview exclusion information
	DWMWA_LAST
};

QProcessEnvironment WindowsPlatformIntegration::m_environment;

WindowsPlatformIntegration::WindowsPlatformIntegration(Application *parent) : PlatformIntegration(parent),
	m_taskbar(nullptr),
	m_registrationIdentifier(QLatin1String("OtterBrowser")),
	m_applicationFilePath(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())),
	m_applicationRegistration(QLatin1String("HKEY_CURRENT_USER\\Software\\RegisteredApplications"), QSettings::NativeFormat),
	m_propertiesRegistration(QLatin1String("HKEY_CURRENT_USER\\Software\\Classes\\") + m_registrationIdentifier, QSettings::NativeFormat),
	m_registrationPairs({{QLatin1String("http"), ProtocolType}, {QLatin1String("https"), ProtocolType}, {QLatin1String("ftp"), ProtocolType}, {QLatin1String(".htm"), ExtensionType}, {QLatin1String(".html"), ExtensionType}, {QLatin1String(".xhtml"), ExtensionType}}),
	m_cleanupTimer(0)
{
	if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7)
	{
		connect(Application::getInstance(), SIGNAL(windowRemoved(MainWindow*)), this, SLOT(removeWindow(MainWindow*)));
		connect(TransfersManager::getInstance(), SIGNAL(transferChanged(Transfer*)), this, SLOT(updateTaskbarButtons()));
		connect(TransfersManager::getInstance(), SIGNAL(transferStarted(Transfer*)), this, SLOT(updateTaskbarButtons()));
		connect(TransfersManager::getInstance(), SIGNAL(transferFinished(Transfer*)), this, SLOT(updateTaskbarButtons()));
		connect(TransfersManager::getInstance(), SIGNAL(transferRemoved(Transfer*)), this, SLOT(updateTaskbarButtons()));
		connect(TransfersManager::getInstance(), SIGNAL(transferStopped(Transfer*)), this, SLOT(updateTaskbarButtons()));
	}

	if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA)
	{
		const QString applicationFilePath(QCoreApplication::applicationFilePath());
		QWinJumpList jumpLists;
		QWinJumpListCategory* tasks(jumpLists.tasks());
		tasks->addLink(ThemesManager::createIcon(QLatin1String("tab-new")), tr("New tab"), applicationFilePath, QStringList(QLatin1String("--new-tab")));
		tasks->addLink(ThemesManager::createIcon(QLatin1String("tab-new-private")), tr("New private tab"), applicationFilePath, QStringList(QLatin1String("--new-private-tab")));
		tasks->addLink(ThemesManager::createIcon(QLatin1String("window-new")), tr("New window"), applicationFilePath, QStringList(QLatin1String("--new-window")));
		tasks->addLink(ThemesManager::createIcon(QLatin1String("window-new-private")), tr("New private window"), applicationFilePath, QStringList(QLatin1String("--new-private-window")));
		tasks->setVisible(true);
	}

	Application::getInstance()->installNativeEventFilter(&m_eventFilter);
}

void WindowsPlatformIntegration::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_cleanupTimer)
	{
		killTimer(m_cleanupTimer);

		m_cleanupTimer = 0;
		m_environment.clear();
	}
}

void WindowsPlatformIntegration::addTabThumbnail(Window* window)
{
	QWidget* widget(window->topLevelWidget());

	if (m_taskbar)
	{
		enableWidgetIconicPreview(widget);

		qDebug() << "added";
		m_taskbar->RegisterTab((HWND)widget->winId(), (HWND)window->getMainWindow()->winId());
		m_taskbar->SetTabOrder((HWND)widget->winId(), NULL);
		m_taskbar->SetTabActive(NULL, (HWND)widget->winId(), 0);
	}
}

void WindowsPlatformIntegration::createTaskBar()
{
	HRESULT result = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList4, /*(LPVOID*)m_taskbar*/ reinterpret_cast<void**> (&(m_taskbar)));
	
	if (result == S_OK)
	{
		result = m_taskbar->HrInit();
	
		if (result != S_OK)
		{
			m_taskbar = nullptr;
		}
		else {
			emit thumbnailsInitialized();
		}
	}
}

void WindowsPlatformIntegration::enableWidgetIconicPreview(QWidget* widget)
{
	BOOL enable = TRUE;

	setWindowAttribute((HWND)widget->winId(), DWMWA_FORCE_ICONIC_REPRESENTATION, &enable, sizeof(enable));
	setWindowAttribute((HWND)widget->winId(), DWMWA_HAS_ICONIC_BITMAP, &enable, sizeof(enable));
}

void WindowsPlatformIntegration::setWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
	HMODULE shell;

	shell = LoadLibrary(L"dwmapi.dll");
	if (shell) {
		t_DwmSetWindowAttribute set_window_attribute = reinterpret_cast<t_DwmSetWindowAttribute>(GetProcAddress(shell, "DwmSetWindowAttribute"));
		set_window_attribute(hwnd, dwAttribute, pvAttribute, cbAttribute);

		FreeLibrary(shell);
	}
}

void WindowsPlatformIntegration::setIconicThumbnail(HWND hwnd, QSize size)
{
	QWidget* widget(nullptr);
	const QVector<MainWindow*> windows(Application::getWindows());

	for (int i = 0; i < windows.count(); ++i)
	{
		for (int j = 0; j < windows.at(i)->getWindowCount(); ++j)
		{
			qDebug() << "searching " << hwnd << " - " << (HWND)windows.at(i)->getWindowByIndex(j)->winId();
			if ((HWND)windows.at(i)->getWindowByIndex(j)->winId() == hwnd)
			{
				qDebug() << "found! " << hwnd;
				widget = windows.at(i)->getWindowByIndex(j)->topLevelWidget();
			}
		}
	}

	if (widget)
	{
		QPixmap thumbnail = QPixmap::grabWidget(widget).scaled(size, Qt::KeepAspectRatio);

		//QPixmap::Alpha in case the image has transparent regions
		HBITMAP hbitmap = QtWin::toHBITMAP(thumbnail); // thumbnail.toWinHBITMAP(QPixmap::Alpha);
		//QtWin::toHBITMAP
		//DwmSetIconicThumbnail(id, hbitmap, 0);

		HMODULE shell;

		shell = LoadLibrary(L"dwmapi.dll");
		if (shell) {
			t_DwmSetIconicThumbnail set_iconic_thumbnail = reinterpret_cast<t_DwmSetIconicThumbnail>(GetProcAddress(shell, "DwmSetIconicThumbnail"));
			set_iconic_thumbnail(hwnd, hbitmap, 0);

			FreeLibrary(shell);
		}

		
		DeleteObject(hbitmap);
	}
}

void WindowsPlatformIntegration::removeWindow(MainWindow *window)
{
	if (m_taskbarButtons.contains(window))
	{
		m_taskbarButtons.remove(window);
	}
}

void WindowsPlatformIntegration::updateTaskbarButtons()
{
	const QVector<Transfer*> transfers(TransfersManager::getInstance()->getTransfers());
	qint64 bytesTotal(0);
	qint64 bytesReceived(0);
	bool hasActiveTransfers(false);

	for (int i = 0; i < transfers.count(); ++i)
	{
		if (transfers[i]->getState() == Transfer::RunningState && transfers[i]->getBytesTotal() > 0)
		{
			hasActiveTransfers = true;
			bytesTotal += transfers[i]->getBytesTotal();
			bytesReceived += transfers[i]->getBytesReceived();
		}
	}

	const QVector<MainWindow*> windows(Application::getWindows());

	for (int i = 0; i < windows.count(); ++i)
	{
		MainWindow *window(windows.at(i));

		if (hasActiveTransfers)
		{
			if (!m_taskbarButtons.contains(window))
			{
				m_taskbarButtons[window] = new QWinTaskbarButton(window);
				m_taskbarButtons[window]->setWindow(window->windowHandle());
				m_taskbarButtons[window]->progress()->show();
			}

			m_taskbarButtons[window]->progress()->setValue((bytesReceived > 0) ? qFloor((static_cast<qreal>(bytesReceived) / bytesTotal) * 100) : 0);
		}
		else if (m_taskbarButtons.contains(window))
		{
			m_taskbarButtons[window]->progress()->reset();
			m_taskbarButtons[window]->progress()->hide();
			m_taskbarButtons[window]->deleteLater();
			m_taskbarButtons.remove(window);
		}
	}
}

void WindowsPlatformIntegration::showNotification(Notification *notification)
{
	TrayIcon *trayIcon(Application::getTrayIcon());

	if (trayIcon && QSystemTrayIcon::supportsMessages())
	{
		trayIcon->showMessage(notification);
	}
	else
	{
		NotificationDialog *dialog(new NotificationDialog(notification));
		dialog->show();
	}
}

void WindowsPlatformIntegration::runApplication(const QString &command, const QUrl &url) const
{
	if (command.isEmpty())
	{
		QDesktopServices::openUrl(url);

		return;
	}

	const int indexOfExecutable(command.indexOf(QLatin1String(".exe"), 0, Qt::CaseInsensitive));

	if (indexOfExecutable == -1)
	{
		Console::addMessage(tr("Failed to run command \"%1\", file is not executable").arg(command), Console::OtherCategory, Console::ErrorLevel);

		return;
	}

	const QString application(command.left(indexOfExecutable + 4));
	QStringList arguments(command.mid(indexOfExecutable + 5).split(QLatin1Char(' '), QString::SkipEmptyParts));
	const int indexOfPlaceholder(arguments.indexOf(QLatin1String("%1")));

	if (url.isValid())
	{
		if (indexOfPlaceholder > -1)
		{
			arguments.replace(indexOfPlaceholder, arguments.at(indexOfPlaceholder).arg(url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.url()));
		}
		else
		{
			arguments.append(url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.url());
		}
	}
	else if (indexOfPlaceholder > -1)
	{
		arguments.removeAt(indexOfPlaceholder);
	}

	if (!QProcess::startDetached(application, arguments))
	{
		Console::addMessage(tr("Failed to run command \"%1\" (arguments: \"%2\")").arg(command).arg(arguments.join(QLatin1Char(' '))), Console::OtherCategory, Console::ErrorLevel);
	}
}

void WindowsPlatformIntegration::getApplicationInformation(ApplicationInformation &information)
{
	const QString rootPath(information.command.left(information.command.indexOf(QLatin1String("\\"))).remove(QLatin1Char('%')));

	if (m_environment.contains(rootPath))
	{
		information.command.replace(QLatin1Char('%') + rootPath + QLatin1Char('%'), m_environment.value(rootPath));
	}

	const QString fullApplicationPath(information.command.left(information.command.indexOf(QLatin1String(".exe"), 0, Qt::CaseInsensitive) + 4));
	const QFileInfo fileInformation(fullApplicationPath);
	HKEY key = nullptr;
	TCHAR readBuffer[128];
	DWORD bufferSize = sizeof(readBuffer);

	if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache"), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(key, fullApplicationPath.toStdWString().c_str(), nullptr, nullptr, (LPBYTE)readBuffer, &bufferSize) == ERROR_SUCCESS)
		{
			information.name = QString::fromWCharArray(readBuffer);
		}

		RegCloseKey(key);
	}

	if (information.name.isEmpty())
	{
		information.name = fileInformation.baseName();
	}

	information.icon = QFileIconProvider().icon(fileInformation);
}

void WindowsPlatformIntegration::startLinkDrag(const QUrl &url, const QString &title, const QPixmap &pixmap, QObject *parent) const
{
	QDrag *drag(new QDrag(parent));
	QMimeData *mimeData(new QMimeData());
	mimeData->setText(url.toString());

	QTemporaryDir directory;
	QFile file(directory.path() + QDir::separator() + (url.host().isEmpty() ? QLatin1String("localhost") : url.host()) + QLatin1String(".url"));

	if (file.open(QIODevice::WriteOnly))
	{
		file.write((QLatin1String("[InternetShortcut]\r\nURL=") + url.toString()).toUtf8());
		file.close();

		mimeData->setUrls({url, QUrl::fromLocalFile(file.fileName())});
	}
	else
	{
		mimeData->setUrls({url});
	}

	if (!title.isEmpty())
	{
		mimeData->setProperty("x-url-title", title);
	}

	mimeData->setProperty("x-url-string", url.toString());

	drag->setMimeData(mimeData);
	drag->setPixmap(pixmap);
	drag->exec(Qt::MoveAction);
}

Style* WindowsPlatformIntegration::createStyle(const QString &name) const
{
	if (name.isEmpty() || name.toLower().startsWith(QLatin1String("windows")))
	{
		return new WindowsPlatformStyle(name);
	}

	return nullptr;
}

QVector<ApplicationInformation> WindowsPlatformIntegration::getApplicationsForMimeType(const QMimeType &mimeType)
{
	QVector<ApplicationInformation> applications;
	const QString suffix(mimeType.preferredSuffix());

	if (suffix.isEmpty())
	{
		Console::addMessage(tr("No valid suffix for given MIME type: %1").arg(mimeType.name()), Console::OtherCategory, Console::ErrorLevel);

		return applications;
	}

	if (suffix == QLatin1String("exe"))
	{
		return applications;
	}

	if (m_cleanupTimer != 0)
	{
		killTimer(m_cleanupTimer);

		m_cleanupTimer = 0;
	}

	if (m_environment.isEmpty())
	{
		m_environment = QProcessEnvironment::systemEnvironment();
	}

	// Vista+ applications
	const QSettings defaultAssociation(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.") + suffix, QSettings::NativeFormat);
	QString defaultApplication(defaultAssociation.value(QLatin1String("."), QString()).toString());
	QStringList associations;

	if (defaultApplication.isEmpty())
	{
		const QSettings defaultAssociation(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Classes\\.") + suffix, QSettings::NativeFormat);

		defaultApplication = defaultAssociation.value(QLatin1String("."), QString()).toString();
	}

	if (!defaultApplication.isEmpty())
	{
		associations.append(defaultApplication);
	}

	const QSettings modernAssociations(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.") + suffix + QLatin1String("\\OpenWithProgids"), QSettings::NativeFormat);

	associations.append(modernAssociations.childKeys());
	associations.removeAt(associations.indexOf(QLatin1String(".")));
	associations.removeDuplicates();

	for (int i = 0; i < associations.count(); ++i)
	{
		const QString value(associations.at(i));

		if (m_registrationIdentifier == value)
		{
			continue;
		}

		ApplicationInformation information;
		const QSettings applicationPath(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Classes\\") + value + QLatin1String("\\shell\\open\\command"), QSettings::NativeFormat);

		information.command = applicationPath.value(QLatin1String("."), QString()).toString().remove(QLatin1Char('"'));

		if (information.command.contains(QLatin1String("explorer.exe"), Qt::CaseInsensitive))
		{
			information.command = information.command.left(information.command.indexOf(QLatin1String(".exe"), 0, Qt::CaseInsensitive) + 4);
			information.command += " %1";
		}

		if (information.command.isEmpty())
		{
			Console::addMessage(tr("Failed to load a valid application path for MIME type %1: %2").arg(suffix).arg(value), Console::OtherCategory, Console::ErrorLevel);

			continue;
		}

		getApplicationInformation(information);

		applications.append(information);
	}

	// Win XP applications
	const QSettings legacyAssociations(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.") + suffix + QLatin1String("\\OpenWithList"), QSettings::NativeFormat);
	const QString order(legacyAssociations.value(QLatin1String("MRUList"), QString()).toString());
	const QString applicationFileName(QFileInfo(QCoreApplication::applicationFilePath()).fileName());

	associations = legacyAssociations.childKeys();
	associations.removeAt(associations.indexOf(QLatin1String("MRUList")));

	for (int i = 0; i < associations.count(); ++i)
	{
		ApplicationInformation information;
		const QString value(legacyAssociations.value(order.at(i)).toString());

		if (applicationFileName == value)
		{
			continue;
		}

		const QSettings applicationPath(QLatin1String("HKEY_CURRENT_USER\\SOFTWARE\\Classes\\Applications\\") + value + QLatin1String("\\shell\\open\\command"), QSettings::NativeFormat);

		information.command = applicationPath.value(QLatin1String("."), QString()).toString().remove(QLatin1Char('"'));

		if (information.command.isEmpty())
		{
			const QSettings applicationPath(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Applications\\") + value + QLatin1String("\\shell\\open\\command"), QSettings::NativeFormat);

			information.command = applicationPath.value(QLatin1String("."), QString()).toString().remove(QLatin1Char('"'));

			if (information.command.isEmpty())
			{
				const QSettings applicationPath(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") + value, QSettings::NativeFormat);

				information.command = applicationPath.value(QLatin1String("."), QString()).toString();

				if (!information.command.isEmpty())
				{
					information.command.append(QLatin1String(" %1"));
				}
				else
				{
					Console::addMessage(tr("Failed to load a valid application path for MIME type %1: %2").arg(suffix).arg(value), Console::OtherCategory, Console::ErrorLevel);

					continue;
				}
			}
		}

		getApplicationInformation(information);

		bool exclude(false);

		for (int j = 0; j < applications.count(); ++j)
		{
			if (applications.at(j).name == information.name)
			{
				exclude = true;

				break;
			}
		}

		if (!exclude)
		{
			applications.append(information);
		}
	}

	m_cleanupTimer = startTimer(5 * 60000);

	return applications;
}

QString WindowsPlatformIntegration::getUpdaterBinary() const
{
	return QLatin1String("updater.exe");
}

QString WindowsPlatformIntegration::getPlatformName() const
{
	if (QSysInfo::buildCpuArchitecture() == QLatin1String("x86_64"))
	{
		return QLatin1String("win64");
	}

	return QLatin1String("win32");
}

bool WindowsPlatformIntegration::canShowNotifications() const
{
	return true;
}

bool WindowsPlatformIntegration::setAsDefaultBrowser()
{
	if (!isBrowserRegistered() && !registerToSystem())
	{
		return false;
	}

	QSettings registry(QLatin1String("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);

	for (int i = 0; i < m_registrationPairs.count(); ++i)
	{
		if (m_registrationPairs.at(i).second == ProtocolType)
		{
			registry.setValue(QLatin1String("Classes/") + m_registrationPairs.at(i).first + QLatin1String("/DefaultIcon/."), m_applicationFilePath + QLatin1String(",1"));
			registry.setValue(QLatin1String("Classes/") + m_registrationPairs.at(i).first + QLatin1String("/shell/open/command/."), QLatin1String("\"") + m_applicationFilePath + QLatin1String("\" \"%1\""));
		}
		else
		{
			registry.setValue(QLatin1String("Classes/") + m_registrationPairs.at(i).first + QLatin1String("/."), m_registrationIdentifier);
		}
	}

	registry.setValue(QLatin1String("Clients/StartmenuInternet/."), m_registrationIdentifier);
	registry.sync();

	if (QSysInfo::windowsVersion() >= QSysInfo::WV_10_0)
	{
		DWORD pid = 0;
		IApplicationActivationManager *activationManager = nullptr;
		HRESULT result = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER, IID_IApplicationActivationManager, (LPVOID*)&activationManager);

		if (result == S_OK)
		{
			result = activationManager->ActivateApplication(QString("windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel").toStdWString().c_str(), QString("page=SettingsPageAppsDefaults").toStdWString().c_str(), AO_NONE, &pid);

			activationManager->Release();

			if (result == S_OK)
			{
				return true;
			}
		}

		Console::addMessage(QCoreApplication::translate("main", "Failed to run File Associations Manager, error code: %1\nApplication ID: %2").arg(result).arg(pid), Otter::Console::OtherCategory, Console::ErrorLevel);
	}
	else if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA)
	{
		IApplicationAssociationRegistrationUI *applicationAssociationRegistrationUI = nullptr;
		HRESULT result = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI, nullptr, CLSCTX_INPROC_SERVER, IID_IApplicationAssociationRegistrationUI, (LPVOID*)&applicationAssociationRegistrationUI);

		if (result == S_OK && applicationAssociationRegistrationUI)
		{
			result = applicationAssociationRegistrationUI->LaunchAdvancedAssociationUI(m_registrationIdentifier.toStdWString().c_str());

			applicationAssociationRegistrationUI->Release();

			if (result == S_OK)
			{
				return true;
			}
		}

		Console::addMessage(QCoreApplication::translate("main", "Failed to run File Associations Manager, error code: %1").arg(result), Otter::Console::OtherCategory, Console::ErrorLevel);
	}
	else
	{
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_DWORD | SHCNF_FLUSH, nullptr, nullptr);
		Sleep(1000);
	}

	return true;
}

bool WindowsPlatformIntegration::registerToSystem()
{
	m_applicationRegistration.setValue(m_registrationIdentifier, QLatin1String("Software\\Clients\\StartMenuInternet\\OtterBrowser\\Capabilities"));
	m_applicationRegistration.sync();

	m_propertiesRegistration.setValue(QLatin1String("/."), QLatin1String("Otter Browser Document"));
	m_propertiesRegistration.setValue(QLatin1String("FriendlyTypeName"), QLatin1String("Otter Browser Document"));
	m_propertiesRegistration.setValue(QLatin1String("DefaultIcon/."), m_applicationFilePath + QLatin1String(",1"));
	m_propertiesRegistration.setValue(QLatin1String("EditFlags"), 2);
	m_propertiesRegistration.setValue(QLatin1String("shell/open/ddeexec/."), QString());
	m_propertiesRegistration.setValue(QLatin1String("shell/open/command/."), QLatin1String("\"") + m_applicationFilePath + QLatin1String("\" \"%1\""));
	m_propertiesRegistration.sync();

	QSettings capabilities(QLatin1String("HKEY_CURRENT_USER\\Software\\Clients\\StartMenuInternet\\") + m_registrationIdentifier, QSettings::NativeFormat);
	capabilities.setValue(QLatin1String("./"), QLatin1String("Otter Browser"));
	capabilities.setValue(QLatin1String("Capabilities/ApplicationDescription"), QLatin1String("Web browser controlled by the user, not vice-versa"));
	capabilities.setValue(QLatin1String("Capabilities/ApplicationIcon"), m_applicationFilePath + QLatin1String(",0"));
	capabilities.setValue(QLatin1String("Capabilities/ApplicationName"), QLatin1String("Otter Browser"));

	for (int i = 0; i < m_registrationPairs.count(); ++i)
	{
		if (m_registrationPairs.at(i).second == ProtocolType)
		{
			capabilities.setValue(QLatin1String("Capabilities/UrlAssociations/") + m_registrationPairs.at(i).first, m_registrationIdentifier);
		}
		else
		{
			capabilities.setValue(QLatin1String("Capabilities/FileAssociations/") + m_registrationPairs.at(i).first, m_registrationIdentifier);
		}
	}

	capabilities.setValue(QLatin1String("Capabilities/Startmenu/StartMenuInternet"), m_registrationIdentifier);
	capabilities.setValue(QLatin1String("DefaultIcon/."), m_applicationFilePath + QLatin1String(",0"));
	capabilities.setValue(QLatin1String("InstallInfo/IconsVisible"), 1);
	// TODO ----------------------------------------------------------------
	//capabilities.setValue("InstallInfo/HideIconsCommand", applicationFilePath + " -command");
	//capabilities.setValue("InstallInfo/ReinstallCommand", applicationFilePath + " -command");
	//capabilities.setValue("InstallInfo/ShowIconsCommand", applicationFilePath + " -command");
	// ---------------------------------------------------------------------
	capabilities.setValue(QLatin1String("shell/open/command/."), QLatin1String("\"") + m_applicationFilePath + QLatin1String("\""));
	capabilities.sync();

	if (m_applicationRegistration.status() != QSettings::NoError || capabilities.status() != QSettings::NoError)
	{
		Console::addMessage(QCoreApplication::translate("main", "Failed to register application to system registry: %1, %2").arg(m_applicationRegistration.status(), capabilities.status()), Otter::Console::OtherCategory, Console::ErrorLevel);

		return false;
	}

	return true;
}

bool WindowsPlatformIntegration::isBrowserRegistered() const
{
	if (m_applicationRegistration.value(m_registrationIdentifier).isNull() || !m_propertiesRegistration.value(QLatin1String("/shell/open/command/."), QString()).toString().contains(m_applicationFilePath))
	{
		return false;
	}

	return true;
}

bool WindowsPlatformIntegration::canSetAsDefaultBrowser() const
{
	return (isBrowserRegistered() ? true : m_applicationRegistration.isWritable());
}

bool WindowsPlatformIntegration::isDefaultBrowser() const
{
	bool isDefault(true);

	if (m_applicationRegistration.value(m_registrationIdentifier).isNull())
	{
		return false;
	}

	const QSettings registry(QLatin1String("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);

	for (int i = 0; i < m_registrationPairs.count(); ++i)
	{
		if (m_registrationPairs.at(i).second == ProtocolType)
		{
			isDefault &= (registry.value(QLatin1String("Classes/") + m_registrationPairs.at(i).first + QLatin1String("/shell/open/command/."), QString()).toString().contains(m_applicationFilePath));
		}
		else
		{
			isDefault &= (registry.value(QLatin1String("Classes/") + m_registrationPairs.at(i).first + QLatin1String("/."), QString()).toString() == m_registrationIdentifier);

			if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA)
			{
				isDefault &= (registry.value(QLatin1String("Microsoft/Windows/CurrentVersion/Explorer/FileExts/") + m_registrationPairs.at(i).first + QLatin1String("/UserChoice/Progid")).toString() == m_registrationIdentifier);
			}
		}
	}

	isDefault &= (registry.value(QLatin1String("Clients/StartmenuInternet/."), QString()).toString() == m_registrationIdentifier);

	return isDefault;
}

//bool WindowsPlatformIntegration::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
//{
//	static unsigned int taskBarCreatedId = WM_NULL;
//
//	MSG* recievedMessage = static_cast<MSG*>(message);
//
//	if (taskBarCreatedId == WM_NULL)
//	{
//		taskBarCreatedId = RegisterWindowMessage(QString("TaskbarButtonCreated").toStdWString().c_str());
//
//		return false;
//	}
//
//	if (recievedMessage->message == taskBarCreatedId && recievedMessage->hwnd == (HWND)Application::getInstance()->getWindow()->winId() /*getInstance()->m_parentWidget->winId()*/)
//	{
//		HRESULT result = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList4, (LPVOID*)m_taskbar /*reinterpret_cast<void**> (&(m_taskbar))*/);
//
//		if (result == S_OK)
//		{
//			result = m_taskbar->HrInit();
//
//			if (result != S_OK)
//			{
//				m_taskbar = nullptr;
//			}
//		}
//
//		return true;
//	}
//
//	return false;
//}

}
