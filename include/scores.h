#ifndef SNAKE_SCORES_H
#define SNAKE_SCORES_H

#include "snake_shared.h"

const char *score_file_path(void);
const char *score_temp_file_path(void);
const char *legacy_score_file_path(void);

/*
 * Leser, autentiserer og filtrerer den aktive topplisten. scores må ha plass
 * til MAX_SCORES elementer. Returnerer antall gyldige oppføringer; 0 betyr
 * enten tom liste eller at filen ikke kunne leses eller godkjennes.
 */
int load_scores(Score scores[MAX_SCORES]);

/* Fjerner utløpte oppføringer i minnet og oppdaterer filen ved endring. */
void refresh_expired_scores(Score scores[MAX_SCORES], int *count);

/*
 * Validerer og rangerer et resultat, og skriver topplisten atomisk når
 * resultatet får plass. Listen må være sortert synkende, med eldre like
 * resultater først. Returnerer false ved ugyldige argumenter eller lagringsfeil.
 */
bool save_score(Score scores[MAX_SCORES], int *count,
                const char *name, int value);

/*
 * Lavnivåfunksjoner eksponert for målrettede format- og integrasjonstester.
 * write_scores() erstatter den aktive filen via en midlertidig fil.
 */
bool write_scores(const Score scores[MAX_SCORES], int count);
int remove_expired_scores(Score scores[MAX_SCORES], int count);

/* Returnerer kandidatens 1-baserte plass; verdier over MAX_SCORES er utenfor. */
int score_rank(const Score scores[MAX_SCORES], int count, int value);

#endif
