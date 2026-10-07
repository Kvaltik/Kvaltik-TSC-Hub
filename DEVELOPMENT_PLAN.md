# Plán funkčních modulů

Cíl: všech 15 modulů má skutečné chování, jasně uvedené limity a ověřitelné výsledky. Samotná karta, dialog s popisem nebo změna textu tlačítka není dokončený modul.

## Společný základ

- TSC Connector: ověřit exporty RailDriver64.dll, životní cyklus připojení a odpojení, seznam controllerů se zachovanými ID, aktuální hodnoty a rozsahy, změnu lokomotivy a chybové stavy.
- Nastavení: trvale uložit zvolenou instalaci, jednotky a profily lokomotiv; neplatné nastavení vysvětlit.
- Modulová okna: jednotný vzhled, klávesnice, stav bez připojení, zavírání bez ukončení Hubu.
- Ukládání: UTF-8, kontrola chyb zápisu, žádné falešné potvrzení úspěchu; zálohy před změnou herních dat.

## Rozsah první použitelné verze

| Modul | Co má uživatel skutečně udělat | Závislost / co ověřit |
|---|---|---|
| TSC Connector | Zkontrolovat DLL, lokomotivu, ID, hodnoty a rozsahy controllerů; vidět důvod odpojení | RailDriver API; test i s běžícím scénářem |
| Driver Display | Otevřít samostatný panel na druhém monitoru s rychlostí a vybranými ukazateli | Jednotky a mapování podle lokomotivy; žádné vymyšlené hodnoty |
| NavTrain | Sledovat postup po zvolené trase a následující zastávku | Poloha, trasa, zastávky a jízdní řád; zdroj nutno zvolit |
| VO79 | Nastavit kanál a volací znak, ovládat podporované funkce rádia | Konkrétní lokomotiva a její controllery; odlišit lokální panel od simulovaného rádia |
| Jízdní řád | Načíst, upravit, uložit a zobrazit zastávky, příjezdy a odjezdy | Nejprve lokální formát; online zdroj až po výběru poskytovatele |
| Kniha jízd | Založit jízdu, ukončit ji, zobrazit historii a exportovat CSV | Lokální ukládání, správná diakritika a escapování CSV |
| Scenario Creator | Připravit a uložit scénář se zvolenou tratí, soupravou a zastávkami | Nejprve ověřený formát herního scénáře; textový návrh není hratelný scénář |
| Consist Manager | Prohlížet a sestavit soupravu z dostupných vozidel | Formáty blueprintů a ConsistTemplates, pořadí a orientace vozů, záloha |
| Route Manager | Zobrazit tratě názvem, scénáře a stav dostupnosti | RouteProperties.xml, jazykové varianty a balíčky .ap |
| Scenario Doctor | Vypsat konkrétní chybějící závislosti scénáře a umístění problému | Čtení .bin/.xml a .ap; nerozbalené balíčky nesmějí vyvolat falešné chyby |
| Shader Manager | Zálohovat, přepnout a obnovit existující profil ReShade | Detekce ReShade, cesty k presetům; žádné stahování neověřených DLL |
| RailControl | Zobrazit schéma tratě a dostupné provozní informace | Datový zdroj výhybek, návěstidel a provozu; RailDriver sám nemusí stačit |
| Live Map | Zobrazit polohu a směr vlaku na mapě | Ověřená GPS data, mapový podklad a jeho licence; stav bez GPS |
| Rozkazovač | Vytvořit, uložit a znovu otevřít strukturovaný rozkaz | Dohodnout typy rozkazů a jejich pole |
| Výpravčí | Přehrát zvolené odjezdové signály a spouštět podporované události | Zvukové soubory s právem použití, mapování událostí do hry |

## Doporučené pořadí

1. Connector, nastavení a diagnostika.
2. Driver Display, Kniha jízd, lokální Jízdní řád a Rozkazovač.
3. Route Manager a Scenario Doctor; potom Consist Manager a Scenario Creator.
4. Profily lokomotiv pro VO79 a Výpravčího, Shader Manager.
5. Poloha a trasa pro Live Map, NavTrain a RailControl.

## Stav při zahájení

Hlavní program načítá pouze GetLocoName. Všech 15 karet volá ModuleInfo. Soubor modules.h není připojený a obsahuje pouze náčrty; nelze jej prezentovat jako hotovou implementaci. Před připojením vyžaduje opravu práce s řetězci, ukládání a zachování ID controllerů.

## Ověření každé etapy

Build a relevantní automatické testy v GitHub Actions. U telemetrie navíc zkouška bez hry, v menu, v běžícím scénáři a po změně lokomotivy. Dokončené moduly označovat jednotlivě, nikoliv odstranit upozornění o přípravě ze všech karet najednou.
