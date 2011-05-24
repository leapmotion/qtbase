/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtGui>
class StaticWidget : public QWidget
{
Q_OBJECT
public:
    int hue;
    bool pressed;
    StaticWidget(QWidget *parent = 0)
    :QWidget(parent)
    {
        setAttribute(Qt::WA_StaticContents);
        setAttribute(Qt::WA_OpaquePaintEvent);
           hue = 200;
        pressed = false;
    }

    // Update 4 rects in a checkerboard pattern, using either
    // a QRegion or separate rects (see the useRegion switch)
    void updatePattern(QPoint pos)
    {
        const int rectSize = 10;
        QRect rect(pos.x() - rectSize, pos.y() - rectSize, rectSize *2, rectSize * 2);

        QVector<QRect> updateRects;
        updateRects.append(rect.translated(rectSize * 2, rectSize * 2));
        updateRects.append(rect.translated(rectSize * 2, -rectSize * 2));
        updateRects.append(rect.translated(-rectSize * 2, rectSize * 2));
        updateRects.append(rect.translated(-rectSize * 2, -rectSize * 2));


        bool useRegion = false;
        if (useRegion) {
            QRegion region;
            region.setRects(updateRects.data(), 4);
            update(region);
        } else {
            foreach (QRect rect, updateRects)
                update(rect);
        }
    }


    void resizeEvent(QResizeEvent *)
    {
  //      qDebug() << "static widget resize from" << e->oldSize() << "to" << e->size();
    }

    void mousePressEvent(QMouseEvent *event)
    {
//        qDebug() << "mousePress at" << event->pos();
        pressed = true;
        updatePattern(event->pos());
    }

    void mouseReleaseEvent(QMouseEvent *)
    {
        pressed = false;
    }

    void mouseMoveEvent(QMouseEvent *event)
    {
        if (pressed)
            updatePattern(event->pos());
    }

    void paintEvent(QPaintEvent *e)
    {
        QPainter p(this);
        static int color = 200;
        color = (color + 41) % 205 + 50;
//        color = ((color + 45) %150) + 100;
        qDebug() << "static widget repaint" << e->rect();
        if (pressed)
            p.fillRect(e->rect(), QColor::fromHsv(100, 255, color));
        else
            p.fillRect(e->rect(), QColor::fromHsv(hue, 255, color));
        p.setPen(QPen(QColor(Qt::white)));

        for (int y = e->rect().top(); y <= e->rect().bottom() + 1; ++y) {
            if (y % 20 == 0)
                p.drawLine(e->rect().left(), y, e->rect().right(), y);
        }

        for (int x = e->rect().left(); x <= e->rect().right() +1 ; ++x) {
            if (x % 20 == 0)
                p.drawLine(x, e->rect().top(), x, e->rect().bottom());
        }
    }
};
