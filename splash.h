#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <objidl.h>
#include <string>
#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

namespace KvaltikSplash {

static Gdiplus::Image* g_image = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static int g_progress = 0;
static std::wstring g_step = L"Spouštím Kvaltík TSC Hub...";

inline std::wstring StepForProgress(int p) {
    if (p < 18) return L"Inicializace aplikace...";
    if (p < 36) return L"Hledám Train Simulator Classic...";
    if (p < 54) return L"Načítám RailDriver64.dll...";
    if (p < 72) return L"Načítám VO79, NavTrain a RailControl...";
    if (p < 88) return L"Načítám jízdní řád a TSC Connector...";
    if (p < 98) return L"Kontroluji moduly...";
    return L"Hotovo.";
}

inline Gdiplus::Image* LoadImageFromResource(HINSTANCE instance) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_SPLASH_JPG), RT_RCDATA);
    if (!resource) return nullptr;

    HGLOBAL loaded = LoadResource(instance, resource);
    if (!loaded) return nullptr;

    DWORD size = SizeofResource(instance, resource);
    const void* bytes = LockResource(loaded);
    if (!bytes || size == 0) return nullptr;

    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!copy) return nullptr;

    void* destination = GlobalLock(copy);
    if (!destination) {
        GlobalFree(copy);
        return nullptr;
    }

    CopyMemory(destination, bytes, size);
    GlobalUnlock(copy);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(copy, TRUE, &stream) != S_OK) {
        GlobalFree(copy);
        return nullptr;
    }

    auto* image = Gdiplus::Image::FromStream(stream);
    stream->Release();

    if (!image || image->GetLastStatus() != Gdiplus::Ok) {
        delete image;
        return nullptr;
    }

    return image;
}

inline void DrawSplash(HWND hwnd, HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    Gdiplus::Graphics graphics(hdc);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    if (g_image) {
        graphics.DrawImage(g_image, 0, 0, width, height);
    } else {
        Gdiplus::SolidBrush background(Gdiplus::Color(255, 7, 16, 28));
        graphics.FillRectangle(&background, 0, 0, width, height);
    }

    const int panelH = ((96 > height / 7) ? 96 : height / 7);
    const int panelY = height - panelH;
    Gdiplus::SolidBrush shade(Gdiplus::Color(178, 3, 10, 20));
    graphics.FillRectangle(&shade, 0, panelY, width, panelH);

    const int margin = ((28 > width / 28) ? 28 : width / 28);
    const int barY = panelY + 48;
    const int barH = ((8 > height / 80) ? 8 : height / 80);
    const int barW = width - margin * 2;

    Gdiplus::SolidBrush barBack(Gdiplus::Color(220, 25, 40, 57));
    graphics.FillRectangle(&barBack, margin, barY, barW, barH);

    int fill = (barW * g_progress) / 100;
    Gdiplus::LinearGradientBrush bar(
        Gdiplus::Point(margin, barY),
        Gdiplus::Point(margin + ((fill > 1) ? fill : 1), barY),
        Gdiplus::Color(255, 0, 153, 255),
        Gdiplus::Color(255, 86, 210, 255)
    );
    if (fill > 0) graphics.FillRectangle(&bar, margin, barY, fill, barH);

    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family, ((16.0f > height / 39.0f) ? 16.0f : height / 39.0f), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font footerFont(&family, ((13.0f > height / 52.0f) ? 13.0f : height / 52.0f), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush white(Gdiplus::Color(245, 245, 250, 255));
    Gdiplus::SolidBrush pale(Gdiplus::Color(220, 174, 202, 228));

    graphics.DrawString(g_step.c_str(), -1, &font, Gdiplus::PointF((Gdiplus::REAL)margin, (Gdiplus::REAL)(panelY + 14)), &white);

    std::wstring percent = std::to_wstring(g_progress) + L" %";
    Gdiplus::RectF bounds;
    graphics.MeasureString(percent.c_str(), -1, &font, Gdiplus::PointF(0,0), &bounds);
    graphics.DrawString(percent.c_str(), -1, &font,
        Gdiplus::PointF((Gdiplus::REAL)(width - margin - bounds.Width), (Gdiplus::REAL)(panelY + 14)), &white);

    graphics.DrawString(L"Kvaltík TSC Hub • všechno na jednom místě", -1, &footerFont,
        Gdiplus::PointF((Gdiplus::REAL)margin, (Gdiplus::REAL)(barY + barH + 13)), &pale);
}

inline LRESULT CALLBACK SplashProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawSplash(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER:
            if (wParam == 1) {
                g_progress += 2;
                if (g_progress > 100) g_progress = 100;
                g_step = StepForProgress(g_progress);
                InvalidateRect(hwnd, nullptr, FALSE);
                if (g_progress >= 100) {
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 2, 220, nullptr);
                }
            } else if (wParam == 2) {
                KillTimer(hwnd, 2);
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

inline void Show(HINSTANCE instance) {
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) != Gdiplus::Ok) return;

    g_image = LoadImageFromResource(instance);
    g_progress = 0;
    g_step = L"Spouštím Kvaltík TSC Hub...";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"KvaltikTSCHubSplash";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int maxW = work.right - work.left - 80;
    int maxH = work.bottom - work.top - 80;

    int width = ((1280 < maxW) ? 1280 : maxW);
    int height = width * 9 / 16;
    if (height > maxH) {
        height = maxH;
        width = height * 16 / 9;
    }

    int x = work.left + ((work.right - work.left) - width) / 2;
    int y = work.top + ((work.bottom - work.top) - height) / 2;

    HWND splash = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Kvaltík TSC Hub",
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, instance, nullptr
    );

    if (splash) {
        ShowWindow(splash, SW_SHOW);
        UpdateWindow(splash);
        SetTimer(splash, 1, 28, nullptr);

        MSG msg{};
        while (IsWindow(splash)) {
            if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    delete g_image;
    g_image = nullptr;
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

} // namespace KvaltikSplash
