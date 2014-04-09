/*
 * Simple Sokoban -- a (simple) Sokoban game
 * Copyright (C) 2014 Mateusz Viste
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>    /* malloc() */
#include <string.h>    /* memcpy() */
#include <time.h>
#include <SDL2/SDL.h>                 /* SDL       */
#include <SDL2/SDL_image.h>           /* SDL_image */
#include "sok_core.h"
#include "data_lev.h"                 /* embedded image files */
#include "data_img.h"                 /* embedded level files */
#include "data_fnt.h"                 /* embedded level files */

#define PVER "v1.0 alpha"

#define MAXLEVELS 1024
#define SCREEN_DEFAULT_WIDTH 800
#define SCREEN_DEFAULT_HEIGHT 600

#define DISPLAYCENTERED 1
#define DISPLAYFADEIN 2
#define DISPLAYFADEOUT 4


struct spritesstruct {
  SDL_Texture *atom;
  SDL_Texture *atom_on_goal;
  SDL_Texture *bg;
  SDL_Texture *cleared;
  SDL_Texture *floor;
  SDL_Texture *goal;
  SDL_Texture *help;
  SDL_Texture *intro;
  SDL_Texture *player;
  SDL_Texture *walls[16];
  SDL_Texture *font[64];
};

/* returns the absolute value of the 'i' integer. */
static int absval(int i) {
  if (i < 0) return(-i);
  return(i);
}

static int char2fontid(char c) {
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'a') && (c <= 'z')) return(10 + c - 'a');
  if ((c >= 'A') && (c <= 'Z')) return(10 + c - 'A');
  return(0);
}

static int getoffseth(struct sokgame *game, int winw, int tilesize) {
  /* if playfield is smaller than the screen */
  if (game->field_width * tilesize <= winw) return((winw / 2) - (game->field_width * tilesize / 2));
  /* if playfield is larger than the screen */
  if (game->positionx * tilesize + (tilesize / 2) > (winw / 2)) {
    int res = (winw / 2) - (game->positionx * tilesize + (tilesize / 2));
    if ((game->field_width * tilesize) + res < winw) res = winw - (game->field_width * tilesize);
    return(res);
  }
  return(0);
}

static int getoffsetv(struct sokgame *game, int winh, int tilesize) {
  /* if playfield is smaller than the screen */
  if (game->field_height * tilesize <= winh) return((winh / 2) - (game->field_height * tilesize / 2));
  /* if playfield is larger than the screen */
  if (game->positiony * tilesize > winh / 2) {
    int res = (winh / 2) - (game->positiony * tilesize + (tilesize / 2));
    if ((game->field_height * tilesize) + res < winh) res = winh - (game->field_height * tilesize);
    return(res);
  }
  return(0);
}

static void flush_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event) != 0);
}

/* wait for a key up to timeout seconds (0 = indefintely), while redrawing the renderer screen, if not null */
static int wait_for_a_key(int timeout, SDL_Renderer *renderer) {
  SDL_Event event;
  time_t timeouttime;
  timeouttime = time(NULL) + timeout;
  for (;;) {
    SDL_Delay(50);
    if (SDL_PollEvent(&event) != 0) {
      if (renderer != NULL) SDL_RenderPresent(renderer);
      if (event.type == SDL_QUIT) {
          return(1);
        } else if (event.type == SDL_KEYDOWN) {
          if (event.key.keysym.sym == SDLK_ESCAPE) return(1);
          return(0);
      }
    }
    if (timeout > 0) {
      if (time(NULL) >= timeouttime) return(0);
    }
  }
}

/* display a bitmap onscreen */
static int displaytexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Window *window, int timeout, int flags) {
  int x, winw, winh;
  SDL_Rect rect, *rectptr;
  SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
  SDL_GetWindowSize(window, &winw, &winh);
  if (flags & DISPLAYCENTERED) {
      rectptr = &rect;
    } else {
      rectptr = NULL;
  }
  rect.x = (winw - rect.w) / 2;
  rect.y = (winh - rect.h) / 2;
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  if (flags & DISPLAYFADEIN) {
    for (x = 0; x < 150 ; x += 3) {
      SDL_SetTextureAlphaMod(texture, x);
      SDL_RenderCopy(renderer, texture, NULL, rectptr);
      SDL_RenderPresent(renderer);
      SDL_Delay(20);
    }
  }
  SDL_RenderCopy(renderer, texture, NULL, rectptr);
  SDL_RenderPresent(renderer);
  return(wait_for_a_key(timeout, renderer));
}

static void draw_string(char *string, struct spritesstruct *sprites, SDL_Renderer *renderer, int x, int y) {
  int i;
  SDL_Texture *glyph;
  SDL_Rect rectsrc, rectdst;
  rectdst.x = x;
  rectdst.y = y;
  for (i = 0; string[i] != 0; i++) {
    if (string[i] == ' ') {
      rectdst.x += 20;
      continue;
    }
    glyph = sprites->font[char2fontid(string[i])];
    SDL_QueryTexture(glyph, NULL, NULL, &rectsrc.w, &rectsrc.h);
    rectdst.w = rectsrc.w;
    rectdst.h = rectsrc.h;
    SDL_RenderCopy(renderer, glyph, NULL, &rectdst);
    rectdst.x += rectsrc.w;
  }
}

/* get an 'id' for a wall on a given position. this is a 4-bits bitfield that indicates where the wall has neighbors (up/right/down/left). */
static int getwallid(struct sokgame *game, int x, int y) {
  int res = 0;
  if ((y > 0) && (game->field[x][y - 1] & field_wall)) res |= 1;
  if ((x < 63) && (game->field[x + 1][y] & field_wall)) res |= 2;
  if ((y < 63) && (game->field[x][y + 1] & field_wall)) res |= 4;
  if ((x > 0) && (game->field[x - 1][y] & field_wall)) res |= 8;
  return(res);
}

static void draw_playfield_tile(struct sokgame *game, int x, int y, struct spritesstruct *sprites, SDL_Renderer *renderer, int winw, int winh, int tilesize, int drawatom, int moveoffsetx, int moveoffsety) {
  SDL_Rect rect;
  /* compute the dst rect */
  rect.x = getoffseth(game, winw, tilesize) + (x * tilesize) + moveoffsetx;
  rect.y = getoffsetv(game, winh, tilesize) + (y * tilesize) + moveoffsety;
  rect.w = tilesize;
  rect.h = tilesize;

  if (drawatom == 0) {
      if (game->field[x][y] & field_floor) SDL_RenderCopy(renderer, sprites->floor, NULL, &rect);
      if (game->field[x][y] & field_goal) SDL_RenderCopy(renderer, sprites->goal, NULL, &rect);
      if (game->field[x][y] & field_wall) SDL_RenderCopy(renderer, sprites->walls[getwallid(game, x, y)], NULL, &rect);
    } else {
      int atomongoal = 0;
      if ((game->field[x][y] & field_goal) && (game->field[x][y] & field_atom)) {
        atomongoal = 1;
        if ((moveoffsetx > 0) && ((game->field[x + 1][y] & field_goal) == 0)) atomongoal = 0;
        if ((moveoffsetx < 0) && ((game->field[x - 1][y] & field_goal) == 0)) atomongoal = 0;
        if ((moveoffsety > 0) && ((game->field[x][y + 1] & field_goal) == 0)) atomongoal = 0;
        if ((moveoffsety < 0) && ((game->field[x][y - 1] & field_goal) == 0)) atomongoal = 0;
      }
      if (atomongoal != 0) {
          SDL_RenderCopy(renderer, sprites->atom_on_goal, NULL, &rect);
        } else if (game->field[x][y] & field_atom) {
          SDL_RenderCopy(renderer, sprites->atom, NULL, &rect);
      }
  }
}

static void draw_player(struct sokgame *game, struct sokgamestates *states, struct spritesstruct *sprites, SDL_Renderer *renderer, int winw, int winh, int tilesize, int offsetx, int offsety) {
  SDL_Rect rect;
  /* compute the dst rect */
  rect.x = getoffseth(game, winw, tilesize) + (game->positionx * tilesize) + offsetx;
  rect.y = getoffsetv(game, winh, tilesize) + (game->positiony * tilesize) + offsety;
  rect.w = tilesize;
  rect.h = tilesize;
  SDL_RenderCopyEx(renderer, sprites->player, NULL, &rect, states->angle, NULL, SDL_FLIP_NONE);
}

/* loads a graphic and returns its width, or -1 on error */
static int loadGraphic(SDL_Texture **texture, SDL_Renderer *renderer, void *memptr, int memlen) {
  int res;
  SDL_Surface *surface;
  SDL_RWops *rwop;
  rwop = SDL_RWFromMem(memptr, memlen);
  surface = IMG_LoadPNG_RW(rwop);
  SDL_FreeRW(rwop);
  if (surface == NULL) return(-1);
  res = surface->w;
  *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  return(res);
}

static void draw_screen(struct sokgame *game, struct sokgamestates *states, struct spritesstruct *sprites, SDL_Renderer *renderer, SDL_Window *window, int tilesize, int moveoffsetx, int moveoffsety, int scrolling) {
  int x, y, winw, winh, offx, offy;
  /* int partialoffsetx = 0, partialoffsety = 0; */
  char stringbuff[256];
  int scrollingadjx = 0, scrollingadjy = 0; /* this is used when scrolling + movement of player is needed */
  SDL_GetWindowSize(window, &winw, &winh);
  SDL_RenderCopy(renderer, sprites->bg, NULL, NULL);

  if (scrolling > 0) {
    if (moveoffsetx > scrolling) {
      scrollingadjx = moveoffsetx - scrolling;
      moveoffsetx = scrolling;
    }
    if (moveoffsetx < -scrolling) {
      scrollingadjx = moveoffsetx + scrolling;
      moveoffsetx = -scrolling;
    }
    if (moveoffsety > scrolling) {
      scrollingadjy = moveoffsety - scrolling;
      moveoffsety = scrolling;
    }
    if (moveoffsety < -scrolling) {
      scrollingadjy = moveoffsety + scrolling;
      moveoffsety = -scrolling;
    }
  }
  /* draw non-moveable tiles (floors, walls, goals) */
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      if (scrolling != 0) {
          draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, 0, -moveoffsetx, -moveoffsety);
        } else {
          draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, 0, 0, 0);
      }
    }
  }
  /* draw moveable elements (atoms) */
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      offx = 0;
      offy = 0;
      if (scrolling == 0) {
          if ((moveoffsetx > 0) && (x == game->positionx + 1) && (y == game->positiony)) offx = moveoffsetx;
          if ((moveoffsetx < 0) && (x == game->positionx - 1) && (y == game->positiony)) offx = moveoffsetx;
          if ((moveoffsety > 0) && (y == game->positiony + 1) && (x == game->positionx)) offy = moveoffsety;
          if ((moveoffsety < 0) && (y == game->positiony - 1) && (x == game->positionx)) offy = moveoffsety;
        } else {
          offx = -moveoffsetx;
          offy = -moveoffsety;
          if ((moveoffsetx > 0) && (x == game->positionx + 1) && (y == game->positiony)) offx = scrollingadjx;
          if ((moveoffsetx < 0) && (x == game->positionx - 1) && (y == game->positiony)) offx = scrollingadjx;
          if ((moveoffsety > 0) && (y == game->positiony + 1) && (x == game->positionx)) offy = scrollingadjy;
          if ((moveoffsety < 0) && (y == game->positiony - 1) && (x == game->positionx)) offy = scrollingadjy;
      }
      draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, 1, offx, offy);
    }
  }
  /* draw where the player is */
  if (scrolling != 0) {
      draw_player(game, states, sprites, renderer, winw, winh, tilesize, scrollingadjx, scrollingadjy);
    } else {
      draw_player(game, states, sprites, renderer, winw, winh, tilesize, moveoffsetx, moveoffsety);
  }
  /* draw text */
  sprintf(stringbuff, "level %d", game->level);
  draw_string(stringbuff, sprites, renderer, 10, 0);
  sprintf(stringbuff, "best score 0");
  draw_string(stringbuff, sprites, renderer, 10, 25);
  sprintf(stringbuff, "moves %d", states->movescount);
  draw_string(stringbuff, sprites, renderer, 10, 50);
  /* Update the screen */
  SDL_RenderPresent(renderer);
}

static int rotatePlayer(struct spritesstruct *sprites, struct sokgame *game, struct sokgamestates *states, enum SOKMOVE dir, SDL_Renderer *renderer, SDL_Window *window, int tilesize) {
  int srcangle = states->angle;
  int dstangle = 0, dirmotion, winw, winh;
  SDL_GetWindowSize(window, &winw, &winh);
  switch (dir) {
    case sokmoveUP:
      dstangle = 0;
      break;
    case sokmoveRIGHT:
      dstangle = 90;
      break;
    case sokmoveDOWN:
      dstangle = 180;
      break;
    case sokmoveLEFT:
      dstangle = 270;
      break;
  }
  /* figure out how to compute the shortest way to rotate the player... */
  if (srcangle != dstangle) {
    int tmpangle, stepsright = 0, stepsleft = 0;
    for (tmpangle = srcangle; ; tmpangle++) {
      if (tmpangle >= 360) tmpangle = 0;
      stepsright += 1;
      if (tmpangle == dstangle) break;
    }
    for (tmpangle = srcangle; ; tmpangle--) {
      if (tmpangle < 0) tmpangle = 359;
      stepsleft += 1;
      if (tmpangle == dstangle) break;
    }
    if (stepsleft < stepsright) {
        dirmotion = -1;
      } else if (stepsright < stepsleft) {
        dirmotion = 1;
      } else {
        if (rand() % 2 == 0) {
            dirmotion = -1;
          } else {
            dirmotion = 1;
        }
    }
    /* perform the rotation */
    for (tmpangle = srcangle; ; tmpangle += dirmotion) {
      if (tmpangle >= 360) tmpangle = 0;
      if (tmpangle < 0) tmpangle = 359;
      states->angle = tmpangle;
      if (tmpangle % 10 == 0) {
        draw_screen(game, states, sprites, renderer, window, tilesize, 0, 0, 0);
        SDL_Delay(10);
      }
      if (tmpangle == dstangle) break;
    }
    return(1);
  }
  return(0);
}

static int scrollneeded(struct sokgame *game, SDL_Window *window, int tilesize, int offx, int offy) {
  int winw, winh, offsetx, offsety, result = 0;
  SDL_GetWindowSize(window, &winw, &winh);
  offsetx = absval(getoffseth(game, winw, tilesize));
  offsety = absval(getoffsetv(game, winh, tilesize));
  game->positionx += offx;
  game->positiony += offy;
  result = offsetx - absval(getoffseth(game, winw, tilesize));
  if (result == 0) result = offsety - absval(getoffsetv(game, winh, tilesize));
  if (result < 0) result = -result; /* convert to abs() value */
  game->positionx -= offx;
  game->positiony -= offy;
  return(result);
}


int main(int argc, char **argv) {
  struct sokgame **gameslist, game;
  struct sokgamestates states;
  struct spritesstruct spritesdata;
  struct spritesstruct *sprites = &spritesdata;
  int levelscount, curlevel = 0, exitflag = 0, showhelp = 1, x;
  int tilesize;
  char *levelfile = NULL;
  #define LEVCOMMENTMAXLEN 32
  char levcomment[LEVCOMMENTMAXLEN];

  SDL_Window* window = NULL;
  SDL_Renderer *renderer;
  SDL_Event event;

  /* init (seed) the randomizer */
  srand(time(NULL));

  /* Init SDL and set the video mode */
  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("Simple Sokoban " PVER, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_DEFAULT_WIDTH, SCREEN_DEFAULT_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  SDL_SetWindowMinimumSize(window, 160, 120);

  renderer = SDL_CreateRenderer(window, -1, 0);
  if (renderer == NULL) {
    SDL_DestroyWindow(window);
    printf( "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  /* Load sprites */
  loadGraphic(&sprites->atom, renderer, img_atom_png, img_atom_png_len);
  loadGraphic(&sprites->atom_on_goal, renderer, img_atom_on_goal_png, img_atom_on_goal_png_len);
  tilesize = loadGraphic(&sprites->floor, renderer, img_floor_png, img_floor_png_len);
  loadGraphic(&sprites->goal, renderer, img_goal_png, img_goal_png_len);
  loadGraphic(&sprites->player, renderer, img_player_png, img_player_png_len);
  loadGraphic(&sprites->intro, renderer, img_intro_png, img_intro_png_len);
  loadGraphic(&sprites->bg, renderer, img_bg_png, img_bg_png_len);
  loadGraphic(&sprites->cleared, renderer, img_cleared_png, img_cleared_png_len);
  loadGraphic(&sprites->help, renderer, img_help_png, img_help_png_len);
  loadGraphic(&sprites->walls[0],  renderer, img_wall0_png,  img_wall0_png_len);
  loadGraphic(&sprites->walls[1],  renderer, img_wall1_png,  img_wall1_png_len);
  loadGraphic(&sprites->walls[2],  renderer, img_wall2_png,  img_wall2_png_len);
  loadGraphic(&sprites->walls[3],  renderer, img_wall3_png,  img_wall3_png_len);
  loadGraphic(&sprites->walls[4],  renderer, img_wall4_png,  img_wall4_png_len);
  loadGraphic(&sprites->walls[5],  renderer, img_wall5_png,  img_wall5_png_len);
  loadGraphic(&sprites->walls[6],  renderer, img_wall6_png,  img_wall6_png_len);
  loadGraphic(&sprites->walls[7],  renderer, img_wall7_png,  img_wall7_png_len);
  loadGraphic(&sprites->walls[8],  renderer, img_wall8_png,  img_wall8_png_len);
  loadGraphic(&sprites->walls[9],  renderer, img_wall9_png,  img_wall9_png_len);
  loadGraphic(&sprites->walls[10], renderer, img_wall10_png, img_wall10_png_len);
  loadGraphic(&sprites->walls[11], renderer, img_wall11_png, img_wall11_png_len);
  loadGraphic(&sprites->walls[12], renderer, img_wall12_png, img_wall12_png_len);
  loadGraphic(&sprites->walls[13], renderer, img_wall13_png, img_wall13_png_len);
  loadGraphic(&sprites->walls[14], renderer, img_wall14_png, img_wall14_png_len);
  loadGraphic(&sprites->walls[15], renderer, img_wall15_png, img_wall15_png_len);

  /* load font */
  for (x = 0; x < 64; x++) sprites->font[x] = NULL;
  loadGraphic(&sprites->font[char2fontid('0')], renderer, font_0_png, font_0_png_len);
  loadGraphic(&sprites->font[char2fontid('1')], renderer, font_1_png, font_1_png_len);
  loadGraphic(&sprites->font[char2fontid('2')], renderer, font_2_png, font_2_png_len);
  loadGraphic(&sprites->font[char2fontid('3')], renderer, font_3_png, font_3_png_len);
  loadGraphic(&sprites->font[char2fontid('4')], renderer, font_4_png, font_4_png_len);
  loadGraphic(&sprites->font[char2fontid('5')], renderer, font_5_png, font_5_png_len);
  loadGraphic(&sprites->font[char2fontid('6')], renderer, font_6_png, font_6_png_len);
  loadGraphic(&sprites->font[char2fontid('7')], renderer, font_7_png, font_7_png_len);
  loadGraphic(&sprites->font[char2fontid('8')], renderer, font_8_png, font_8_png_len);
  loadGraphic(&sprites->font[char2fontid('9')], renderer, font_9_png, font_9_png_len);
  loadGraphic(&sprites->font[char2fontid('a')], renderer, font_a_png, font_a_png_len);
  loadGraphic(&sprites->font[char2fontid('b')], renderer, font_b_png, font_b_png_len);
  loadGraphic(&sprites->font[char2fontid('c')], renderer, font_c_png, font_c_png_len);
  loadGraphic(&sprites->font[char2fontid('d')], renderer, font_d_png, font_d_png_len);
  loadGraphic(&sprites->font[char2fontid('e')], renderer, font_e_png, font_e_png_len);
  loadGraphic(&sprites->font[char2fontid('f')], renderer, font_f_png, font_f_png_len);
  loadGraphic(&sprites->font[char2fontid('g')], renderer, font_g_png, font_g_png_len);
  loadGraphic(&sprites->font[char2fontid('h')], renderer, font_h_png, font_h_png_len);
  loadGraphic(&sprites->font[char2fontid('i')], renderer, font_i_png, font_i_png_len);
  loadGraphic(&sprites->font[char2fontid('j')], renderer, font_j_png, font_j_png_len);
  loadGraphic(&sprites->font[char2fontid('k')], renderer, font_k_png, font_k_png_len);
  loadGraphic(&sprites->font[char2fontid('l')], renderer, font_l_png, font_l_png_len);
  loadGraphic(&sprites->font[char2fontid('m')], renderer, font_m_png, font_m_png_len);
  loadGraphic(&sprites->font[char2fontid('n')], renderer, font_n_png, font_n_png_len);
  loadGraphic(&sprites->font[char2fontid('o')], renderer, font_o_png, font_o_png_len);
  loadGraphic(&sprites->font[char2fontid('p')], renderer, font_p_png, font_p_png_len);
  loadGraphic(&sprites->font[char2fontid('q')], renderer, font_q_png, font_q_png_len);
  loadGraphic(&sprites->font[char2fontid('r')], renderer, font_r_png, font_r_png_len);
  loadGraphic(&sprites->font[char2fontid('s')], renderer, font_s_png, font_s_png_len);
  loadGraphic(&sprites->font[char2fontid('t')], renderer, font_t_png, font_t_png_len);
  loadGraphic(&sprites->font[char2fontid('u')], renderer, font_u_png, font_u_png_len);
  loadGraphic(&sprites->font[char2fontid('v')], renderer, font_v_png, font_v_png_len);
  loadGraphic(&sprites->font[char2fontid('w')], renderer, font_w_png, font_w_png_len);
  loadGraphic(&sprites->font[char2fontid('x')], renderer, font_x_png, font_x_png_len);
  loadGraphic(&sprites->font[char2fontid('y')], renderer, font_y_png, font_y_png_len);
  loadGraphic(&sprites->font[char2fontid('z')], renderer, font_z_png, font_z_png_len);

  /* Hide the mouse cursor */
  SDL_ShowCursor(SDL_DISABLE);

  if (argc == 2) levelfile = argv[1];

  gameslist = malloc(sizeof(struct sokgame *) * MAXLEVELS);
  if (gameslist == NULL) {
    puts("Memory allocation failed!");
    return(1);
  }

  levelscount = sok_loadfile(gameslist, MAXLEVELS, levelfile, levels_microban_xsb, levcomment, LEVCOMMENTMAXLEN);
  if (levelscount < 1) {
    printf("Failed to load the level file! [%d]\n", levelscount);
    return(1);
  }

  printf("Loaded %d levels '%s'\n", levelscount, levcomment);

  /* display the intro screen, and wait for a keypress during a few seconds */
  exitflag = displaytexture(renderer, sprites->intro, window, 6, 0);

  flush_events();

  for (curlevel = 0; curlevel + 1 < levelscount; curlevel++) {
    if (gameslist[curlevel]->solution != NULL) {
        printf("Level %d has solution: %s\n", curlevel + 1, gameslist[curlevel]->solution);
      } else {
        break;
    }
  }
  memcpy(&game, gameslist[curlevel], sizeof(game));
  sok_resetstates(&states);

  if (curlevel > 0) showhelp = 0;

  for (;;) {
    draw_screen(&game, &states, sprites, renderer, window, tilesize, 0, 0, 0);
    if (showhelp != 0) {
      exitflag = displaytexture(renderer, sprites->help, window, 0, DISPLAYCENTERED);
      draw_screen(&game, &states, sprites, renderer, window, tilesize, 0, 0, 0);
      showhelp = 0;
    }
    printf("history: %s\n", states.history);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) {
      if ((SDL_WaitEvent(&event) != 0) && (event.type != SDL_KEYUP) && (event.type != SDL_MOUSEMOTION)) break;
    }

    /* check what event we got */
    if (event.type == SDL_QUIT) {
        exitflag = 1;
      } else if (event.type == SDL_KEYDOWN) {
        int res = 0, movedir = 0;
        switch (event.key.keysym.sym) {
          case SDLK_LEFT:
          case SDLK_KP_4:
            movedir = sokmoveLEFT;
            break;
          case SDLK_RIGHT:
          case SDLK_KP_6:
            movedir = sokmoveRIGHT;
            break;
          case SDLK_UP:
          case SDLK_KP_8:
            movedir = sokmoveUP;
            break;
          case SDLK_DOWN:
          case SDLK_KP_2:
            movedir = sokmoveDOWN;
            break;
          case SDLK_BACKSPACE:
            sok_undo(&game, &states);
            break;
          case SDLK_ESCAPE:
            exitflag = 1;
            break;
        }
        if (movedir != 0) {
          rotatePlayer(sprites, &game, &states, movedir, renderer, window, tilesize);
          res = sok_move(&game, movedir, 1, &states);
          if (res >= 0) { /* do animations */
            int offset, offsetx = 0, offsety = 0, scrolling;
            /* How will I need to move? */
            if (movedir == sokmoveUP) offsety = -1;
            if (movedir == sokmoveRIGHT) offsetx = 1;
            if (movedir == sokmoveDOWN) offsety = 1;
            if (movedir == sokmoveLEFT) offsetx = -1;
            /* Will I need to move the player, or the entire field? */
            for (offset = 0; offset != tilesize * offsetx; offset += offsetx) {
              if (offset % (tilesize / 8) == 0) {
                SDL_Delay(10);
                scrolling = scrollneeded(&game, window, tilesize, offsetx, offsety);
                draw_screen(&game, &states, sprites, renderer, window, tilesize, offset, 0, scrolling);
              }
            }
            for (offset = 0; offset != tilesize * offsety; offset += offsety) {
              if (offset % (tilesize / 8) == 0) {
                SDL_Delay(10);
                scrolling = scrollneeded(&game, window, tilesize, offsetx, offsety);
                draw_screen(&game, &states, sprites, renderer, window, tilesize, 0, offset, scrolling);
              }
            }
          }
          res = sok_move(&game, movedir, 0, &states);
          if ((res >= 0) && (res & sokmove_solved)) {
            /* display a congrats message and increment level */
            draw_screen(&game, &states, sprites, renderer, window, tilesize, 0, 0, 0);
            exitflag = displaytexture(renderer, sprites->cleared, window, 3, DISPLAYCENTERED | DISPLAYFADEIN);
            curlevel += 1;
            /* load the new level and reset states */
            memcpy(&game, gameslist[curlevel], sizeof(game));
            sok_resetstates(&states);
          }
        }
    }

    if (exitflag != 0) break;
  }

  /* free all textures */
  if (sprites->atom) SDL_DestroyTexture(sprites->atom);
  if (sprites->atom_on_goal) SDL_DestroyTexture(sprites->atom_on_goal);
  if (sprites->floor) SDL_DestroyTexture(sprites->floor);
  if (sprites->goal) SDL_DestroyTexture(sprites->goal);
  if (sprites->player) SDL_DestroyTexture(sprites->player);
  if (sprites->intro) SDL_DestroyTexture(sprites->intro);
  if (sprites->bg) SDL_DestroyTexture(sprites->bg);
  if (sprites->cleared) SDL_DestroyTexture(sprites->cleared);
  if (sprites->help) SDL_DestroyTexture(sprites->help);
  for (x = 0; x < 16; x++) if (sprites->walls[x]) SDL_DestroyTexture(sprites->walls[x]);
  for (x = 0; x < 64; x++) if (sprites->font[x]) SDL_DestroyTexture(sprites->font[x]);

  /* clean up SDL */
  flush_events();
  SDL_DestroyWindow(window);
  SDL_Quit();

  return(0);
}
