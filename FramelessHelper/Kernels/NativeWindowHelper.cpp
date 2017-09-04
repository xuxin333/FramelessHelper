#include "NativeWindowHelper.h"
#include "NativeWindowHelper_p.h"

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

#include <QScreen>
#include <QEvent>
#include <QtWin>

#include "NativeWindowFilter.h"
#include "NativeWindowTester.h"

// class NativeWindowHelper

NativeWindowHelper::NativeWindowHelper(QWindow *window, NativeWindowTester *tester)
    : QObject(window)
    , d_ptr(new NativeWindowHelperPrivate())
{
    d_ptr->q_ptr = this;

    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(window);
    d->window = window;

    Q_CHECK_PTR(tester);
    d->tester = tester;

    if (d->window) {
        if (d->window->flags() & Qt::FramelessWindowHint) {
            d->window->installEventFilter(this);
            d->updateWindowStyle();
        }
    }
}

NativeWindowHelper::NativeWindowHelper(QWindow *window)
    : QObject(window)
    , d_ptr(new NativeWindowHelperPrivate())
{
    d_ptr->q_ptr = this;

    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(window);
    d->window = window;

    if (d->window) {
        if (d->window->flags() & Qt::FramelessWindowHint) {
            d->window->installEventFilter(this);
            d->updateWindowStyle();
        }
    }
}

NativeWindowHelper::~NativeWindowHelper()
{
}

bool NativeWindowHelper::nativeEventFilter(void *msg, long *result)
{
    Q_D(NativeWindowHelper);

    Q_CHECK_PTR(d->window);

    LPMSG lpMsg = reinterpret_cast<LPMSG>(msg);
    WPARAM wParam = lpMsg->wParam;
    LPARAM lParam = lpMsg->lParam;

    if (WM_NCHITTEST == lpMsg->message) {
        *result = d->hitTest(GET_X_LPARAM(lParam),
                             GET_Y_LPARAM(lParam));
        return true;
    } else if (WM_NCACTIVATE == lpMsg->message) {
        if (!QtWin::isCompositionEnabled()) {
            *result = 1;
            return true;
        }
    } else if (WM_NCCALCSIZE == lpMsg->message) {
        if (TRUE == wParam) {
            if (d->isMaximized()) {
                QMargins maximizedMargins
                        = d->maximizedMargins();

                QScreen *screen = d->window->screen();
                QRect g = screen->availableGeometry();

                NCCALCSIZE_PARAMS &params = *reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);

                params.rgrc[0].top = g.top() - maximizedMargins.top();
                params.rgrc[0].left = g.left() - maximizedMargins.left();
                params.rgrc[0].right = g.right() + maximizedMargins.right() + 1;
                params.rgrc[0].bottom = g.bottom() + maximizedMargins.bottom() + 1;
            }

            *result = 0;
            return true;
        }
    } else if (WM_GETMINMAXINFO == lpMsg->message) {
        QMargins maximizedMargins
                = d->maximizedMargins();

        QScreen *screen = d->window->screen();
        QRect g = screen->availableGeometry();

        int top = g.top() - maximizedMargins.top();
        int left = g.left() - maximizedMargins.left();
        int right = g.right() + maximizedMargins.right();
        int bottom = g.bottom() + maximizedMargins.bottom();

        LPMINMAXINFO lpMMInfo = reinterpret_cast<LPMINMAXINFO>(lParam);

        lpMMInfo->ptMaxPosition.x = left;
        lpMMInfo->ptMaxPosition.y =  top;
        lpMMInfo->ptMaxSize.x = right - left + 1;
        lpMMInfo->ptMaxSize.y = bottom - top + 1;
        lpMMInfo->ptMaxTrackSize.x = right - left + 1;
        lpMMInfo->ptMaxTrackSize.y = bottom - top + 1;

        *result = 0;
        return true;
    } else if (WM_NCLBUTTONDBLCLK == lpMsg->message) {
        auto minimumSize = d->window->minimumSize();
        auto maximumSize = d->window->maximumSize();
        if ((minimumSize.width() >= maximumSize.width())
                || (minimumSize.height() >= maximumSize.height())) {
            *result = 0;
            return true;
        }
    }

    return false;
}

bool NativeWindowHelper::eventFilter(QObject *obj, QEvent *ev)
{
    Q_D(NativeWindowHelper);

    if (ev->type() == QEvent::Resize) {
        if (d->isMaximized()) {
            QMargins maximizedMargins
                    = d->maximizedMargins();

            QScreen *screen = d->window->screen();
            QRect g = screen->availableGeometry();

            int top = g.top() - maximizedMargins.top();
            int left = g.left() - maximizedMargins.left();
            int right = g.right() + maximizedMargins.right();
            int bottom = g.bottom() + maximizedMargins.bottom();

            d->window->setGeometry(left, top, right - left + 1, bottom - top + 1);
        }
    } else if (ev->type() == QEvent::WinIdChange) {
        d->updateWindowStyle();
    }

    return QObject::eventFilter(obj, ev);
}

// class NativeWindowHelperPrivate

NativeWindowHelperPrivate::NativeWindowHelperPrivate()
    : q_ptr(Q_NULLPTR)
    , window(Q_NULLPTR)
    , tester(Q_NULLPTR)
{
}

NativeWindowHelperPrivate::~NativeWindowHelperPrivate()
{
    if (window)
        NativeWindowFilter::deliver(window, Q_NULLPTR);
}

void NativeWindowHelperPrivate::updateWindowStyle()
{
    Q_Q(NativeWindowHelper);

    Q_CHECK_PTR(window);

    HWND hWnd = reinterpret_cast<HWND>(window->winId());
    if (NULL == hWnd)
        return;

    NativeWindowFilter::deliver(window, q);

    LONG oldStyle = WS_OVERLAPPEDWINDOW | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;;
    LONG newStyle = WS_POPUP            | WS_THICKFRAME;

    if (QtWin::isCompositionEnabled())
        newStyle |= WS_CAPTION;

    if (window->flags() & Qt::CustomizeWindowHint) {
        if (window->flags() & Qt::WindowSystemMenuHint)
            newStyle |= WS_SYSMENU;
        if (window->flags() & Qt::WindowMinimizeButtonHint)
            newStyle |= WS_MINIMIZEBOX;
        if (window->flags() & Qt::WindowMaximizeButtonHint)
            newStyle |= WS_MAXIMIZEBOX;
    } else {
        newStyle |= WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    }

    LONG currentStyle = GetWindowLong(hWnd, GWL_STYLE);
    SetWindowLong(hWnd, GWL_STYLE, (currentStyle & ~oldStyle) | newStyle);

    SetWindowPos(hWnd, NULL, 0, 0, 0 , 0,
                 SWP_NOOWNERZORDER | SWP_NOZORDER |
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    if (QtWin::isCompositionEnabled())
        QtWin::extendFrameIntoClientArea(window, 1, 1, 1, 1);
}

int NativeWindowHelperPrivate::hitTest(int x, int y) const
{
    Q_CHECK_PTR(window);

    enum RegionMask {
        Client = 0x0000,
        Top    = 0x0001,
        Left   = 0x0010,
        Right  = 0x0100,
        Bottom = 0x1000,
    };

    auto wfg = window->frameGeometry();
    QMargins draggableMargins
            = this->draggableMargins();

    int top = draggableMargins.top();
    int left = draggableMargins.left();
    int right = draggableMargins.right();
    int bottom = draggableMargins.bottom();

    if (top <= 0)
        top = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(0x5C); /* SM_CXPADDEDBORDER */
    if (left <= 0)
        left = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(0x5C); /* SM_CXPADDEDBORDER */
    if (right <= 0)
        right = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(0x5C); /* SM_CXPADDEDBORDER */
    if (bottom <= 0)
        bottom = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(0x5C); /* SM_CXPADDEDBORDER */

    auto result =
            (Top *    (y < (wfg.top() +    top))) |
            (Left *   (x < (wfg.left() +   left))) |
            (Right *  (x > (wfg.right() -  right))) |
            (Bottom * (y > (wfg.bottom() - bottom)));

    bool wResizable = window->minimumWidth() < window->maximumWidth();
    bool hResizable = window->minimumHeight() < window->maximumHeight();

    switch (result) {
    case Top | Left    : return wResizable && hResizable ? HTTOPLEFT     : result;
    case Top           : return hResizable               ? HTTOP         : result;
    case Top | Right   : return wResizable && hResizable ? HTTOPRIGHT    : result;
    case Right         : return wResizable               ? HTRIGHT       : result;
    case Bottom | Right: return wResizable && hResizable ? HTBOTTOMRIGHT : result;
    case Bottom        : return hResizable               ? HTBOTTOM      : result;
    case Bottom | Left : return wResizable && hResizable ? HTBOTTOMLEFT  : result;
    case Left          : return wResizable               ? HTLEFT        : result;
    }

    auto pos = window->mapFromGlobal(QPoint(x, y));
    return ((Q_NULLPTR != tester) && !tester->hitTest(pos)) ? HTCLIENT : HTCAPTION;
}

bool NativeWindowHelperPrivate::isMaximized() const
{
    Q_CHECK_PTR(window);

    HWND hWnd = reinterpret_cast<HWND>(window->winId());
    if (NULL == hWnd)
        return false;

    WINDOWPLACEMENT windowPlacement;
    if (!GetWindowPlacement(hWnd, &windowPlacement))
        return false;

    return (SW_MAXIMIZE == windowPlacement.showCmd);
}

QMargins NativeWindowHelperPrivate::draggableMargins() const
{
    return tester ? tester->draggableMargins() : QMargins();
}

QMargins NativeWindowHelperPrivate::maximizedMargins() const
{
    return tester ? tester->maximizedMargins() : QMargins();
}