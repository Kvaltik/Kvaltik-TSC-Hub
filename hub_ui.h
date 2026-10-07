#pragma once
#include <algorithm>

namespace HubUi {
constexpr COLORREF Background = RGB(16, 22, 29);
constexpr COLORREF Surface = RGB(25, 34, 44);
constexpr COLORREF Border = RGB(43, 57, 70);
constexpr COLORREF Text = RGB(233, 240, 245);
constexpr COLORREF Muted = RGB(155, 173, 189);
constexpr COLORREF Accent = RGB(77, 215, 190);
static HBRUSH backgroundBrush = CreateSolidBrush(Background);
static HBRUSH surfaceBrush = CreateSolidBrush(Surface);
static int scroll = 0;
struct Item { HWND window; int x, y, width, height; };
static std::vector<Item> items;
struct Card { const wchar_t* title; const wchar_t* subtitle; int id; };
static const Card cards[] = {
    {L"NavTrain", L"Asistent strojvedoucího", 2001},
    {L"VO79", L"Palubní radiostanice", 2002},
    {L"Jízdní řád", L"Trasa, stanice a časy", 2005},
    {L"Driver Display", L"Přehled na druhém monitoru", 2014},
    {L"Kniha jízd", L"Historie a statistiky", 2008},
    {L"Scenario Creator", L"Tvorba scénářů", 2004},
    {L"Consist Manager", L"Správa vlakových souprav", 2006},
    {L"Route Manager", L"Tratě a jejich závislosti", 2011},
    {L"Scenario Doctor", L"Kontrola scénářů a assetů", 2007},
    {L"Shader Manager", L"Vzhled a ReShade profily", 2010},
    {L"RailControl", L"Dispečerské pracoviště", 2003},
    {L"Live Map", L"Poloha a provoz na trati", 2009},
    {L"Rozkazovač", L"České vlakové rozkazy", 2012},
    {L"Výpravčí", L"Odjezdové signály a události", 2013},
    {L"TSC Connector", L"Diagnostika RailDriver", 2015}
};
inline void Track(HWND window, int x, int y, int w, int h) { items.push_back({window,x,y,w,h}); }
inline void EnsureVisible(HWND window) {
    RECT rect{}, client{};
    GetWindowRect(window, &rect);
    MapWindowPoints(nullptr, GetParent(window), reinterpret_cast<POINT*>(&rect), 2);
    GetClientRect(GetParent(window), &client);
    if (rect.top < 0 || rect.bottom > client.bottom) {
        int delta = rect.top < 0 ? rect.top - 8 : rect.bottom - client.bottom + 8;
        SetScrollPos(GetParent(window), SB_VERT, scroll + delta, TRUE);
        SendMessageW(GetParent(window), WM_VSCROLL, SB_THUMBPOSITION, 0);
    }
}
inline LRESULT CALLBACK ButtonProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEMOVE && !GetPropW(window, L"hover")) {
        SetPropW(window, L"hover", reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (msg == WM_MOUSELEAVE) { RemovePropW(window,L"hover"); InvalidateRect(window,nullptr,FALSE); }
    if (msg == WM_SETFOCUS) EnsureVisible(window);
    if (msg == WM_NCDESTROY) { RemovePropW(window,L"hover"); RemoveWindowSubclass(window,ButtonProc,1); }
    return DefSubclassProc(window,msg,wp,lp);
}
inline void TextAt(HDC dc, const wchar_t* text, RECT rect, HFONT font, COLORREF color, UINT flags=DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS) {
    auto old = SelectObject(dc,font);
    SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,text,-1,&rect,flags);
    SelectObject(dc,old);
}
inline void DrawButton(const DRAWITEMSTRUCT& item) {
    HDC dc=item.hDC; RECT r=item.rcItem;
    const bool primary=item.CtlID==1001;
    const bool hover=GetPropW(item.hwndItem,L"hover")!=nullptr;
    const bool pressed=(item.itemState&ODS_SELECTED)!=0;
    const bool disabled=(item.itemState&ODS_DISABLED)!=0;
    COLORREF fill=primary ? Accent : (hover ? RGB(35,49,61) : Surface);
    if(pressed) fill=primary ? RGB(51,178,157) : RGB(43,62,76);
    FillRect(dc,&r,backgroundBrush);
    HBRUSH brush=CreateSolidBrush(fill); HPEN pen=CreatePen(PS_SOLID,1,hover ? Accent : Border);
    auto oldBrush=SelectObject(dc,brush); auto oldPen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right-1,r.bottom-1,12,12);
    SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(brush); DeleteObject(pen);
    const Card* card=nullptr;
    for(const auto& c:cards) if(c.id==static_cast<int>(item.CtlID)) card=&c;
    if(card) {
        RECT title{r.left+18,r.top+10,r.right-22,r.top+34};
        TextAt(dc,card->title,title,g_bold,disabled ? Muted : Text);
        RECT sub{r.left+18,r.top+35,r.right-18,r.bottom-8};
        TextAt(dc,card->subtitle,sub,g_small,Muted);
    } else {
        wchar_t title[128]{}; GetWindowTextW(item.hwndItem,title,128);
        RECT text=r; InflateRect(&text,-12,0);
        TextAt(dc,title,text,g_bold,disabled ? Muted : (primary ? Background : Text),DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    }
    if((item.itemState&ODS_FOCUS) && !(item.itemState&ODS_NOFOCUSRECT)) {
        RECT focus=r; InflateRect(&focus,-5,-5); SetTextColor(dc,Text); DrawFocusRect(dc,&focus);
    }
}
inline void Layout(HWND window) {
    RECT client{}; GetClientRect(window,&client);
    const int extra=(std::max)(0,static_cast<int>(client.right)-1060);
    SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS};
    info.nMin=0; info.nMax=831; info.nPage=client.bottom; info.nPos=scroll;
    SetScrollInfo(window,SB_VERT,&info,TRUE);
    scroll=GetScrollPos(window,SB_VERT);
    for(const auto& item:items) {
        int x=item.x, w=item.width;
        if(x>=730) x+=extra;
        else if(x>=386) x+=extra/2;
        if(item.width>=990) w+=extra;
        else if(item.width==320) w+=extra/3;
        // Columns expand evenly while retaining their 20px gutters.
        if(item.y>=260 && item.y<=620) {
            int col=(item.x-36)/340;
            x=item.x+col*extra/3;
            if(item.width==320) w=item.width+extra/3;
        }
        MoveWindow(item.window,x,item.y-scroll,w,item.height,TRUE);
    }
    InvalidateRect(window,nullptr,TRUE);
}
inline void Scroll(HWND window,int position) { scroll=position; Layout(window); }
inline void Paint(HWND window) {
    PAINTSTRUCT ps{}; HDC dc=BeginPaint(window,&ps);
    RECT client{}; GetClientRect(window,&client); FillRect(dc,&client,backgroundBrush);
    RECT line{36,112-scroll,client.right-36,113-scroll}; FillRect(dc,&line,surfaceBrush);
    RECT accent{36,28-scroll,40,76-scroll}; HBRUSH brush=CreateSolidBrush(Accent); FillRect(dc,&accent,brush); DeleteObject(brush);
    EndPaint(window,&ps);
}
inline void Cleanup() { DeleteObject(backgroundBrush); DeleteObject(surfaceBrush); }
}

void Font(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); }
HWND Label(HWND parent,const wchar_t* text,int x,int y,int w,int h,HFONT f,DWORD style=SS_LEFT) {
    HWND c=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_NOPREFIX|style,x,y,w,h,parent,nullptr,g_inst,nullptr);
    Font(c,f); HubUi::Track(c,x,y,w,h); return c;
}
HWND Btn(HWND parent,const wchar_t* text,int id,int x,int y,int w,int h) {
    HWND c=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),g_inst,nullptr);
    Font(c,g_bold); SetWindowSubclass(c,HubUi::ButtonProc,1,0); HubUi::Track(c,x,y,w,h); return c;
}
void BuildUi(HWND hwnd) {
    Label(hwnd,L"Kvaltík",54,23,280,42,g_title);
    Label(hwnd,L"TSC HUB  /  TVOJE ŽELEZNIČNÍ PRACOVIŠTĚ",56,70,500,22,g_small);
    Btn(hwnd,L"Najít TSC",1003,600,35,128,42);
    Btn(hwnd,L"Aktualizace",1004,740,35,128,42);
    Btn(hwnd,L"Spustit TSC  →",1001,880,35,144,42);
    Label(hwnd,L"PŘIPOJENÍ",36,134,260,20,g_small);
    g_tsc=Label(hwnd,L"TSC: —",36,161,220,28,g_bold);
    g_dll=Label(hwnd,L"RailDriver: —",278,161,300,28,g_bold);
    g_loco=Label(hwnd,L"Lokomotiva: —",36,198,525,24,g_small,SS_ENDELLIPSIS);
    g_speed=Label(hwnd,L"Rychlost: —",596,198,235,24,g_small);
    Btn(hwnd,L"Obnovit stav",1002,880,160,144,38);
    Label(hwnd,L"01  /  ŘÍZENÍ JÍZDY",36,260,320,25,g_bold);
    Label(hwnd,L"02  /  TVORBA A SPRÁVA",376,260,320,25,g_bold);
    Label(hwnd,L"03  /  PROVOZ A DIAGNOSTIKA",716,260,320,25,g_bold);
    for(int col=0;col<3;++col) {
        Label(hwnd,L"Připravujeme · kliknutím zobrazíš popis",36+340*col,289,320,22,g_small);
        for(int row=0;row<5;++row) {
            const auto& c=HubUi::cards[col*5+row];
            Btn(hwnd,c.title,c.id,36+340*col,324+row*64,320,56);
        }
    }
    Label(hwnd,L"SLOŽKY HRY",36,663,150,22,g_small);
    const wchar_t* folders[]={L"RailWorks",L"Assets",L"Content",L"Routes",L"Plugins"};
    for(int i=0;i<5;++i) Btn(hwnd,folders[i],3001+i,200+i*164,652,152,36);
    Label(hwnd,L"POSLEDNÍ UDÁLOSTI",36,716,320,22,g_small);
    Label(hwnd,(L"Verze "+std::wstring(KvaltikUpdater::CURRENT_VERSION)).c_str(),880,716,144,22,g_small,SS_RIGHT);
    g_log=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,
        36,746,990,62,hwnd,nullptr,g_inst,nullptr);
    Font(g_log,g_small); HubUi::Track(g_log,36,746,990,62);
    SendMessageW(g_log,EM_SETLIMITTEXT,64000,0);
}
