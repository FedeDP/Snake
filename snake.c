//
//  main.c
//  Snake
//
//  Created by Gabriele Carrettoni on 06/11/14.
//  Copyright (c) 2014 Gabriele Carrettoni. All rights reserved.
//

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>

#define ROWS 30
#define COLS 120
#define STARTING_SIZE 3
#define RIGHT 0
#define LEFT 1
#define UP 2
#define DOWN 3

typedef struct list {
    int x;
    int y;
    int direction;
    struct list *next;
    struct list *previous;
} snake;

/* Program state struct */
struct state {
    int rowtot;
    int coltot;
    int points;
    int lose;
    char grid[ROWS][COLS];
    snake *s;
    WINDOW *field;
    WINDOW *score;
};

static int screen_init(struct state *ps);
static void screen_end(struct state *ps);
static snake *reclist(int i, snake *previous, struct state *ps);
static void freelist(snake *s);
static void fruit_gen(struct state *ps);
static void grid_init(struct state *ps);
static void change_directions(int direction, struct state *ps);
static void eat_fruit(int x, int y, struct state *ps);
static void snake_move(struct state *ps);
static snake *snake_grow(int x, int y, struct state *ps);
static int main_cycle(struct state *ps);
static int size(snake *s);

int main(void)
{
    static struct state ps;
    ps.lose = 0;
    ps.points = 0;
    srand(time(NULL));
    if (screen_init(&ps))
        return 1;
    grid_init(&ps);
    while (!ps.lose) {
        if (!main_cycle(&ps))
            break;
    }
    freelist(ps.s);
    screen_end(&ps);
    return 0;
}

static int screen_init(struct state *ps)
{
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    raw();
    noecho();
    getmaxyx(stdscr, ps->rowtot, ps->coltot);
    /* check terminal size */
    if ((ps->rowtot < ROWS + 6) || (ps->coltot < COLS + 2)) {
        clear();
        endwin();
        printf("This screen has %d rows and %d columns. Enlarge it.\n", ps->rowtot, ps->coltot);
        printf("You need at least %d rows and %d columns.\n", ROWS + 6, COLS + 2);
        return 1;
    }
    /* print grid centered */
    ps->field = subwin(stdscr, ROWS + 2, COLS + 2, (ps->rowtot - 6 - ROWS) / 2, (ps->coltot - COLS - 2) / 2);
    ps->score = subwin(stdscr, 2 + 2, ps->coltot, ps->rowtot - 4, 0);
    keypad(ps->field, TRUE);
    wtimeout(ps->field, 30);
    wborder(ps->field, '|', '|', '-', '-', '+', '+', '+', '+');
    wborder(ps->score, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(ps->score, 2, 1, "F2 anytime to *rage* quit. Arrow keys to move.");
    mvwprintw(ps->score, 1, 1, "%d points.", ps->points);
    mvwprintw(ps->field, 0, 0, "Snake");
    mvwprintw(ps->score, 0, 0, "Score");
    wrefresh(ps->score);
    return 0;
}

static void screen_end(struct state *ps)
{
    char exitmsg[] = "Leaving...bye! See you later :)";
    wclear(ps->field);
    wclear(ps->score);
    delwin(ps->field);
    delwin(ps->score);
    attron(COLOR_PAIR(rand()%6 + 1));
    attron(A_BOLD);
    if (ps->lose)
        mvprintw(ps->rowtot / 2, (ps->coltot - strlen("You scored %d points!")) / 2, "You scored %d points!", ps->points);
    else
        mvprintw(ps->rowtot / 2, (ps->coltot - strlen(exitmsg)) / 2, "%s", exitmsg);
    refresh();
    sleep(1);
    attroff(COLOR_PAIR);
    attroff(A_BOLD);
    endwin();
}

static snake *reclist(int i, snake *previous, struct state *ps)
{
    snake *s = malloc(sizeof(snake));
    if ((s) && (i != STARTING_SIZE)) {
        s->x = ROWS/2;
        s->y = COLS/2 - i;
        s->direction = RIGHT;
        ps->grid[s->x][s->y] = 'O';
        wattron(ps->field, COLOR_PAIR(2));
        mvwprintw(ps->field, s->x + 1, s->y + 1, "%c", ps->grid[s->x][s->y]);
        wattroff(ps->field, COLOR_PAIR);
        s->previous = previous;
        s->next = reclist(i + 1, s, ps);
    } else {
        s = NULL;
    }
    return s;
}

static void freelist(snake *s)
{
    if (s->next)
        freelist(s->next);
    free(s);
    printf("Freed\n");
}

static void fruit_gen(struct state *ps)
{
    int dim = (ROWS * COLS) - size(ps->s);
    int fixed_grid[dim];
    int i, k;
    int j = 0;
    if (dim == 0) {
        ps->lose = 1;
        return;
    }
    for (i = 0; i < ROWS; i++) {
        for (k = 0; k < COLS; k++) {
            if (ps->grid[i][k] != 'O') {
                fixed_grid[j] = (i * COLS) + k;
                j++;
            }
        }
    }
    j = rand() % dim;
    i = fixed_grid[j] / COLS;
    k = fixed_grid[j] - (i * COLS);
    ps->grid[i][k] = '*';
    wattron(ps->field, COLOR_PAIR(1));
    mvwprintw(ps->field, i + 1, k + 1, "%c", ps->grid[i][k]);
    wattroff(ps->field, COLOR_PAIR);
}

static void grid_init(struct state *ps)
{
    snake *temp = NULL;
    ps->s = reclist(0, NULL, ps);
    for (temp = ps->s; temp->next; temp = temp->next);
    /* Circular list: first elem has a ptr to last elem */
    ps->s->previous = temp;
    fruit_gen(ps);
}

static void snake_move(struct state *ps)
{
    int i, k, eat = 0;
    snake *temp = NULL;
    for (temp = ps->s; temp; temp = temp->next) {
        ps->grid[temp->x][temp->y] = ' ';
        mvwprintw(ps->field, temp->x + 1, temp->y + 1, "%c", ps->grid[temp->x][temp->y]);
        if ((eat) && (!temp->next)) {
            i = temp->x;
            k = temp->y;
        }
        switch (temp->direction) {
            case RIGHT:
                temp->y++;
                if (temp->y == COLS)
                    temp->y = 0;
                break;
            case LEFT:
                temp->y--;
                if (temp->y == -1)
                    temp->y = COLS - 1;
                break;
            case UP:
                temp->x--;
                if (temp->x == -1)
                    temp->x = ROWS - 1;
                break;
            case DOWN:
                temp->x++;
                if (temp->x == ROWS)
                    temp->x = 0;
                break;
        }
        if ((temp == ps->s) && (ps->grid[temp->x][temp->y] == '*'))
            eat = 1;
        if (ps->grid[temp->x][temp->y] == 'O') {
            ps->lose = 1;
            return;
        }
        ps->grid[temp->x][temp->y] = 'O';
        wattron(ps->field, COLOR_PAIR(2));
        mvwprintw(ps->field, temp->x + 1, temp->y + 1, "%c", ps->grid[temp->x][temp->y]);
        wattroff(ps->field, COLOR_PAIR);
    }
    if (eat) {
        eat_fruit(i, k, ps);
        mvwprintw(ps->score, 1, 1, "%d points.", ps->points);
        wrefresh(ps->score);
    }
}

static void change_directions(int direction, struct state *ps)
{
    snake *temp = NULL;
    temp = ps->s->previous;
    while (temp->previous != ps->s->previous) {
        temp->direction = temp->previous->direction;
        temp = temp->previous;
    }
    ps->s->direction = direction;
    snake_move(ps);
}

static int main_cycle(struct state *ps)
{
    int direction = ps->s->direction;
    wmove(ps->field, ps->s->x + 1, ps->s->y + 1);
    wrefresh(ps->field);
    switch (wgetch(ps->field)) {
        case KEY_LEFT:
            if (direction != RIGHT)
                direction = LEFT;
            break;
        case KEY_RIGHT:
            if (direction != LEFT)
                direction = RIGHT;
            break;
        case KEY_UP:
            if (direction != DOWN)
                direction = UP;
            break;
        case KEY_DOWN:
            if (direction != UP)
                direction = DOWN;
            break;
        case KEY_F(2): /* f2 to exit */
            return 0;
    }
    change_directions(direction, ps);
    return 1;
}

static void eat_fruit(int x, int y, struct state *ps)
{
    ps->points = ps->points + 7;
    ps->s = snake_grow(x, y, ps);
    fruit_gen(ps);
}

static snake *snake_grow(int x, int y, struct state *ps)
{
    snake *temp = NULL;
    temp = ps->s->previous;
    temp->next = malloc(sizeof(snake));
    if (temp->next) {
        temp->next->x = x;
        temp->next->y = y;
        temp->next->previous = temp;
        temp->next->direction = temp->direction;
        ps->grid[temp->next->x][temp->next->y] = 'O';
        wattron(ps->field, COLOR_PAIR(2));
        mvwprintw(ps->field, temp->next->x + 1, temp->next->y + 1, "%c", ps->grid[temp->next->x][temp->next->y]);
        wattroff(ps->field, COLOR_PAIR);
        temp->next->next = NULL;
        ps->s->previous = temp->next;
    } else {
        temp->next = NULL;
    }
    return ps->s;
}

static int size(snake *s)
{
    snake *temp = NULL;
    int i = 0;
    for (temp = s; temp->next; temp = temp->next)
        i++;
    return i;
}
