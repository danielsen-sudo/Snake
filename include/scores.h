#ifndef SNAKE_SCORES_H
#define SNAKE_SCORES_H

#include "snake_shared.h"

const char *score_file_path(void);
const char *score_temp_file_path(void);
const char *legacy_score_file_path(void);
int load_scores(Score scores[MAX_SCORES]);
void refresh_expired_scores(Score scores[MAX_SCORES], int *count);
bool save_score(Score scores[MAX_SCORES], int *count,
                const char *name, int value);

/* Eksponert for målrettede format-, utløps- og integrasjonstester. */
bool write_scores(const Score scores[MAX_SCORES], int count);
int remove_expired_scores(Score scores[MAX_SCORES], int count);
int score_rank(const Score scores[MAX_SCORES], int count, int value);

#endif
