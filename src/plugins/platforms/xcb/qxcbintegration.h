/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QXCBINTEGRATION_H
#define QXCBINTEGRATION_H

#include <QtGui/QPlatformIntegration>
#include <QtGui/QPlatformScreen>

QT_BEGIN_NAMESPACE

class QXcbConnection;

class QXcbIntegration : public QPlatformIntegration
{
public:
    QXcbIntegration();
    ~QXcbIntegration();

    bool hasCapability(Capability cap) const;
    QPixmapData *createPixmapData(QPixmapData::PixelType type) const;
    QPlatformWindow *createPlatformWindow(QWindow *window) const;
    QWindowSurface *createWindowSurface(QWindow *window, WId winId) const;

    QList<QPlatformScreen *> screens() const;
    void moveToScreen(QWidget *window, int screen);
    bool isVirtualDesktop();
    QPixmap grabWindow(WId window, int x, int y, int width, int height) const;

    QPlatformFontDatabase *fontDatabase() const;

    QPlatformNativeInterface *nativeInterface()const;

private:
    bool hasOpenGL() const;
    QList<QPlatformScreen *> m_screens;
    QXcbConnection *m_connection;

    QPlatformFontDatabase *m_fontDatabase;
    QPlatformNativeInterface *m_nativeInterface;
};

QT_END_NAMESPACE

#endif
