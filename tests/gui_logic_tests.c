#include "../include/game.h"
#include "../include/scores.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Rene motor- og rangeringsprøver uten terminal eller SDL-vindu. */

static void test_highscore_rank(void)
{
    Score scores[MAX_SCORES] = {
        {"Ada", 12, 1}, {"Bo", 8, 1}, {"Cy", 8, 1}, {"Dee", 4, 1}
    };

    assert(score_rank(scores, 4, 13) == 1);
    assert(score_rank(scores, 4, 8) == 4);
    assert(score_rank(scores, 4, 3) == 5);
}

static void test_growth(void)
{
    GameState game = {0};

    srand(7);
    assert(game_init(&game, 1));
    game.food = (Point){game.snake[0].x + 1, game.snake[0].y};
    assert(game_step(&game));
    assert(game.length == 4);
    assert(game.eaten == 1);
    game_destroy(&game);
}

static void test_wall_collision(void)
{
    GameState game = {0};

    srand(9);
    assert(game_init(&game, 1));
    game.snake[0] = (Point){1, 5};
    game.direction = DIR_LEFT;
    game.requested = DIR_LEFT;
    assert(!game_step(&game));
    assert(game.reason == END_COLLISION);
    game_destroy(&game);
}

static void test_direction_and_speed(void)
{
    GameState game = {0};

    srand(11);
    assert(game_init(&game, 1));
    assert(!game_request_direction(&game, DIR_LEFT));
    assert(game.requested == DIR_RIGHT);
    assert(game_request_direction(&game, DIR_UP));
    assert(game.requested == DIR_UP);
    assert(game_delay_ms(&game) == START_DELAY_MS);
    game.eaten = 100;
    assert(game_delay_ms(&game) == MIN_DELAY_MS);
    game_destroy(&game);
}

static void test_self_collision(void)
{
    GameState game = {0};

    srand(13);
    assert(game_init(&game, 1));
    game.length = 4;
    game.snake[0] = (Point){5, 5};
    game.snake[1] = (Point){5, 6};
    game.snake[2] = (Point){4, 6};
    game.snake[3] = (Point){4, 5};
    game.direction = DIR_DOWN;
    game.requested = DIR_DOWN;
    assert(!game_step(&game));
    assert(game.reason == END_COLLISION);
    game_destroy(&game);
}

int main(void)
{
    test_highscore_rank();
    test_growth();
    test_wall_collision();
    test_direction_and_speed();
    test_self_collision();
    puts("Motor- og rangeringsprøver bestått.");
    return 0;
}
