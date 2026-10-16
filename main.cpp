#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <iomanip>
#include "splash.h"
#include "src/Core/Detection.h"
#include "src/Core/Connector.h"
#include "src/Core/Updater.h"
#include "src/Train/Journeys.h"
#include "src/Radio/Diagnostics.h"
#include "src/Operations/Documents.h"
#include "src/Tools/Inventory.h"
#include "src/App/Modules.h"
#include "nav_route.h"
#pragma comment(lib,"comdlg32.lib")

namespace App {
constexpr UINT UpdateDone=WM_APP+1;
HINSTANCE instance{};
HWND window{},menu{},title{},description{},content{},input{},notice{},remember{},buttons[4]{};
HFONT font{},heading{};
HBRUSH background=CreateSolidBrush(RGB(12,19,30)),surface=CreateSolidBrush(RGB(24,35,49));
std::unique_ptr<Core::Settings> settings;
Core::Connector connector;
std::thread worker;
std::wstring updateMessage=L"Kontrola aktualizací zatím neproběhla.";
std::string latest;
int page=-1;
bool signedIn=false,loading=false,busy=false,installing=false;
std::vector<Navigation::Stop> stops;
size_t stopIndex=0;
std::wstring routePath;

std::wstring Text(HWND control){int n=GetWindowTextLengthW(control);std::wstring s(n+1,L'\0');GetWindowTextW(control,s.data(),n+1);s.resize(n);return s;}
void Set(HWND control,const std::wstring& s){if(Text(control)!=s)SetWindowTextW(control,s.c_str());}
void Status(const std::wstring& text){Set(notice,text);}
void Log(const std::wstring& text){try{auto path=settings->directory/L"hub.log";auto log=Core::Read(path);if(log.size()>256000)log=log.substr(log.size()-128000);Core::Write(path,log+Train::Timestamp()+L" "+text+L"\r\n");}catch(...) {}}
HWND Control(const wchar_t* kind,const wchar_t* text,DWORD style,int id){auto h=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;}
void Layout(){RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom,left=signedIn?272:40;
 MoveWindow(menu,18,30,234,height-55,TRUE);MoveWindow(title,left,32,width-left-28,54,TRUE);MoveWindow(description,left,94,width-left-28,70,TRUE);
 MoveWindow(input,left,172,width-left-28,34,TRUE);MoveWindow(remember,left,216,330,30,TRUE);
 int buttonWidth=(width-left-46)/4;for(int i=0;i<4;++i)MoveWindow(buttons[i],left+i*(buttonWidth+6),254,buttonWidth,38,TRUE);
 MoveWindow(content,left,310,width-left-28,std::max(50,height-385),TRUE);MoveWindow(notice,left,height-62,width-left-28,50,TRUE);
}
void Button(int index,const wchar_t* label,bool enabled=true){Set(buttons[index],label);ShowWindow(buttons[index],*label?SW_SHOW:SW_HIDE);EnableWindow(buttons[index],enabled);}
bool DocumentPage(){return page==12||page==13;}
void SaveDocument(){if(signedIn&&DocumentPage())Core::Write(Operations::Document(settings->directory,page),Text(content));}
void OpenFolder(const Core::fs::path& path){if(!Core::fs::is_directory(path))throw std::runtime_error("Directory unavailable");if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Cannot open folder");}
std::wstring Controllers(const std::vector<Telemetry::Controller>& controls){std::wostringstream out;out<<connector.snapshot.status<<L"\r\n"<<connector.snapshot.loco<<L"\r\n\r\nID\tController\tHodnota / minimum / maximum\r\n";
 for(auto& c:controls){std::wstring name;try{name=Core::Wide(c.name);}catch(...){name=L"(neplatné UTF-8)";}out<<c.id<<L"\t"<<name<<L"\t";for(float v:{c.value,c.minimum,c.maximum}){if(Telemetry::Valid(v))out<<std::fixed<<std::setprecision(3)<<v;else out<<L"—";out<<L" / ";}out<<L"\r\n";}return out.str();}
void Refresh(){if(!signedIn)return;const auto& snapshot=connector.snapshot;
 if(page==0){float speed=0;std::wstring value=Telemetry::SpeedKph(snapshot.controllers,speed)?std::to_wstring(speed)+L" km/h":L"—";
 Set(content,L"PROFIL\r\n"+settings->profile+L"\r\n\r\nINSTALACE\r\n"+(settings->railworks.empty()?L"Nenalezena — vyber Nastavení":settings->railworks)+L"\r\n\r\nCONNECTOR\r\n"+snapshot.status+L"\r\n\r\nLOKOMOTIVA\r\n"+(snapshot.loco.empty()?L"—":snapshot.loco)+L"\r\n\r\nRYCHLOST\r\n"+value+L"\r\n\r\nVERZE "+Core::Wide(HUB_VERSION)+L"\r\n"+updateMessage);}
 else if(page==1||page==2||page==14||page==16)Set(content,Controllers(snapshot.controllers));
 else if(page==9||page==10)Set(content,Controllers(Radio::Controls(snapshot))+L"\r\nBez ověřeného profilu nelze bezpečně ovládat rádio.");
 else if(page==7){float speed=0;std::wostringstream out;out<<snapshot.status<<L"\r\n\r\n"<<snapshot.loco<<L"\r\n\r\n";if(Telemetry::SpeedKph(snapshot.controllers,speed))out<<std::fixed<<std::setprecision(1)<<speed<<L" km/h";else out<<L"— km/h\r\nLokomotiva neposkytuje podporovaný ukazatel rychlosti.";Set(content,out.str());}
 else if(page==6){std::wstring text=L"Načti CSV pomocí tlačítka Vybrat soubor.";if(!stops.empty()){text=L"Trasa: "+routePath+L"\r\n\r\nZastávka "+std::to_wstring(stopIndex+1)+L" / "+std::to_wstring(stops.size())+L"\r\n\r\n"+Core::Wide(stops[stopIndex].name)+L"\r\n\r\n";for(size_t i=0;i<stops.size();++i)text+=(i==stopIndex?L"→ ":L"    ")+Core::Wide(stops[i].name)+L"\r\n";}Set(content,text);EnableWindow(buttons[1],stopIndex>0);EnableWindow(buttons[2],!stops.empty()&&stopIndex+1<stops.size());}
 else if(page==4){Set(content,L"Instalovaná verze: "+Core::Wide(HUB_VERSION)+L"\r\n\r\n"+updateMessage+L"\r\n\r\n"+Core::Read(settings->directory/L"update-result.txt"));EnableWindow(buttons[0],!busy);EnableWindow(buttons[1],!busy&&!latest.empty());}
}
void CheckUpdates(bool install){if(busy)return;if(worker.joinable())worker.join();busy=true;installing=install;const auto selected=latest;updateMessage=install?L"Stahuji a ověřuji aktualizaci…":L"Kontroluji GitHub Releases…";
 worker=std::thread([selected,install]{auto result=new std::pair<bool,std::string>;try{if(install){Core::Updates::Stage(selected);*result={true,"installed"};}else *result={true,Core::Updates::Latest()};}catch(const std::exception& e){*result={false,e.what()};}if(!PostMessageW(window,UpdateDone,0,reinterpret_cast<LPARAM>(result)))delete result;});Refresh();}
void ShowPage(int next){SaveDocument();loading=true;page=next;SendMessageW(menu,LB_SETCURSEL,page,0);Set(title,modules[page].title);Set(description,modules[page].description);Set(input,L"");Set(content,L"");Status(L"");ShowWindow(input,SW_HIDE);ShowWindow(remember,SW_HIDE);SendMessageW(content,EM_SETREADONLY,TRUE,0);for(int i=0;i<4;++i)Button(i,L"");
 if(page==0){Button(0,L"Spustit TSC");Button(1,L"Nastavení");Button(2,L"Connector");Button(3,L"Datová složka");}
 else if(page==3){ShowWindow(input,SW_SHOW);Set(input,settings->railworks);Button(0,L"Použít cestu");Button(1,L"Vybrat složku");Button(2,L"Vybrat EXE");Button(3,L"Automaticky najít");Set(content,L"Přijímá složku RailWorks, Steam knihovnu, steamapps, common nebo EXE hry.\r\n\r\nNeplatný výběr zachová uloženou instalaci.\r\n\r\nConnector používá RailDriver64.dll. Pro telemetrii spusť 64bitovou hru.");}
 else if(page==4){Button(0,L"Zkontrolovat");Button(1,L"Instalovat a restartovat");}
 else if(page==5){ShowWindow(input,SW_SHOW);ShowWindow(remember,SW_SHOW);Set(input,settings->profile);SendMessageW(remember,BM_SETCHECK,settings->remember?BST_CHECKED:BST_UNCHECKED,0);Button(0,L"Uložit profil");Button(1,L"Odhlásit");Set(content,L"Lokální profil nevyžaduje heslo. Data chrání oprávnění tvého účtu Windows.\r\n\r\nOnline účet, synchronizace a registrace nejsou připojené.");}
 else if(page==6){Button(0,L"Vybrat soubor");Button(1,L"Předchozí");Button(2,L"Následující");}
 else if(page==8){ShowWindow(input,SW_SHOW);SendMessageW(input,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Číslo vlaku / popis jízdy"));Train::Journeys journal(settings->directory);Button(0,L"Zahájit jízdu",!journal.Active());Button(1,L"Ukončit jízdu",journal.Active());Button(2,L"Otevřít CSV složku");Set(content,(journal.Active()?L"Probíhá jízda: "+Core::Read(settings->directory/L"active-journey.txt")+L"\r\n\r\n":L"")+journal.History());}
 else if(DocumentPage()){SendMessageW(content,EM_SETREADONLY,FALSE,0);Set(content,Core::Read(Operations::Document(settings->directory,page)));Button(0,L"Uložit");Button(1,L"Otevřít složku");}
 else if(page>=17){auto sub=page==19?L"Assets":page==18?L"Content":page==21?L"":L"Content/Routes";Set(content,Tools::Inventory(settings->railworks,sub));Button(0,L"Obnovit inventář");Button(1,L"Otevřít složku");}
 else if(page==11){Set(content,L"Požadované podklady pro integraci:\r\n\r\n• adresa a protokol radio serveru\r\n• přihlášení a oprávnění\r\n• formát kanálů a hlasového přenosu\r\n\r\nŽádné spojení se serverem není simulováno.");}
 else if(page==15){Set(content,L"RailDriver neposkytuje ověřený mapový podklad.\r\nVirtuální controllery 400/401 zde nejsou vydávány za spolehlivou GPS.\r\n\r\nŽivá data můžeš zkontrolovat v Connectoru.");Button(0,L"TSC Connector");}
 else {Button(0,L"Znovu připojit");}
 loading=false;Refresh();Layout();}
void Login(){signedIn=false;page=-1;ShowWindow(menu,SW_HIDE);ShowWindow(input,SW_SHOW);ShowWindow(remember,SW_SHOW);Set(title,L"Kvaltík TSC Hub 2.0");Set(description,L"Přihlášení do lokálního profilu · jméno zůstává pouze na tomto počítači");Set(input,settings->profile);SendMessageW(remember,BM_SETCHECK,settings->remember?BST_CHECKED:BST_UNCHECKED,0);Button(0,L"Pokračovat");for(int i=1;i<4;++i)Button(i,L"");SendMessageW(content,EM_SETREADONLY,TRUE,0);Set(content,L"Vítej na palubě.\r\n\r\nZadej jméno profilu a pokračuj na přehled.\r\n\r\nOnline účty nejsou připojené; heslo ani e-mail se nevyžadují.");Layout();SetFocus(input);}
void UsePath(const std::wstring& selected){auto resolved=RailWorksPath::Resolve(selected);if(resolved.empty()){Status(L"Nenalezen RailWorks64.exe ani RailWorks.exe. Původní cesta zůstává zachována.");return;}auto previous=settings->railworks;settings->railworks=resolved;try{settings->Save();}catch(...){settings->railworks=previous;throw;}connector.Connect(resolved);connector.Poll();Set(input,resolved);Status(L"Instalace uložena: "+resolved);Log(L"Instalace: "+resolved);}
std::wstring PickFile(const wchar_t* filter){wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=filter;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;return GetOpenFileNameW(&dialog)?path:L"";}
void Action(int index){
 if(!signedIn){auto name=Text(input);if(!Core::ValidProfile(name)){Status(L"Zadej jméno o délce 1 až 64 znaků.");return;}settings->profile=name;settings->remember=SendMessageW(remember,BM_GETCHECK,0,0)==BST_CHECKED;settings->Save();signedIn=true;ShowWindow(menu,SW_SHOW);ShowPage(0);CheckUpdates(false);return;}
 if(page==0){if(index==1){ShowPage(3);return;}if(index==2){ShowPage(1);return;}if(index==3){OpenFolder(settings->directory);return;}if(!RailWorksPath::IsRoot(settings->railworks)){ShowPage(3);Status(L"Nejprve vyber instalaci.");return;}auto exe=Core::fs::path(settings->railworks)/L"RailWorks64.exe";if(!Core::fs::exists(exe))exe=Core::fs::path(settings->railworks)/L"RailWorks.exe";if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",exe.c_str(),nullptr,settings->railworks.c_str(),SW_SHOWNORMAL))<=32)throw std::runtime_error("Cannot launch TSC");Status(L"Požadavek na spuštění hry odeslán.");}
 else if(page==3){if(index==0)UsePath(Text(input));else if(index==3){auto found=Core::FindRailWorks();if(found.empty())Status(L"Automatická detekce nic nenašla. Vyber složku nebo EXE ručně.");else UsePath(found);}else if(index==2){auto file=PickFile(L"RailWorks (*.exe)\0*.exe\0\0");if(!file.empty())UsePath(file);}else{BROWSEINFOW b{};b.hwndOwner=window;b.lpszTitle=L"RailWorks nebo Steam knihovna";b.ulFlags=BIF_NEWDIALOGSTYLE|BIF_EDITBOX;auto pidl=SHBrowseForFolderW(&b);if(pidl){wchar_t path[32768]{};bool ok=SHGetPathFromIDListEx(pidl,path,32768,GPFIDL_DEFAULT)!=FALSE;CoTaskMemFree(pidl);if(ok)UsePath(path);}}}
 else if(page==4){if(index==1){SaveDocument();if(latest.empty())return;}CheckUpdates(index==1);}
 else if(page==5){if(index==1){settings->remember=false;settings->Save();Login();}else{auto name=Text(input);if(!Core::ValidProfile(name))throw std::runtime_error("Invalid profile name");settings->profile=name;settings->remember=SendMessageW(remember,BM_GETCHECK,0,0)==BST_CHECKED;settings->Save();Status(L"Profil uložen.");}}
 else if(page==6){if(index==0){auto path=PickFile(L"Zastávky CSV (*.csv)\0*.csv\0\0");if(path.empty())return;std::ifstream file{Core::fs::path(path)};auto parsed=Navigation::Parse(file);for(auto& stop:parsed)Core::Wide(stop.name);stops=std::move(parsed);stopIndex=0;routePath=path;}else if(index==1&&stopIndex>0)--stopIndex;else if(index==2&&stopIndex+1<stops.size())++stopIndex;Refresh();}
 else if(page==8){Train::Journeys journal(settings->directory);if(index==0)journal.Begin(Text(input),connector.snapshot.loco);else if(index==1)journal.End();else OpenFolder(settings->directory);ShowPage(8);}
 else if(DocumentPage()){SaveDocument();if(index==1)OpenFolder(settings->directory);Status(L"Poznámky uloženy v UTF-8.");}
 else if(page>=17){if(index==0)ShowPage(page);else{if(settings->railworks.empty())throw std::runtime_error("Select TSC installation first");OpenFolder(Core::fs::path(settings->railworks)/(page==19?L"Assets":page==18?L"Content":page==21?L"":L"Content/Routes"));}}
 else if(page==15)ShowPage(1);
 else {connector.Connect(settings->railworks);connector.Poll();Refresh();}
}
LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){try{switch(msg){
 case WM_CREATE:{window=hwnd;menu=Control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP|LBS_NOINTEGRALHEIGHT,10);for(auto& m:modules)SendMessageW(menu,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(m.menu));title=Control(L"STATIC",L"",0,0);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(heading),TRUE);description=Control(L"STATIC",L"",0,0);input=Control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,11);SendMessageW(input,EM_SETLIMITTEXT,32767,0);remember=Control(L"BUTTON",L"Zapamatovat profil",BS_AUTOCHECKBOX|WS_TABSTOP,12);for(int i=0;i<4;++i)buttons[i]=Control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,20+i);content=Control(L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_TABSTOP,30);SendMessageW(content,EM_SETLIMITTEXT,1000000,0);notice=Control(L"STATIC",L"",0,0);Login();SetTimer(hwnd,1,1000,nullptr);return 0;}
 case WM_SIZE:Layout();return 0;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={960,640};return 0;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,RGB(224,234,245));bool panel=reinterpret_cast<HWND>(lp)==content||reinterpret_cast<HWND>(lp)==input||reinterpret_cast<HWND>(lp)==menu;SetBkColor(dc,panel?RGB(24,35,49):RGB(12,19,30));return reinterpret_cast<LRESULT>(panel?surface:background);}
 case WM_COMMAND:if(LOWORD(wp)==10&&HIWORD(wp)==LBN_SELCHANGE){auto selected=static_cast<int>(SendMessageW(menu,LB_GETCURSEL,0,0));try{if(selected>=0)ShowPage(selected);}catch(...){SendMessageW(menu,LB_SETCURSEL,page,0);throw;}}else if(LOWORD(wp)>=20&&LOWORD(wp)<24&&HIWORD(wp)==BN_CLICKED)Action(LOWORD(wp)-20);else if(LOWORD(wp)==30&&HIWORD(wp)==EN_CHANGE&&!loading&&DocumentPage())Status(L"Změny se uloží při opuštění stránky. Můžeš je uložit i tlačítkem Uložit.");return 0;
 case WM_TIMER:connector.Poll();Refresh();return 0;
 case UpdateDone:{std::unique_ptr<std::pair<bool,std::string>> result(reinterpret_cast<std::pair<bool,std::string>*>(lp));if(worker.joinable())worker.join();busy=false;if(result->first){if(installing){PostMessageW(hwnd,WM_CLOSE,0,0);return 0;}if(Core::Updates::Version(result->second)>Core::Updates::Version(HUB_VERSION)){latest=result->second;updateMessage=L"Dostupná verze "+Core::Wide(latest)+L". Instalaci spustíš tlačítkem Instalovat a restartovat.";}else{latest.clear();updateMessage=L"Používáš aktuální nebo novější verzi.";}}else updateMessage=L"Aktualizace není dostupná: "+Core::Wide(result->second);Log(updateMessage);installing=false;Refresh();return 0;}
 case WM_CLOSE:SaveDocument();if(busy){Status(L"Probíhá kontrola nebo instalace aktualizace. Počkej na dokončení.");return 0;}DestroyWindow(hwnd);return 0;
 case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
 }}catch(const std::exception& e){std::wstring error=L"Operace se nezdařila. ";try{error+=Core::Wide(e.what());}catch(...){}Status(error);Log(error);}return DefWindowProcW(hwnd,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,PWSTR,int show){using namespace App;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);try{
 if(argc==4&&std::wstring(argv[1])==L"--apply-update"){int result=Core::Updates::Apply(std::stoul(argv[2]),argv[3]);LocalFree(argv);CoUninitialize();return result;}
 LocalFree(argv);instance=h;settings=std::make_unique<Core::Settings>();std::wstring startupError;try{settings->Load();}catch(...){startupError=L"Nastavení nelze načíst. Zkontroluj datovou složku.";}
 KvaltikSplash::Show(h);if(!RailWorksPath::IsRoot(settings->railworks)){try{auto found=Core::FindRailWorks();if(!found.empty())settings->railworks=found;}catch(...){startupError=L"Část Steam knihoven se nepodařilo přečíst. Použij ruční výběr.";}}
 connector.Connect(settings->railworks);connector.Poll();INITCOMMONCONTROLSEX icc{sizeof(icc),ICC_STANDARD_CLASSES};InitCommonControlsEx(&icc);
 font=CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");heading=CreateFontW(30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=Proc;wc.hInstance=h;wc.lpszClassName=L"KvaltikHub2";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=background;RegisterClassExW(&wc);
 window=CreateWindowExW(0,wc.lpszClassName,L"Kvaltík TSC Hub 2.0",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1180,800,nullptr,nullptr,h,nullptr);if(!window)throw std::runtime_error("Cannot create window");
 if(settings->remember&&Core::ValidProfile(settings->profile)){signedIn=true;ShowWindow(menu,SW_SHOW);ShowPage(0);CheckUpdates(false);}Status(startupError);ShowWindow(window,show);UpdateWindow(window);
 MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 if(worker.joinable())worker.join();connector.Disconnect();DeleteObject(font);DeleteObject(heading);DeleteObject(background);DeleteObject(surface);CoUninitialize();return static_cast<int>(msg.wParam);
 }catch(...){MessageBoxW(nullptr,L"Hub nelze spustit. Ověř oprávnění k místní složce AppData.",L"Kvaltík TSC Hub 2.0",MB_ICONERROR);CoUninitialize();return 1;}}
