/*
 *      An interactive Conway's Game of Life simulator for Game Boy.
 *      Written by David E. Shere <david.e.shere@gmail.com> in 2024.
*/

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>
#include "tiles.h"
#include "tilemap.h"
#include "cursor.h"

#define TILE_SIZE 8         // Tile width in pixels.
#define SPRITE_CURSOR 0     // Cursor's sprite tile ID.

// Needed to align the cursor with the cells.
#define CURSOR_OFFSET_X 1
#define CURSOR_OFFSET_Y 2

// Width and height of the game world in tiles.
#define WIDTH 20
#define HEIGHT 18

// Save data locations
#define SAVED ((char*)0xa000);
#define SAVED_STATES ((char*)0xa001)

// Represents a single cell of the Life game.
// bool *state - points to the cell's live/dead state.
// struct Cell* neighbors[8] - array of pointers to all neighboring cells.
typedef struct Cell {
    bool *state;
    struct Cell* neighbors[8];
} Cell;


// Represents the player's cursor
// uint8_t sprite - sprite ID for the cursor
// uint8_t x - x position in the game world
// uint8_t y - y position in the game world
typedef struct Cursor {
    uint8_t sprite;
    uint8_t x;
    uint8_t y;
} Cursor;

// Calculates the number of living cells neighboring a cell.
uint8_t cell_neighbors(Cell *c) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < 8; i++)
        n += *(c->neighbors[i])->state ? 1 : 0;
    return n;
}

// Moves the cursor by dx and dy from current position.
void move_cursor(Cursor *cursor, int8_t dx, int8_t dy) {
    if (dy == -1)
        cursor->y = cursor->y + dy < HEIGHT ?  cursor->y + dy : HEIGHT + dy;
    else if (dy == 1)
        cursor->y = cursor->y + dy < HEIGHT ?  cursor->y + dy : 0;
    if (dx == -1)
        cursor->x = cursor->x + dx < WIDTH  ?  cursor->x + dx : WIDTH + dx;
    else if (dx == 1)
        cursor->x = cursor->x + dx < WIDTH  ?  cursor->x + dx : 0;
    move_sprite( cursor->sprite
               , TILE_SIZE * (cursor->x + CURSOR_OFFSET_X)
               , TILE_SIZE * (cursor->y + CURSOR_OFFSET_Y));
}





void main(void) {
    Cell world[WIDTH][HEIGHT];          // Our set of cells for the game.
    uint8_t neighbors[WIDTH][HEIGHT];   // Used to store live neighbor counts.
    uint8_t cell_states[WIDTH][HEIGHT]; // The live/dead states of cells.
    
    // Enable save access while initializing cells.
    ENABLE_RAM_MBC1;
    char *saved = SAVED;
    bool load_saved = saved[0] == 's' ? true : false; // Check for a save.
    char *s = SAVED_STATES;

    // Initialize the cells in the world.
    for (int8_t x = 0; x < WIDTH; x++) {
    for (int8_t y = 0; y < HEIGHT; y++) {
        int8_t n = 0;
        for (int8_t dx = -1; dx <= 1; dx++) {
        for (int8_t dy = -1; dy <= 1; dy++) {
            // Add pointers to neighboring cells to each cell.
            if (dx != 0 || dy != 0) {
                int8_t i = x+dx;
                int8_t j = y+dy;
                if (x + dx == -1) i = WIDTH-1;
                else if (x + dx == WIDTH) i = 0;
                if (y + dy == -1) j = HEIGHT-1;
                else if (y + dy == HEIGHT) j = 0;
                world[x][y].neighbors[n++] = &world[i][j];
            }
        }}
        // Point each cell to its state.
        world[x][y].state = &cell_states[x][y];
        // Load cell state from saved data if available.
        if (load_saved) {
            *world[x][y].state = *s++ == 'L' ? true : false;
        } else {
            *world[x][y].state = false;
        }
    }}
    DISABLE_RAM_MBC1;

    // Load the background data and initial tile map.
    set_bkg_data(0,8*8, TILES);
    set_bkg_tiles(0,0,WIDTH,HEIGHT, TILEMAP);

    // Update the tiles based on cell states.
    for (int x = 0; x < WIDTH; x++) {
    for (int y = 0; y < HEIGHT; y++) {
        set_tile_xy(x,y, *world[x][y].state ? LIVE : DEAD);
    }}

    // Show the cells.
    SHOW_BKG;
    DISPLAY_ON;

    // Set up and show the cursor.
    SPRITES_8x8;
    Cursor cursor;
    set_sprite_data(0, 1, CURSOR);
    set_sprite_tile(0,0);
    cursor.x = WIDTH/2-1;
    cursor.y = HEIGHT/2-1;
    cursor.sprite = SPRITE_CURSOR;
    move_cursor(&cursor,0,0);
    SHOW_SPRITES;


    // Keeps track of whether the simulation is running or paused.
    bool running = false;

    // Used to keep track of button states
    uint8_t joypadPrev = 0;
    uint8_t joypadCurr = 0;

    while(1) {
        // Update button states.
        joypadPrev = joypadCurr;
        joypadCurr = joypad();

        // If the simulation is paused, wait for button presses.
        if (!running) {

            // Start simulation on B.
            if ((joypadCurr & J_B) && !(joypadPrev & J_B)) {
                running = !running;
                HIDE_SPRITES;
                continue;
            }

            // Clear all cell states on SELECT.
            if ((joypadCurr & J_SELECT) && !(joypadPrev & J_SELECT)) {
                set_bkg_tiles(0,0,WIDTH,HEIGHT, TILEMAP);
                for (uint8_t x = 0; x < WIDTH; x++)
                for (uint8_t y = 0; y < HEIGHT; y++)
                    cell_states[x][y] = false;
            }

            // Save the cell states on START.
            if ((joypadCurr & J_START) && !(joypadPrev & J_START)) {
                ENABLE_RAM_MBC1;
                *saved = 's';
                char *s = SAVED_STATES;
                for (uint8_t x = 0; x < WIDTH; x++) {
                for (uint8_t y = 0; y < HEIGHT; y++) {
                    *s++ = cell_states[x][y] ? 'L' : 'D';
                }}
                DISABLE_RAM_MBC1;
                set_tile_xy(0,0,3);
            }

            // Toggle a cell if A is pressed.
            if ((joypadCurr & J_A) && !(joypadPrev & J_A)) {
                Cell *c = &world[cursor.x][cursor.y];
                *c->state = !(*c->state);
                set_tile_xy(cursor.x,cursor.y, *c->state ? LIVE : DEAD);
            }
            
            // DPAD to move cursor.
            if ((joypadCurr & J_UP) && !(joypadPrev & J_UP)) {
                move_cursor(&cursor, 0, -1);
            }
            if ((joypadCurr & J_DOWN) && !(joypadPrev & J_DOWN)) {
                move_cursor(&cursor, 0, 1);
            }
            if ((joypadCurr & J_LEFT) && !(joypadPrev & J_LEFT)) {
                move_cursor(&cursor, -1, 0);
            }
            if ((joypadCurr & J_RIGHT) && !(joypadPrev & J_RIGHT)) {
                move_cursor(&cursor, 1, 0);
            }
        } else { // If we aren't paused...

            // Pause on B.
            if ((joypadCurr & J_B) && !(joypadPrev & J_B)) {
                running = !running;
                SHOW_SPRITES;
                continue;
            }

            // ...otherwise calculate neighbor states for all cells...
            for (uint8_t x = 0; x < WIDTH; x++) {
            for (uint8_t y = 0; y < HEIGHT; y++) {
                Cell *c = &world[x][y];
                neighbors[x][y] = cell_neighbors(c);
            }}

            // ...and change cell states for a new generation...
            for (uint8_t x = 0; x < WIDTH; x++) {
            for (uint8_t y = 0; y < HEIGHT; y++) {
                uint8_t n = neighbors[x][y];
                if (*world[x][y].state && n != 2 && n != 3) {
                    *world[x][y].state = false;
                } else if (!*world[x][y].state && n == 3)
                    *world[x][y].state = true;
                set_tile_xy(x, y, *world[x][y].state ? LIVE : DEAD);
            }}
        }

        vsync();
    }
}

