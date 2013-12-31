/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2014 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "TabBarDockWidget.h"
#include "TabBarWidget.h"
#include "../core/ActionsManager.h"

#include <QtWidgets/QBoxLayout>

namespace Otter
{

TabBarDockWidget::TabBarDockWidget(QWidget *parent) : QDockWidget(parent),
	m_tabBar(NULL),
	m_newTabButton(new QToolButton(this))
{
}

void TabBarDockWidget::setup(QAction *closedWindowsAction)
{
	QWidget *widget = new QWidget(this);

	m_tabBar = new TabBarWidget(widget);

	QToolButton *trashButton = new QToolButton(widget);
	trashButton->setAutoRaise(true);
	trashButton->setDefaultAction(closedWindowsAction);

	QBoxLayout *tabsLayout = new QBoxLayout(QBoxLayout::LeftToRight, widget);
	tabsLayout->addWidget(m_tabBar);
	tabsLayout->addSpacing(32);
	tabsLayout->addWidget(trashButton);
	tabsLayout->setContentsMargins(0, 0, 0, 0);
	tabsLayout->setSpacing(0);

	widget->setLayout(tabsLayout);

	setTitleBarWidget(NULL);
	setWidget(widget);

	m_newTabButton->setAutoRaise(true);
	m_newTabButton->setDefaultAction(ActionsManager::getAction(QLatin1String("NewTab")));
	m_newTabButton->setFixedSize(32, 32);
	m_newTabButton->show();
	m_newTabButton->raise();
	m_newTabButton->move(m_tabBar->geometry().topRight());

	connect(this, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), m_tabBar, SLOT(setOrientation(Qt::DockWidgetArea)));
	connect(m_tabBar, SIGNAL(moveNewTabButton(int)), this, SLOT(moveNewTabButton(int)));
}

void TabBarDockWidget::moveNewTabButton(int position)
{
	if (m_newTabButton && m_tabBar)
	{
		const bool isHorizontal = (m_tabBar->shape() == QTabBar::RoundedNorth || m_tabBar->shape() == QTabBar::RoundedSouth);

		m_newTabButton->move(isHorizontal ? QPoint((qMin(position, m_tabBar->width()) + widget()->pos().x()), ((m_tabBar->height() - m_newTabButton->height()) / 2)) : QPoint(((m_tabBar->width() - m_newTabButton->width()) / 2), (qMin(position, m_tabBar->height()) + widget()->pos().y())));
	}
}

TabBarWidget* TabBarDockWidget::getTabBar()
{
	return m_tabBar;
}

}
