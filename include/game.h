#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include "snake_shared.h"

typedef struct {
    Point *snake;
    Point food;
    int length;
    int capacity;
    int eaten;
    int preset;
    Direction direction;
    Direction requested;
    EndReason reason;
} GameState;

bool point_equals(Point first, Point second);

/* snake må peke på minst length elementer; length kan være 0. */
bool snake_contains(const Point *snake, int length, Point point);

/*
 * Velger en tilfeldig ledig rute innenfor ytterveggen. snake må inneholde
 * length gyldige punkter. Returnerer false når brettet ikke har en ledig rute.
 */
bool place_food(Point *food, const Point *snake, int length,
                int width, int height);
bool directions_are_opposite(Direction first, Direction second);

/*
 * Initialiserer et nullstilt eller ferdig destruert GameState for preset 0-3.
 * Ved suksess eier GameState snake-bufferen til game_destroy() kalles.
 * Ved feil returneres false, og game kan trygt sendes til game_destroy().
 */
bool game_init(GameState *game, int preset_index);
void game_destroy(GameState *game);

/* Avviser en øyeblikkelig 180-graders vending. */
bool game_request_direction(GameState *game, Direction direction);

/*
 * Utfører ett steg med sist forespurte retning. false betyr at runden er
 * avsluttet, og reason angir kollisjon eller at brettet er fullt.
 */
bool game_step(GameState *game);

/* Returnerer gjeldende stegintervall, aldri lavere enn MIN_DELAY_MS. */
int game_delay_ms(const GameState *game);

#endif
