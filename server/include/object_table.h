#ifndef OBJECT_TABLE_H
#define OBJECT_TABLE_H

// #include "animate.h"
#include "client_registry.h"
#include <animate/animate.h>
#include <stdbool.h>

#define MAX_CLIENTS 64
#define INITIAL_CAPACITY 64

typedef struct CanvasEntry {
    struct canvas *ptr;
    int            canvas_id;

    int width;
    int height;

    ActiveClient *owner;
    ActiveClient *shared[MAX_CLIENTS];
    int           shared_count;

    bool barrier_pending[MAX_CLIENTS];
    int  barrier_waiting;
} CanvasEntry;

typedef struct SpriteEntry {
    struct sprite *ptr;
    int            sprite_id;

    ActiveClient *owner;
} SpriteEntry;

typedef struct PlacementEntry {
    struct sprite_placement *ptr;
    int                      placement_id;

    ActiveClient *owner;
    CanvasEntry  *canvas;
} PlacementEntry;

/* TABLE STRUCTS */
typedef struct {
    CanvasEntry *arr;
    int          count;
    int          capacity;
} CanvasTable;

typedef struct {
    SpriteEntry *arr;
    int          count;
    int          capacity;
} SpriteTable;

typedef struct {
    PlacementEntry *arr;
    int             count;
    int             capacity;
} PlacementTable;

/* GLOBAL TABLES */
extern CanvasTable    CANVAS_TABLE;
extern SpriteTable    SPRITE_TABLE;
extern PlacementTable PLACEMENT_TABLE;

/* API */
void tables_init();
void tables_destroy();

/* Insert */
int canvas_insert(struct canvas *ptr,
                  ActiveClient  *owner,
                  int            width,
                  int            height);
int sprite_insert(struct sprite *ptr, ActiveClient *owner);
int placement_insert(struct sprite_placement *ptr,
                     ActiveClient            *owner,
                     CanvasEntry             *canvas);

/* Lookup */
CanvasEntry    *canvas_lookup(int id);
SpriteEntry    *sprite_lookup(int id);
PlacementEntry *placement_lookup(int id);

/* Remove */
void canvas_remove(int id);
int  sprite_remove(int id);
void placement_remove(int id);

#endif
