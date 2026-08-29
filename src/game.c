#include "../include/game.h"

#include <stdlib.h>
#include <string.h>

bool point_equals(Point first, Point second)
{
    return first.x == second.x && first.y == second.y;
}

bool snake_contains(const Point *snake, int length, Point point)
{
    int index;
    for (index = 0; index < length; ++index) {
        if (point_equals(snake[index], point)) return true;
    }
    return false;
}

bool place_food(Point *food, const Point *snake, int length,
                int width, int height)
{
    int free_cells = (width - 2) * (height - 2) - length;
    int target;
    int seen = 0;
    int x;
    int y;

    if (free_cells <= 0) return false;
    target = rand() % free_cells;
    for (y = 1; y < height - 1; ++y) {
        for (x = 1; x < width - 1; ++x) {
            Point candidate = {x, y};
            if (!snake_contains(snake, length, candidate)) {
                if (seen == target) {
                    *food = candidate;
                    return true;
                }
                ++seen;
            }
        }
    }
    return false;
}

bool directions_are_opposite(Direction first, Direction second)
{
    return (first == DIR_UP && second == DIR_DOWN) ||
           (first == DIR_DOWN && second == DIR_UP) ||
           (first == DIR_LEFT && second == DIR_RIGHT) ||
           (first == DIR_RIGHT && second == DIR_LEFT);
}

bool game_init(GameState *game, int preset_index)
{
    const BoardPreset *board;

    if (game == NULL || preset_index < 0 || preset_index >= 4) return false;
    memset(game, 0, sizeof(*game));
    board = &PRESETS[preset_index];
    game->capacity = (board->width - 2) * (board->height - 2);
    game->snake = malloc((size_t)game->capacity * sizeof(*game->snake));
    if (game->snake == NULL) return false;

    game->length = 3;
    game->preset = preset_index;
    game->direction = DIR_RIGHT;
    game->requested = DIR_RIGHT;
    game->reason = END_ESCAPE;
    game->snake[0] = (Point){board->width / 2, board->height / 2};
    game->snake[1] = (Point){game->snake[0].x - 1, game->snake[0].y};
    game->snake[2] = (Point){game->snake[0].x - 2, game->snake[0].y};
    if (!place_food(&game->food, game->snake, game->length,
                    board->width, board->height)) {
        game_destroy(game);
        return false;
    }
    return true;
}

void game_destroy(GameState *game)
{
    if (game == NULL) return;
    free(game->snake);
    game->snake = NULL;
    game->length = 0;
    game->capacity = 0;
}

bool game_request_direction(GameState *game, Direction direction)
{
    if (game == NULL || directions_are_opposite(game->direction, direction))
        return false;
    game->requested = direction;
    return true;
}

int game_delay_ms(const GameState *game)
{
    int delay = START_DELAY_MS - game->eaten * SPEED_STEP_MS;
    return delay < MIN_DELAY_MS ? MIN_DELAY_MS : delay;
}

bool game_step(GameState *game)
{
    const BoardPreset *board = &PRESETS[game->preset];
    Point head = game->snake[0];
    bool grows;
    int collision_length;

    game->direction = game->requested;
    if (game->direction == DIR_UP) --head.y;
    else if (game->direction == DIR_DOWN) ++head.y;
    else if (game->direction == DIR_LEFT) --head.x;
    else ++head.x;

    grows = point_equals(head, game->food);
    /* Halen flytter seg ved vanlige steg og er da en lovlig målrute. */
    collision_length = grows ? game->length : game->length - 1;
    if (head.x <= 0 || head.x >= board->width - 1 || head.y <= 0 ||
        head.y >= board->height - 1 ||
        snake_contains(game->snake, collision_length, head)) {
        game->reason = END_COLLISION;
        return false;
    }

    memmove(&game->snake[1], &game->snake[0],
            (size_t)(grows ? game->length : game->length - 1) *
                sizeof(*game->snake));
    game->snake[0] = head;
    if (grows) {
        ++game->length;
        ++game->eaten;
        if (!place_food(&game->food, game->snake, game->length,
                        board->width, board->height)) {
            game->reason = END_BOARD_FULL;
            return false;
        }
    }
    return true;
}
