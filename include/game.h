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
bool snake_contains(const Point *snake, int length, Point point);
bool place_food(Point *food, const Point *snake, int length,
                int width, int height);
bool directions_are_opposite(Direction first, Direction second);

/* GameState eier snake-bufferen fra init lykkes til game_destroy kalles. */
bool game_init(GameState *game, int preset_index);
void game_destroy(GameState *game);

/* Avviser en øyeblikkelig 180-graders vending. */
bool game_request_direction(GameState *game, Direction direction);

/* Utfører ett steg. false betyr at reason beskriver hvorfor runden er over. */
bool game_step(GameState *game);
int game_delay_ms(const GameState *game);

#endif
