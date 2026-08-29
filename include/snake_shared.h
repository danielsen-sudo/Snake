#ifndef SNAKE_SHARED_H
#define SNAKE_SHARED_H

/*
 * Felles kontrakt for terminal- og SDL3-programmet.
 * Grensesnittene skal bruke disse deklarasjonene og aldri inkludere
 * hverandres .c-filer.
 */

#include <stdbool.h>
#include <stdint.h>

#define GAME_VERSION "1.5.0"
#define MAX_SCORES 10
#define MAX_NAME_CHARS 50
#define MAX_NAME_BYTES (MAX_NAME_CHARS * 4)
#define START_DELAY_MS 200
#define MIN_DELAY_MS 50
#define SPEED_STEP_MS 10
#define SCORE_LIFETIME_SECONDS (15LL * 24LL * 60LL * 60LL)

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    char name[MAX_NAME_BYTES + 1];
    int score;
    int64_t created_at;
} Score;

typedef struct {
    int width;
    int height;
    const char *name;
} BoardPreset;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef enum {
    END_COLLISION,
    END_ESCAPE,
    END_BOARD_FULL
} EndReason;

extern const BoardPreset PRESETS[4];

/* Validerer UTF-8 og returnerer antall tegn, eller -1 ved ugyldige byte. */
int utf8_character_count(const char *text);

int snake_terminal_main(void);

#endif
