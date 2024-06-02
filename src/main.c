#include "gt/banking.h"
#include "gt/feature/text/text.h"
#include "gt/gametank.h"
#include "gt/drawing_funcs.h"
#include "gt/input.h"

#include "common.h"
#include "entities/player.h"
#include "entities/hblockgroup.h"
#include "entities/loopboy.h"
#include "tilemap.h"

#include "levels/level_one.h"
#include "levels/level_two.h"
#include "levels/level_gg.h"

EntityKind entities[ENTITY_TABLE_SIZE];
PlayerData *player_data;
EntityData entity_data[ENTITY_TABLE_SIZE];
unsigned char tilemap[TILEMAP_SIZE];
unsigned char tilemap_decor[64];

unsigned char current_level = 0;
unsigned short fail_count = 0;

unsigned char needs_draw_fail_count = 2;

#define MAX_DEATH_FREEZE 12
unsigned char death_freeze = 0;

LevelData levels[3];

void noop(char ix) {
  return;
}

CollisionResult noop_collision(char ix) {
  return ResultOk;
}

void clear_entities() {
  char i;

  for (i = 0; i < ENTITY_TABLE_SIZE; i++) {
    entities[i] = EntityEmpty;
  }
}

void init_entities(const unsigned char *data) {
  clear_entities();

  while(*data != EntityEmpty) {
    switch (*data) {
      case EntityPlayer:
        init_player(*(++data), *(++data));
        break;
      case EntityHBlockGroup:
        init_hblockgroup(*(++data), *(++data), *(++data), *(++data));
        break;
      case EntityLoopBoyH:
        init_loopboy(*(++data), *(++data), *(++data), *(++data));
        break;
      default:
        // We shouldn't ever hit this branch if our levels are crafted correctly
        // Just hard-lock
        while (1) {}
    }
    data++;
  }
}

void draw_level_name(char *name) {
    init_text();
    text_cursor_x = 1;
    text_cursor_y = 7;
    text_use_alt_color = 1;
    print_text(name);

    flip_pages();

    init_text();
    text_cursor_x = 1;
    text_cursor_y = 7;
    text_use_alt_color = 1;
    print_text(name);
}

unsigned short inc_bcd(unsigned short n) {
  if ((n & 0x000F) != 0x0009) return n + 0x0001;
  if ((n & 0x00F0) != 0x0090) return (n + 0x0010) & 0xFFF0;
  if ((n & 0x0F00) != 0x0900) return (n + 0x0100) & 0xFF00;
  if ((n & 0xF000) != 0x9000) return n + 0x1000 & 0xF000;

  return 0x9999;
}

void print_bcd(unsigned short n) {
  char i;
  bool has_printed = false;

  if (n == 0) {
    print_char('0');
    return;
  }

  i = n >> 12;
  if (i != 0) {
    print_char('0' + i);
    has_printed = true;
  }
  i = (n >> 8) & 15;
  if (i != 0 || has_printed) {
    print_char('0' + i);
    has_printed = true;
  }
  i = (n >> 4) & 15;
  if (i != 0 || has_printed) {
    print_char('0' + i);
    has_printed = true;
  }
  i = n & 15;
  if (i != 0 || has_printed) {
    print_char('0' + i);
    has_printed = true;
  }
}

void draw_fail_count() {
  draw_box(1, 16, 126, 8, 0);
  await_draw_queue();
  init_text();
  text_cursor_x = 1;
  text_cursor_y = 16;
  text_use_alt_color = 1;
  print_text("Fails ");
  print_bcd(fail_count);
}

void init_level() {
  LevelData l;
  l = levels[current_level];

  init_tilemap(l.tilemap, l.tilemap_decor);
  draw_tilemap_full();

  draw_level_name(l.name);

  init_entities(l.entities);
}

void reset_level() {
  LevelData *l;
  l = &levels[current_level];

  reset_player(l->reset_data[0], l->reset_data[1]);
}

void (*const drawing_fns[])(char) = {
  noop,
  draw_player,
  draw_hblockgroup,
  draw_loopboy,
};

CollisionResult (*const test_collision[])(char) = {
  noop_collision,
  update_player,
  collision_hblockgroup,
  collision_loopboy,
};

void (*const update_fns[])(char) = {
  noop,
  noop,
  update_hblockgroup,
  update_loopboy,
};

int main() {
  char i;

  init_graphics();
  load_font(0);

  change_rom_bank(BANK_PROG0);

  flip_pages();
  await_draw_queue();
  flip_pages();
  await_draw_queue();

  // Level set up
  levels[0].tilemap = level_one;
  levels[0].tilemap_decor = level_one_decor;
  levels[0].entities = level_one_entities;
  levels[0].reset_data = level_one_reset_data;
  levels[0].name = level_one_name;

  levels[1].tilemap = level_two;
  levels[1].tilemap_decor = level_two_decor;
  levels[1].entities = level_two_entities;
  levels[1].reset_data = level_two_reset_data;
  levels[1].name = level_two_name;

  levels[2].tilemap = level_gg;
  levels[2].tilemap_decor = level_gg_decor;
  levels[2].entities = level_gg_entities;
  levels[2].reset_data = level_gg_reset_data;
  levels[2].name = level_gg_name;


init_new_level:
  init_level();

  // Run forever
main_loop:
  while (1) {
    PROFILER_START(1);
    update_inputs();

    if (death_freeze == 0) {
      for (i = 0; i < ENTITY_TABLE_SIZE; i++) {
        switch (test_collision[entities[i]](i)) {
          case ResultFail:
            death_freeze = MAX_DEATH_FREEZE;
            goto main_loop;
          case ResultWin:
            current_level++;
            needs_draw_fail_count = 2;
            goto init_new_level;
        }
      }

      for (i = 0; i < ENTITY_TABLE_SIZE; i++) {
        update_fns[entities[i]](i);
      }
    } else {
      death_freeze--;

      if (death_freeze == 0) {
        fail_count = inc_bcd(fail_count);
        reset_level();
        needs_draw_fail_count = 2;
      }
    }

    draw_tilemap_partial();

    if (needs_draw_fail_count > 0) {
          draw_fail_count();
          needs_draw_fail_count--;
    }

    for (i = 0; i < ENTITY_TABLE_SIZE; i++) {
      // Draw entities in reverse order such that players are drawn on top of other entities
      drawing_fns[entities[ENTITY_TABLE_SIZE - 1 - i]](ENTITY_TABLE_SIZE - 1 - i);
    }

    await_draw_queue();
    PROFILER_END(1);
    sleep(1);
    flip_pages();
  }

  //  We should never get here!
  return 0;
}
