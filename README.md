# Snake 1.4

Terminalbasert Snake skrevet i C for Linux.

## Bygg og start

```sh
make
./build/snake
```

Slangen styres med piltastene. Trykk `Esc` for å avbryte et aktivt spill.
Menyvalg aktiveres direkte med talltastene, uten Enter. Startskjermen viser
de tre beste resultatene, mens hele topplisten åpnes fra menyvalg 2. Topplisten
opprettes automatisk i `data/toppliste.dat`, krypteres og autentiseres med
XChaCha20-Poly1305 fra libsodium og fjerner resultater som er 15 dager
gamle. Et eldre `toppliste.txt` migreres automatisk.

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
- `tests/` inneholder automatiske tester.
- `docs/` inneholder krav og versjonsendringer.
- `build/` inneholder genererte programmer.
- `data/` inneholder den krypterte topplisten og slettes ikke av `make clean`.
