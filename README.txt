        __  ___
       /  |/  /___ _____  ___     _______  ______  ____  ___  _____
      / /|_/ / __ `/_  / / _ \   / ___/ / / / __ \/ __ \/ _ \/ ___/
     / /  / / /_/ / / /_/  __/  / /  / /_/ / / / / / / /  __/ /
    /_/  /_/\__,_/ /___/\___/  /_/   \__,_/_/ /_/_/ /_/\___/_/

## Komanda
Komandas sastāvs: Mairis Bērziņš, Ralfs Braunfelds.

## Ieguldījums
Mairis Bērziņš: 50%
Ralfs Braunfelds: 50%

## Apraksts
Mazerunner ir kursa "Linux sistēmas programmēšana" gala darba ietvaros izveidota programma - reāla laika daudzlietotāju spēle ar klientu un serveri.

Taustiņu kontroles:
* Uz augšu: W vai I
* Pa kreisi: A vai J
* Uz leju: S vai K
* Pa labi: D vai L

## Klients
Lai palaistu klientu, nepieciešama uzinstalēta "ncurses" bibliotēka. Ja tā nav, tad lai to izdarītu jāizpilda komanda "sudo apt-get install libncurses5-dev libncursesw5-dev".

Lai sakompilētu klientu, jāizpilda "cmp_mazerunner.sh" skripts iekš "client" mapes ar komandu "sh cmp_mazerunner.sh".

Lai palaistu klientu nepieciešams uztaisīt klienta konfigurācijas failu "client.cfg". Tā paraugs ir atrodams failā "client.cfg.template".

Lai palaistu klientu, jāizpilda uzģenerētais "mazerunner" fails ar komandu "./mazerunner" iekš "client" mapes.

## Serveris
Lai sakompilētu serveri, jāizpilda "compile.sh" skripts iekš "server" mapes.

Lai palaistu serveri nepieciešams uzstādīt servera konfigurācijas failu "server.cfg". Tā paraugs ir atrodams failā "server.cfg.template".

Lai palaistu serveri, jāizpilda uzģenerētais "server" fails ar komandu "./server" iekš "server" mapes.