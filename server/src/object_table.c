#include "object_table.h"
#include "dbg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <animate/animate.h>

CanvasTable    CANVAS_TABLE;
SpriteTable    SPRITE_TABLE;
PlacementTable PLACEMENT_TABLE;

static void *grow_array(void *old, int elem_size, int *capacity)
{
    int   newcap = (*capacity == 0) ? INITIAL_CAPACITY : (*capacity * 2);
    void *newptr = realloc(old, newcap * elem_size);
    if (!newptr) {
        return NULL;
    }

    *capacity = newcap;
    return newptr;
}

void tables_init()
{
    CANVAS_TABLE.arr      = NULL;
    CANVAS_TABLE.count    = 0;
    CANVAS_TABLE.capacity = 0;

    SPRITE_TABLE.arr      = NULL;
    SPRITE_TABLE.count    = 0;
    SPRITE_TABLE.capacity = 0;

    PLACEMENT_TABLE.arr      = NULL;
    PLACEMENT_TABLE.count    = 0;
    PLACEMENT_TABLE.capacity = 0;
}

void tables_destroy()
{
    free(CANVAS_TABLE.arr);
    free(SPRITE_TABLE.arr);
    free(PLACEMENT_TABLE.arr);
}

/* INSERT HELPERS */
int canvas_insert(struct canvas *ptr, ActiveClient *owner)
{
    if (CANVAS_TABLE.count == CANVAS_TABLE.capacity) {
        CANVAS_TABLE.arr = grow_array(
            CANVAS_TABLE.arr, sizeof(CanvasEntry), &CANVAS_TABLE.capacity);
        if (!CANVAS_TABLE.arr) {
            debug_log("Canvas grow fail");
            return -1;
        }
    }

    int          id = CANVAS_TABLE.count;
    CanvasEntry *e  = &CANVAS_TABLE.arr[id];

    e->ptr          = ptr;
    e->canvas_id    = id;
    e->owner        = owner;
    e->shared_count = 0;

    // clean garbage
    memset(e->shared, 0, sizeof(e->shared));
    memset(e->barrier_pending, 0, sizeof(e->barrier_pending));

    e->barrier_waiting = 0;

    CANVAS_TABLE.count++;
    return id;
}

int sprite_insert(struct sprite *ptr, ActiveClient *owner)
{
    if (SPRITE_TABLE.count == SPRITE_TABLE.capacity) {
        SPRITE_TABLE.arr = grow_array(
            SPRITE_TABLE.arr, sizeof(SpriteEntry), &SPRITE_TABLE.capacity);
        if (!SPRITE_TABLE.arr) {
            debug_log("Sprite grow fail");
            return -1;
        }
    }

    int          id = SPRITE_TABLE.count;
    SpriteEntry *e  = &SPRITE_TABLE.arr[id];

    e->ptr       = ptr;
    e->sprite_id = id;
    e->owner     = owner;

    SPRITE_TABLE.count++;
    return id;
}

int placement_insert(struct sprite_placement *ptr,
                     ActiveClient            *owner,
                     CanvasEntry             *canvas)
{
    if (PLACEMENT_TABLE.count == PLACEMENT_TABLE.capacity) {
        PLACEMENT_TABLE.arr = grow_array(PLACEMENT_TABLE.arr,
                                         sizeof(PlacementEntry),
                                         &PLACEMENT_TABLE.capacity);
        if (!PLACEMENT_TABLE.arr) {
            debug_log("Placement grow fail");
            return -1;
        }
    }

    int             id = PLACEMENT_TABLE.count;
    PlacementEntry *e  = &PLACEMENT_TABLE.arr[id];

    e->ptr          = ptr;
    e->placement_id = id;
    e->owner        = owner;
    e->canvas       = canvas;

    PLACEMENT_TABLE.count++;
    return id;
}

/* LOOKUP */
CanvasEntry *canvas_lookup(int id)
{
    if (id < 0 || id >= CANVAS_TABLE.count)
        return NULL;
    return &CANVAS_TABLE.arr[id];
}

SpriteEntry *sprite_lookup(int id)
{
    if (id < 0 || id >= SPRITE_TABLE.count)
        return NULL;
    return &SPRITE_TABLE.arr[id];
}

PlacementEntry *placement_lookup(int id)
{
    if (id < 0 || id >= PLACEMENT_TABLE.count)
        return NULL;
    return &PLACEMENT_TABLE.arr[id];
}

/* REMOVE  */
void canvas_remove(int id)
{
    CanvasEntry *e = canvas_lookup(id);
    if (!e)
        return;
    animate_destroy_canvas(e->ptr);
    e->ptr = NULL;
}

int sprite_remove(int id)
{
    SpriteEntry *e = sprite_lookup(id);
    if (!e)
        return -1;
    int ret = animate_destroy_sprite(e->ptr);
    e->ptr  = NULL;
    return ret;
}

void placement_remove(int id)
{
    PlacementEntry *e = placement_lookup(id);
    if (!e)
        return;
    animate_destroy_placement(e->ptr);
    e->ptr = NULL;
}
