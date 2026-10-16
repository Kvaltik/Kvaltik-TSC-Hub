#pragma once
#include <vector>
namespace App {
struct Module {const wchar_t* menu;const wchar_t* title;const wchar_t* description;};
inline const std::vector<Module> modules={
 {L"Přehled",L"Vítej v Kvaltík TSC Hub 2.0",L"Jedno místo pro instalaci, telemetrii a přípravu jízdy."},
 {L"Core / TSC Connector",L"TSC Connector",L"Skutečná data RailDriver · ID, hodnota, minimum a maximum · pouze čtení"},
 {L"Core / RailDriver",L"RailDriver",L"Diagnostika připojení 64bitové knihovny a dostupných controllerů."},
 {L"Core / Nastavení",L"Instalace Train Simulator Classic",L"Vyber RailWorks, Steam knihovnu nebo přímo RailWorks64.exe. Platná ruční volba se uloží."},
 {L"Core / Aktualizace",L"Aktualizace",L"Stable · GitHub Releases · kontrola SHA-256 · záloha předchozí verze"},
 {L"Core / Profil",L"Lokální profil",L"Profil tohoto uživatele Windows. Online registrace a cloudové účty nejsou součástí milníku."},
 {L"Train / NavTrain",L"NavTrain",L"Načti UTF-8 CSV: name;latitude;longitude. Postup mezi zastávkami je ruční."},
 {L"Train / Driver Display",L"Driver Display",L"Rychlost z explicitních controllerů SpeedometerKPH / SpeedometerMPH. Chybějící data se nedopočítávají."},
 {L"Train / Kniha jízd",L"Kniha jízd",L"Lokální záznam začátku a konce jízdy. Rozpracovaná jízda přežije restart."},
 {L"Radio / VO79",L"VO79 · diagnostika",L"Čtení dostupných radio controllerů. Ovládání vyžaduje ověřený profil konkrétní lokomotivy."},
 {L"Radio / Simplex",L"Simplex · diagnostika",L"Čtení dostupných radio controllerů. Hlasový přenos není v tomto milníku implementován."},
 {L"Radio / Radio Server",L"Radio Server client · stav integrace",L"Klient není připojen. Pro hlasovou komunikaci je nutný protokol serveru a autentizace."},
 {L"Operations / Jízdní řád",L"Jízdní řád · pracovní poznámky",L"Lokální editor poznámek k jízdnímu řádu. Uloží se při změně stránky i zavření aplikace."},
 {L"Operations / Rozkazovač",L"Rozkazovač · pracovní poznámky",L"Lokální editor provozních poznámek. Nejde o validovaný formulář drážního rozkazu."},
 {L"Operations / RailControl",L"RailControl · diagnostika",L"Telemetrie je dostupná níže. Schéma tratě a řízení výhybek vyžadují samostatný datový zdroj."},
 {L"Operations / Live Map",L"Live Map · stav dat",L"Mapa není aktivní: poloha z virtuálních controllerů dosud není ověřena ve hře."},
 {L"Operations / Výpravčí",L"Výpravčí · diagnostika",L"Přehled controllerů scénáře. Spouštění událostí vyžaduje ověřené mapování lokomotivy."},
 {L"Tools / Scenario Creator",L"Scenario Creator · instalované tratě",L"Inventář pro přípravu scénáře. Generování hratelného scénáře není v tomto milníku implementováno."},
 {L"Tools / Scenario Doctor",L"Scenario Doctor · inventář",L"Prohlídka složky Content. Kontrola závislostí .bin a archivů .ap zatím není implementována."},
 {L"Tools / Consist Manager",L"Consist Manager · inventář",L"Prohlídka poskytovatelů vozidel v Assets. Sestavování souprav zatím není implementováno."},
 {L"Tools / Route Manager",L"Route Manager · inventář",L"Složky instalovaných tratí. GUID není název tratě; čtení RouteProperties a .ap bude další etapa."},
 {L"Tools / Shader Manager",L"Shader Manager · inventář",L"Prohlídka kořene instalace. Změny shaderů a instalace DLL nejsou součástí milníku."}
};
}
