/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#ifndef OTTER_WINDOWSPLATFORMINTEGRATION_H
#define OTTER_WINDOWSPLATFORMINTEGRATION_H

#include "../../../core/PlatformIntegration.h"

#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QSettings>
#include <QtWinExtras/QtWin>
#include <QtWinExtras/QWinTaskbarButton>
#include <QtWinExtras/QWinTaskbarProgress>

#ifndef NOMINMAX // VC++ fix - http://qt-project.org/forums/viewthread/22133
#define NOMINMAX
#endif

#include <shlobj.h> // has to be after NOMINMAX!

namespace Otter
{

// Support for versions of shlobj.h that don't include the Vista+ associations API's (MinGW in Qt?)
#if !defined(IApplicationAssociationRegistrationUI)
MIDL_INTERFACE("1f76a169-f994-40ac-8fc8-0959e8874710")
IApplicationAssociationRegistrationUI : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE LaunchAdvancedAssociationUI(
		/* [string][in] */ __RPC__in_string LPCWSTR pszAppRegistryName) = 0;

};

const GUID IID_IApplicationAssociationRegistrationUI = {0x1f76a169,0xf994,0x40ac, {0x8f,0xc8,0x09,0x59,0xe8,0x87,0x47,0x10}};
#endif

#if !defined(IApplicationActivationManager)
enum ACTIVATEOPTIONS
{
	AO_NONE = 0,
	AO_DESIGNMODE = 1,
	AO_NOERRORUI = 2,
	AO_NOSPLASHSCREEN = 4
};

extern "C"
{
	typedef HRESULT(WINAPI *t_DwmInvalidateIconicBitmaps)(HWND hwnd);
	typedef HRESULT(WINAPI *t_DwmSetIconicThumbnail)(HWND hwnd, HBITMAP hbmp, DWORD dwSITFlags);
	typedef HRESULT(WINAPI *t_DwmSetIconicLivePreviewBitmap)(HWND hwnd, HBITMAP hbmp, POINT *pptClient, DWORD dwSITFlags);
	typedef HRESULT(WINAPI *t_DwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
}

MIDL_INTERFACE("2e941141-7f97-4756-ba1d-9decde894a3d")
IApplicationActivationManager : public IUnknown
{
public:

	virtual HRESULT STDMETHODCALLTYPE ActivateApplication(
		/* [in] */ __RPC__in LPCWSTR appUserModelId,
		/* [unique][in] */ __RPC__in_opt LPCWSTR arguments,
		/* [in] */ ACTIVATEOPTIONS options,
		/* [out] */ __RPC__out DWORD *processId) = 0;
};

const GUID IID_IApplicationActivationManager = {0x2e941141,0x7f97,0x4756, {0xba,0x1d,0x9d,0xec,0xde,0x89,0x4a,0x3d}};
const CLSID CLSID_ApplicationActivationManager = {0x45BA127D,0x10A8,0x46EA, {0x8A,0xB7,0x56,0xEA,0x90,0x78,0x94,0x3C}};
#endif

enum RegistrationType
{
	ExtensionType = 0,
	ProtocolType
};

class MainWindow;
class WindowsPlatformIntegration;

class WindowsNativeEventFilter : public QObject, public QAbstractNativeEventFilter
{
	Q_OBJECT

public:
	explicit WindowsNativeEventFilter(QObject *parent = nullptr) : QObject(parent) {};
	bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

protected:
	WindowsPlatformIntegration* getIntegration();
};

class WindowsPlatformIntegration : public PlatformIntegration
{
	Q_OBJECT

public:
	explicit WindowsPlatformIntegration(Application *parent);

	struct TaskbarTab
	{
		TaskbarTab() : widget(nullptr), tabWidget(nullptr) {}

		QPixmap thumbnail;
		QWidget* widget;
		QWidget* tabWidget;
		Window* window;
	};

	void createTaskBar();
	void runApplication(const QString &command, const QUrl &url = {}) const override;
	void startLinkDrag(const QUrl &url, const QString &title, const QPixmap &pixmap, QObject *parent = nullptr) const override;
	void setThumbnail(HWND hwnd, QSize size = QSize(), bool window = false);
	void tabClick(HWND hwnd);
	void tabClose(HWND hwnd);
	Style* createStyle(const QString &name) const override;
	QMap<HWND,TaskbarTab*> getTaskbarTabList();
	QVector<ApplicationInformation> getApplicationsForMimeType(const QMimeType &mimeType) override;
	QString getPlatformName() const override;
	bool canShowNotifications() const override;
	bool canSetAsDefaultBrowser() const override;
	bool isDefaultBrowser() const override;

public slots:
	void addTabThumbnail(quint64 windowIdentifier) override;
	void showNotification(Notification *notification) override;
	bool setAsDefaultBrowser() override;

protected:
	void timerEvent(QTimerEvent *event);
	void enableWidgetIconicPreview(QWidget* widget, BOOL enable = TRUE);
	void getApplicationInformation(ApplicationInformation &information);
	void setWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
	
	Window* findWindow(quint64 identifier);
	QWidget* findTab(HWND hwnd);
	HMODULE getDWM();
	QString getUpdaterBinary() const override;
	bool registerToSystem();
	bool isBrowserRegistered() const;
	

protected slots:
	void removeTabThumbnail(quint64 windowIdentifier) override;
	void registerWindow(MainWindow *window);
	void removeWindow(MainWindow *window);
	void updateTaskbarButtons();

private:
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

	QString m_registrationIdentifier;
	QString m_applicationFilePath;
	QSettings m_applicationRegistration;
	QSettings m_propertiesRegistration;
	QVector<QPair<QString, RegistrationType> > m_registrationPairs;
	QHash<MainWindow*, QWinTaskbarButton*> m_taskbarButtons;
	QMap<HWND,TaskbarTab*> m_tabs;
	WindowsNativeEventFilter m_eventFilter;
	ITaskbarList4* m_taskbar;
	int m_cleanupTimer;

	static QProcessEnvironment m_environment;

signals:
	void thumbnailsInitialized();
};

}

#endif
