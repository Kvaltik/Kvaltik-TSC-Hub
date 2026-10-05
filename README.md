# Kvaltík TSC Hub

Jedna společná Windows aplikace pro naše projekty kolem **Train Simulator Classic / RailWorks**.

## Moduly

- NavTrain
- VO79
- RailControl
- Scenario Creator
- Sešitový jízdní řád
- Consist Manager
- Scenario Doctor
- Kniha jízd
- Live Map
- Shader Manager
- Route Manager
- Rozkazovač
- Výpravčí
- Driver Display
- TSC Connector

## Aktuální verze

První nativní Win32 základ umí:

- automaticky hledat instalaci RailWorks,
- poznat, zda běží Train Simulator Classic,
- načíst `RailDriver64.dll`,
- zobrazit název lokomotivy, pokud ho DLL vrátí,
- spustit TSC,
- otevřít složky RailWorks / Assets / Content / Routes / plugins,
- zobrazit hlavní menu všech připravovaných modulů.

## Hotové EXE bez instalace C++

Tento repozitář používá **GitHub Actions**.

Po každém pushi do `main` se na Windows serveru GitHubu automaticky sestaví:

`KvaltikTSCHub.exe`

Najdeš ho v:

**Actions → Build Windows EXE → poslední úspěšný běh → Artifacts → KvaltikTSCHub-Windows-x64**

Na svém PC kvůli buildu nepotřebuješ Visual Studio, C++, .NET, Node.js ani Python.

## Vývoj

Další cíl je v0.2:

- moderní tmavý vzhled,
- moduly uvnitř jednoho okna místo popupů,
- plný TSC Connector,
- Controller Scanner,
- VO79 jako první plně funkční modul,
- ukládání nastavení a vlastní cesta k RailWorks.
