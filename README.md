# Snake 1.5.0

Terminalbasert Snake skrevet i C for Linux.

Prosjektet har også et SDL3-basert GUI. Terminalspillet er fortsatt
standardprogrammet og fungerer uavhengig av GUI-et.

## Bygg og start

```sh
make
./build/snake
```

## SDL3-prototype

SDL3 ligger lokalt i `.deps/sdl3`. Bygg og start GUI-vinduet med:

```sh
make gui
make run-gui
```

GUI-et har oppstartsmeny, spillernavn, fire brettstørrelser og toppliste fra
den samme krypterte datafilen som terminalspillet. Menyvalgene kan aktiveres
med talltaster eller mus. Slangen styres med piltaster eller WASD, og spillet
blir raskere for hver matbit. `P` eller mellomrom pauser spillet, og vinduet
pauses automatisk når det mister fokus. `Esc` avbryter uten å lagre. Etter
Game Over starter `R` en ny runde på samme brett.

### SDL3 for utviklere

Den lokale SDL3-installasjonen ligger i `.deps/sdl3` og er ignorert av Git.
På Ubuntu-baserte systemer krever bygget CMake, Ninja, XRandR, FreeType og
HarfBuzz:

```sh
sudo apt install cmake ninja-build libxrandr-dev libfreetype-dev libharfbuzz-dev
```

Bygg deretter de fastlåste og sjekksumverifiserte versjonene av SDL3 og
SDL3_ttf lokalt:

```sh
./scripts/bootstrap-sdl3.sh
make gui
make test-gui
```

CLI og GUI har separate startpunkter og bruker én felles motor fra `game.c`
og `include/game.h`. Topplistekontrakten ligger i `include/scores.h`,
lagringsstier i `score_storage.c` og aktiv kryptering i `score_crypto.c`.
Noto Sans og fontlisensen ligger i `assets/fonts/`. Behold `make test`,
`make test-gui` og `make test-sanitize` grønne ved videre refaktorering.

Slangen styres med piltastene. Trykk `Esc` for å avbryte et aktivt spill.
Menyvalg aktiveres direkte med talltastene, uten Enter. Startskjermen viser
de tre beste resultatene, mens hele topplisten åpnes fra menyvalg 2. Topplisten
opprettes automatisk i `${XDG_DATA_HOME}/snake/toppliste.dat`, eller i
`~/.local/share/snake/toppliste.dat` når `XDG_DATA_HOME` ikke er satt.
`SNAKE_DATA_DIR` kan overstyre plasseringen ved testing. En eksisterende
`data/toppliste.dat` eller eldre `toppliste.txt` migreres automatisk uten å
slette originalen. Filen krypteres og autentiseres med XChaCha20-Poly1305 fra
libsodium, og resultater som er 15 dager gamle fjernes.

Prosjektet krever libsodium. På Linux Mint kan utviklingspakken installeres
med `sudo apt install libsodium-dev`. Byggesystemet kan også bruke det
installerte kjørebiblioteket direkte dersom utviklingspakken mangler.

## Tester

```sh
make test
```

Testene dekker sortering, like poengsummer, utløp, kryptert innlasting og
avvisning av manipulert lagringsdata.

## Prosjektstruktur

- `src/` inneholder spillkoden.
- `include/` inneholder lokale deklarasjoner.
- `assets/fonts/` inneholder GUI-fonten og dens lisens.
- `tests/` inneholder automatiske tester.
- `docs/` inneholder krav og versjonsendringer.
- `Historie/` inneholder urørte øyeblikksbilder av eldre versjoner.
- `build/` inneholder genererte programmer.
- `data/` kan inneholde eldre topplistedata som migreres ved første oppstart.

Planlagte, utsatte oppgaver står i [TODO.md](TODO.md).
