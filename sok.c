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
#include <stdlib.h>             /* malloc() */
#include <string.h>             /* memcpy() */
#include <time.h>
#include <SDL2/SDL.h>           /* SDL       */
#include <SDL2/SDL_image.h>     /* SDL_image */
#include "sok_core.h"
#include "data_lev.h"           /* embedded image files */
#include "data_img.h"           /* embedded level files */
#include "data_fnt.h"           /* embedded font files */
#include "data_skn.h"           /* embedded skin files */
#include "data_ico.h"           /* embedded the icon file */

#define PVER "v1.0.1 beta"

#define debugmode 0

#define MAXLEVELS 4096
#define SCREEN_DEFAULT_WIDTH 800
#define SCREEN_DEFAULT_HEIGHT 600

#define DISPLAYCENTERED 1
#define NOREFRESH 2

#define DRAWSCREEN_REFRESH 1
#define DRAWSCREEN_PLAYBACK 2
#define DRAWSCREEN_PUSH 4
#define DRAWSCREEN_NOBG 8
#define DRAWSCREEN_NOTXT 16

#define DRAWSTRING_CENTER -1
#define DRAWSTRING_RIGHT -2
#define DRAWSTRING_BOTTOM -3

#define FONT_SPACE_WIDTH 12
#define FONT_KERNING -3

#define SELECTLEVEL_BACK -1
#define SELECTLEVEL_QUIT -2

struct spritesstruct {
  SDL_Texture *atom;
  SDL_Texture *atom_on_goal;
  SDL_Texture *bg;
  SDL_Texture *black;
  SDL_Texture *cleared;
  SDL_Texture *nosolution;
  SDL_Texture *congrats;
  SDL_Texture *copiedtoclipboard;
  SDL_Texture *snapshottoclipboard;
  SDL_Texture *floor;
  SDL_Texture *goal;
  SDL_Texture *help;
  SDL_Texture *intro;
  SDL_Texture *player;
  SDL_Texture *solved;
  SDL_Texture *walls[16];
  SDL_Texture *font[128];
};

/* returns the absolute value of the 'i' integer. */
static int absval(int i) {
  if (i < 0) return(-i);
  return(i);
}

static void switchfullscreen(SDL_Window *window) {
  static int fullscreenflag = 0;
  fullscreenflag ^= 1;
  if (fullscreenflag != 0) {
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    } else {
      SDL_SetWindowFullscreen(window, 0);
  }
}

static int char2fontid(char c) {
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'a') && (c <= 'z')) return(10 + c - 'a');
  if ((c >= 'A') && (c <= 'Z')) return(36 + c - 'A');
  /* handle symbols */
  switch (c) {
    case ':': return(63);
    case '!': return(64);
    case '$': return(65);
    case '.': return(66);
    case '&': return(67);
    case '*': return(68);
    case ',': return(69);
    case '(': return(70);
    case ')': return(71);
    case '[': return(72);
    case ']': return(73);
    case '-': return(74);
    case '_': return(75);
    case '/': return(76);
  }
  /* if anything else, return 'underscore'... */
  return(75);
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
  if (game->positiony * tilesize + (tilesize / 2) > winh / 2) {
    int res = (winh / 2) - (game->positiony * tilesize + (tilesize / 2));
    if ((game->field_height * tilesize) + res < winh) res = winh - (game->field_height * tilesize);
    return(res);
  }
  return(0);
}

static int flush_events() {
  SDL_Event event;
  int exitflag = 0;
  while (SDL_PollEvent(&event) != 0) if (event.type == SDL_QUIT) exitflag = 1;
  return(exitflag);
}

/* wait for a key up to timeout seconds (-1 = indefintely), while redrawing the renderer screen, if not null */
static int wait_for_a_key(int timeout, SDL_Renderer *renderer) {
  SDL_Event event;
  Uint32 timeouttime;
  timeouttime = SDL_GetTicks() + (timeout * 1000);
  for (;;) {
    SDL_Delay(50);
    if (SDL_PollEvent(&event) != 0) {
      if (renderer != NULL) SDL_RenderPresent(renderer);
      if (event.type == SDL_QUIT) {
          return(1);
        } else if (event.type == SDL_KEYDOWN) {
          return(0);
      }
    }
    if ((timeout > 0) && (SDL_GetTicks() >= timeouttime)) return(0);
  }
}

/* display a bitmap onscreen */
static int displaytexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Window *window, int timeout, int flags, int alpha) {
  int winw, winh;
  SDL_Rect rect, *rectptr;
  SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
  SDL_GetWindowSize(window, &winw, &winh);
  if (flags & DISPLAYCENTERED) {
      rectptr = &rect;
      rect.x = (winw - rect.w) / 2;
      rect.y = (winh - rect.h) / 2;
    } else {
      rectptr = NULL;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(texture, alpha);
  if (SDL_RenderCopy(renderer, texture, NULL, rectptr) != 0) printf("SDL_RenderCopy() failed: %s\n", SDL_GetError());
  if ((flags & NOREFRESH) == 0) SDL_RenderPresent(renderer);
  if (timeout != 0) return(wait_for_a_key(timeout, renderer));
  return(0);
}

/* provides width and height of a string (in pixels) */
static void get_string_size(char *string, struct spritesstruct *sprites, int *w, int *h) {
  int glyphw, glyphh;
  *w = 0;
  *h = 0;
  while (*string != 0) {
    if (*string == ' ') {
        *w += FONT_SPACE_WIDTH;
      } else {
        SDL_QueryTexture(sprites->font[char2fontid(*string)], NULL, NULL, &glyphw, &glyphh);
        *w += glyphw + FONT_KERNING;
        if (glyphh > *h) *h = glyphh;
    }
    string += 1;
  }
}

static void draw_string(char *string, struct spritesstruct *sprites, SDL_Renderer *renderer, int x, int y, SDL_Window *window) {
  int i;
  SDL_Texture *glyph;
  SDL_Rect rectsrc, rectdst;
  /* if centering is requested, get size of the string */
  if ((x < 0) || (y < 0)) {
    int winw, winh, stringw, stringh;
    /* get size of the window */
    SDL_GetWindowSize(window, &winw, &winh);
    /* get pixel length of the string */
    get_string_size(string, sprites, &stringw, &stringh);
    if (x == DRAWSTRING_CENTER) x = (winw - stringw) >> 1;
    if (x == DRAWSTRING_RIGHT) x = winw - stringw - 10;
    if (y == DRAWSTRING_BOTTOM) y = winh - stringh;
    if (y == DRAWSTRING_CENTER) y = (winh - stringh) / 2;
  }
  rectdst.x = x;
  rectdst.y = y;
  for (i = 0; string[i] != 0; i++) {
    if (string[i] == ' ') {
      rectdst.x += FONT_SPACE_WIDTH;
      continue;
    }
    glyph = sprites->font[char2fontid(string[i])];
    SDL_QueryTexture(glyph, NULL, NULL, &rectsrc.w, &rectsrc.h);
    rectdst.w = rectsrc.w;
    rectdst.h = rectsrc.h;
    SDL_RenderCopy(renderer, glyph, NULL, &rectdst);
    rectdst.x += (rectsrc.w + FONT_KERNING);
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

#define DRAWPLAYFIELDTILE_DRAWATOM 1
#define DRAWPLAYFIELDTILE_PUSH 2

static void draw_playfield_tile(struct sokgame *game, int x, int y, struct spritesstruct *sprites, SDL_Renderer *renderer, int winw, int winh, int tilesize, int flags, int moveoffsetx, int moveoffsety) {
  SDL_Rect rect;
  /* compute the dst rect */
  rect.x = getoffseth(game, winw, tilesize) + (x * tilesize) + moveoffsetx;
  rect.y = getoffsetv(game, winh, tilesize) + (y * tilesize) + moveoffsety;
  rect.w = tilesize;
  rect.h = tilesize;

  if ((flags & DRAWPLAYFIELDTILE_DRAWATOM) == 0) {
      if (game->field[x][y] & field_floor) SDL_RenderCopy(renderer, sprites->floor, NULL, &rect);
      if (game->field[x][y] & field_goal) SDL_RenderCopy(renderer, sprites->goal, NULL, &rect);
      if (game->field[x][y] & field_wall) SDL_RenderCopy(renderer, sprites->walls[getwallid(game, x, y)], NULL, &rect);
    } else {
      int atomongoal = 0;
      if ((game->field[x][y] & field_goal) && (game->field[x][y] & field_atom)) {
        atomongoal = 1;
        if (flags & DRAWPLAYFIELDTILE_PUSH) {
          if ((game->positionx == x - 1) && (game->positiony == y) && (moveoffsetx > 0) && ((game->field[x + 1][y] & field_goal) == 0)) atomongoal = 0;
          if ((game->positionx == x + 1) && (game->positiony == y) && (moveoffsetx < 0) && ((game->field[x - 1][y] & field_goal) == 0)) atomongoal = 0;
          if ((game->positionx == x) && (game->positiony == y - 1) && (moveoffsety > 0) && ((game->field[x][y + 1] & field_goal) == 0)) atomongoal = 0;
          if ((game->positionx == x) && (game->positiony == y + 1) && (moveoffsety < 0) && ((game->field[x][y - 1] & field_goal) == 0)) atomongoal = 0;
        }
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
  if (surface == NULL) {
    printf("IMG_LoadPNG_RW() failed: %s\n", IMG_GetError());
    return(-1);
  }
  res = surface->w;
  *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (*texture == NULL) printf("SDL_CreateTextureFromSurface() failed: %s\n", SDL_GetError());
  SDL_FreeSurface(surface);
  return(res);
}

static void draw_screen(struct sokgame *game, struct sokgamestates *states, struct spritesstruct *sprites, SDL_Renderer *renderer, SDL_Window *window, int tilesize, int moveoffsetx, int moveoffsety, int scrolling, int flags, char *levelname) {
  int x, y, winw, winh, offx, offy;
  /* int partialoffsetx = 0, partialoffsety = 0; */
  char stringbuff[256];
  int scrollingadjx = 0, scrollingadjy = 0; /* this is used when scrolling + movement of player is needed */
  int drawtile_flags = 0;
  SDL_GetWindowSize(window, &winw, &winh);
  if (flags & DRAWSCREEN_NOBG) {
      SDL_RenderCopy(renderer, sprites->black, NULL, NULL);
    } else {
      SDL_RenderCopy(renderer, sprites->bg, NULL, NULL);
  }

  if (flags & DRAWSCREEN_PUSH) drawtile_flags = DRAWPLAYFIELDTILE_PUSH;

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
          draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, drawtile_flags, -moveoffsetx, -moveoffsety);
        } else {
          draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, drawtile_flags, 0, 0);
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
      draw_playfield_tile(game, x, y, sprites, renderer, winw, winh, tilesize, DRAWPLAYFIELDTILE_DRAWATOM, offx, offy);
    }
  }
  /* draw where the player is */
  if (scrolling != 0) {
      draw_player(game, states, sprites, renderer, winw, winh, tilesize, scrollingadjx, scrollingadjy);
    } else {
      draw_player(game, states, sprites, renderer, winw, winh, tilesize, moveoffsetx, moveoffsety);
  }
  /* draw text */
  if ((flags & DRAWSCREEN_NOTXT) == 0) {
    sprintf(stringbuff, "%s, level %d", levelname, game->level);
    draw_string(stringbuff, sprites, renderer, 10, DRAWSTRING_BOTTOM, window);
    if (game->solution != NULL) {
        sprintf(stringbuff, "best score: %ld/%ld", sok_history_getlen(game->solution), sok_history_getpushes(game->solution));
      } else {
        sprintf(stringbuff, "best score: -");
    }
    draw_string(stringbuff, sprites, renderer, DRAWSTRING_RIGHT, 0, window);
    sprintf(stringbuff, "moves: %ld / pushes: %ld", sok_history_getlen(states->history), sok_history_getpushes(states->history));
    draw_string(stringbuff, sprites, renderer, 10, 0, window);
  }
  if ((flags & DRAWSCREEN_PLAYBACK) && (time(NULL) % 2 == 0)) draw_string("*** PLAYBACK ***", sprites, renderer, DRAWSTRING_CENTER, 32, window);
  /* Update the screen */
  if (flags & DRAWSCREEN_REFRESH) SDL_RenderPresent(renderer);
}

static int rotatePlayer(struct spritesstruct *sprites, struct sokgame *game, struct sokgamestates *states, enum SOKMOVE dir, SDL_Renderer *renderer, SDL_Window *window, int tilesize, char *levelname, int drawscreenflags) {
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
        draw_screen(game, states, sprites, renderer, window, tilesize, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levelname);
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

static void loadlevel(struct sokgame *togame, struct sokgame *fromgame, struct sokgamestates *states) {
  memcpy(togame, fromgame, sizeof(struct sokgame));
  sok_resetstates(states);
}

/* waits for the user to choose a game type or to load an external xsb file and returns either a pointer to a memory chunk with xsb data or to fill levelfile with a filename */
static unsigned char *selectgametype(SDL_Renderer *renderer, struct spritesstruct *sprites, SDL_Window *window, int tilesize, char **levelfile) {
  int exitflag, winw, winh;
  static int selection = 0;
  unsigned char *memptr[3] = {levels_microban_xsb, levels_sasquatch_xsb, levels_sasquatch3_xsb};
  int textvadj = 12;
  SDL_Event event;
  SDL_Rect rect;

  for (;;) {
    SDL_GetWindowSize(window, &winw, &winh);
    displaytexture(renderer, sprites->intro, window, 0, NOREFRESH, 255);
    /* compute the dst rect */
    rect.x = winw * 0.23;
    rect.y = winh * 0.63 + winh * 0.08 * selection;
    rect.w = tilesize;
    rect.h = tilesize;
    SDL_RenderCopyEx(renderer, sprites->player, NULL, &rect, 90, NULL, SDL_FLIP_NONE);
    draw_string("Easy (Microban)", sprites, renderer, rect.x + 54, textvadj + winh * 0.63 + winh * 0.08 * 0, window);
    draw_string("Normal (Sasquatch)", sprites, renderer, rect.x + 54, textvadj + winh * 0.63 + winh * 0.08 * 1, window);
    draw_string("Hard (Sasquatch III)", sprites, renderer, rect.x + 54, textvadj + winh * 0.63 + winh * 0.08 * 2, window);
    SDL_RenderPresent(renderer);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) if ((SDL_WaitEvent(&event) != 0) && (event.type != SDL_KEYUP) && (event.type != SDL_MOUSEMOTION)) break;

    /* check what event we got */
    if (event.type == SDL_QUIT) {
        return(NULL);
      } else if (event.type == SDL_DROPFILE) {
        if (event.drop.file != NULL) {
          *levelfile = event.drop.file;
          return(NULL);
        }
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
          case SDLK_UP:
          case SDLK_KP_8:
            selection--;
            break;
          case SDLK_DOWN:
          case SDLK_KP_2:
            selection++;
            break;
          case SDLK_RETURN:
          case SDLK_KP_ENTER:
            return(memptr[selection]);
            break;
          case SDLK_F11:
            switchfullscreen(window);
            break;
          case SDLK_ESCAPE:
            return(NULL);
            break;
        }
        if (selection < 0) selection = 2;
        if (selection > 2) selection = 0;
    }
  }
  if (exitflag != 0) return(NULL);
}

static void RenderCopyWithAlpha(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *rect, int alpha) {
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(texture, alpha);
  SDL_RenderCopy(renderer, texture, NULL, rect);
  SDL_SetTextureAlphaMod(texture, 255);
}

/* blit a level preview */
static void blit_levelmap(struct sokgame *game, struct spritesstruct *sprites, int xpos, int ypos, SDL_Renderer *renderer, int tilesize, int alpha) {
  int x, y;
  SDL_Rect rect;
  rect.w = tilesize;
  rect.h = tilesize;
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      /* compute coordinates of the tile on screen */
      rect.x = xpos + (tilesize * x) - (game->field_width * tilesize) / 2;
      rect.y = ypos + (tilesize * y) - (game->field_height * tilesize) / 2;
      /* draw the tile */
      if (game->field[x][y] & field_floor) RenderCopyWithAlpha(renderer, sprites->floor, &rect, alpha);
      if (game->field[x][y] & field_wall) RenderCopyWithAlpha(renderer, sprites->walls[getwallid(game, x, y)], &rect, alpha);
      if ((game->field[x][y] & field_goal) && (game->field[x][y] & field_atom)) { /* atom on goal */
          RenderCopyWithAlpha(renderer, sprites->atom_on_goal, &rect, alpha);
        } else if (game->field[x][y] & field_goal) { /* goal */
          RenderCopyWithAlpha(renderer, sprites->goal, &rect, alpha);
        } else if (game->field[x][y] & field_atom) { /* atom */
          RenderCopyWithAlpha(renderer, sprites->atom, &rect, alpha);
      }
    }
  }
}

static int selectlevel(struct sokgame **gameslist, struct spritesstruct *sprites, SDL_Renderer *renderer, SDL_Window *window, int tilesize, char *levcomment, int levelscount) {
  int i, selection, winw, winh, maxallowedlevel;
  char levelnum[64];
  SDL_Event event;
  /* reload all solutions for levels, in case they changed (for ex. because we just solved a level..) */
  sok_loadsolutions(gameslist, levelscount);

  /* Preselect the first unsolved level */
  selection = 0;
  for (i = 0; i < levelscount; i++) {
    if (gameslist[i]->solution != NULL) {
        if (debugmode != 0) printf("Level %d [%08lX] has solution: %s\n", i + 1, gameslist[i]->crc32, gameslist[i]->solution);
      } else {
        if (debugmode != 0) printf("Level %d [%08lX] has NO solution\n", i + 1, gameslist[i]->crc32);
        selection = i;
        break;
    }
  }

  /* compute the last allowed level */
  i = 0; /* i will temporarily store the number of unsolved levels */
  for (maxallowedlevel = 0; maxallowedlevel < levelscount; maxallowedlevel++) {
    if (gameslist[maxallowedlevel]->solution == NULL) i++;
    if (i > 3) break; /* user can see up to 3 unsolved levels */
  }

  /* loop */
  for (;;) {
    SDL_GetWindowSize(window, &winw, &winh);

    /* draw the screen */
    SDL_RenderClear(renderer);
    if (selection > 0) { /* draw the level before */
      blit_levelmap(gameslist[selection - 1], sprites, winw / 5, winh / 2, renderer, tilesize / 4, 80);
      if (gameslist[selection - 1]->solution != NULL) {
        SDL_Rect rect;
        SDL_QueryTexture(sprites->solved, NULL, NULL, &rect.w, &rect.h);
        rect.x = winw / 5 - rect.w / 2;
        rect.y = winh / 2 - rect.h / 2;
        SDL_RenderCopy(renderer, sprites->solved, NULL, &rect);
      }
    }
    if (selection + 1 < maxallowedlevel) { /* draw the level after */
      blit_levelmap(gameslist[selection + 1], sprites, winw * 4 / 5,  winh / 2, renderer, tilesize / 4, 80);
      if (gameslist[selection + 1]->solution != NULL) {
        SDL_Rect rect;
        SDL_QueryTexture(sprites->solved, NULL, NULL, &rect.w, &rect.h);
        rect.x = winw * 4 / 5 - rect.w / 2;
        rect.y = winh / 2 - rect.h / 2;
        SDL_RenderCopy(renderer, sprites->solved, NULL, &rect);
      }
    }
    blit_levelmap(gameslist[selection], sprites,  winw / 2,  winh / 2, renderer, tilesize / 3, 196);
    if (gameslist[selection]->solution != NULL) {
      SDL_Rect rect;
      SDL_QueryTexture(sprites->solved, NULL, NULL, &rect.w, &rect.h);
      rect.x = winw / 2 - rect.w / 2;
      rect.y = winh / 2 - rect.h / 2;
      SDL_RenderCopy(renderer, sprites->solved, NULL, &rect);
    }
    draw_string(levcomment, sprites, renderer, DRAWSTRING_CENTER, winh / 8, window);
    draw_string("(choose a level)", sprites, renderer, DRAWSTRING_CENTER, winh / 8 + 40, window);
    sprintf(levelnum, "Level %d of %d", selection + 1, levelscount);
    draw_string(levelnum, sprites, renderer, DRAWSTRING_CENTER, winh * 3 / 4, window);
    SDL_RenderPresent(renderer);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) if ((SDL_WaitEvent(&event) != 0) && (event.type != SDL_KEYUP) && (event.type != SDL_MOUSEMOTION)) break;

    /* check what event we got */
    if (event.type == SDL_QUIT) {
        return(SELECTLEVEL_QUIT);
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
          case SDLK_LEFT:
          case SDLK_KP_4:
            if (selection > 0) selection--;
            break;
          case SDLK_RIGHT:
          case SDLK_KP_6:
            if (selection + 1 < maxallowedlevel) selection++;
            break;
          case SDLK_HOME:
            selection = 0;
            break;
          case SDLK_END:
            selection = maxallowedlevel - 1;
            break;
          case SDLK_PAGEUP:
            if (tilesize < 255) tilesize += 4;
            break;
          case SDLK_PAGEDOWN:
            if (tilesize > 6) tilesize -= 4;
            break;
          case SDLK_RETURN:
          case SDLK_KP_ENTER:
            return(selection);
            break;
          case SDLK_F11:
            switchfullscreen(window);
            break;
          case SDLK_ESCAPE:
            return(SELECTLEVEL_BACK);
            break;
        }
    }
  }
}

static int fade2texture(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *texture) {
  int alphaval, exitflag = 0;
  for (alphaval = 0; alphaval < 64; alphaval += 3) {
    exitflag = displaytexture(renderer, texture, window, 0, 0, alphaval);
    if (exitflag != 0) break;
    SDL_Delay(30);
  }
  if (exitflag == 0) exitflag = displaytexture(renderer, texture, window, 0, 0, 255);
  return(exitflag);
}

/* sets the icon in the aplication's title bar */
static void setsokicon(SDL_Window *window) {
  SDL_Surface *surface;
  SDL_RWops *rwop;
  rwop = SDL_RWFromMem(simplesok_png, simplesok_png_len);
  surface = IMG_LoadPNG_RW(rwop);
  SDL_FreeRW(rwop);
  if (surface == NULL) return;
  SDL_SetWindowIcon(window, surface);
  SDL_FreeSurface(surface); /* once the icon is loaded, the surface is not needed anymore */
}

/* returns 1 if curlevel is the last level to solve in the set. returns 0 otherwise. */
static int islevelthelastleft(struct sokgame **gamelist, int curlevel, int levelscount) {
  int x;
  if (curlevel < 0) return(0);
  if (gamelist[curlevel]->solution != NULL) return(0);
  for (x = 0; x < levelscount; x++) {
    if ((gamelist[x]->solution == NULL) && (x != curlevel)) return(0);
  }
  return(1);
}

static void dumplevel2clipboard(struct sokgame *game, char *history) {
  char *txt;
  long solutionlen = 0, playfieldsize;
  int x, y;
  if (game->solution != NULL) solutionlen = strlen(game->solution);
  playfieldsize = (game->field_width + 1) * game->field_height;
  txt = malloc(solutionlen + playfieldsize + 4096);
  if (txt == NULL) return;
  sprintf(txt, "; Level id: %lX\n\n", game->crc32);
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      switch (game->field[x][y] & ~field_floor) {
        case field_wall:
          strcat(txt, "#");
          break;
        case (field_atom | field_goal):
          strcat(txt, "*");
          break;
        case field_atom:
          strcat(txt, "$");
          break;
        case field_goal:
          if ((game->positionx == x) && (game->positiony == y)) {
              strcat(txt, "+");
            } else {
              strcat(txt, ".");
          }
          break;
        default:
          if ((game->positionx == x) && (game->positiony == y)) {
              strcat(txt, "@");
            } else {
              strcat(txt, " ");
          }
          break;
      }
    }
    strcat(txt, "\n");
  }
  strcat(txt, "\n");
  if ((history != NULL) && (history[0] != 0)) { /* only allow if there actually is a solution */
      strcat(txt, "; Solution\n; ");
      strcat(txt, history);
      strcat(txt, "\n");
    } else {
      strcat(txt, "; No solution available\n");
  }
  SDL_SetClipboardText(txt);
  free(txt);
}

int main(int argc, char **argv) {
  struct sokgame **gameslist, game;
  struct sokgamestates *states;
  struct spritesstruct spritesdata;
  struct spritesstruct *sprites = &spritesdata;
  int levelscount, curlevel = 0, exitflag = 0, showhelp = 0, x, lastlevelleft;
  int nativetilesize, tilesize, playsolution, drawscreenflags;
  char *levelfile = NULL;
  #define LEVCOMMENTMAXLEN 32
  char levcomment[LEVCOMMENTMAXLEN];

  SDL_Window* window = NULL;
  SDL_Renderer *renderer;
  SDL_Event event;

  /* init (seed) the randomizer */
  srand(time(NULL));

  /* Init SDL and set the video mode */
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    return(1);
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  /* this makes scaling nicer (use linear scaling instead of raw pixels) */

  if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {  /* init SDL_image with PNG support */
    printf("IMG_Init() failed: %s\n", IMG_GetError());
    return(1);
  }

  window = SDL_CreateWindow("Simple Sokoban " PVER, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_DEFAULT_WIDTH, SCREEN_DEFAULT_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  setsokicon(window);
  SDL_SetWindowMinimumSize(window, 160, 120);

  renderer = SDL_CreateRenderer(window, -1, 0);
  if (renderer == NULL) {
    SDL_DestroyWindow(window);
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return(1);
  }

  /* Load sprites */
  loadGraphic(&sprites->atom, renderer, skin_atom_png, skin_atom_png_len);
  loadGraphic(&sprites->atom_on_goal, renderer, skin_atom_on_goal_png, skin_atom_on_goal_png_len);
  nativetilesize = loadGraphic(&sprites->floor, renderer, skin_floor_png, skin_floor_png_len);
  loadGraphic(&sprites->goal, renderer, skin_goal_png, skin_goal_png_len);
  loadGraphic(&sprites->player, renderer, skin_player_png, skin_player_png_len);
  loadGraphic(&sprites->intro, renderer, img_intro_png, img_intro_png_len);
  loadGraphic(&sprites->bg, renderer, skin_bg_png, skin_bg_png_len);
  loadGraphic(&sprites->black, renderer, img_black_png, img_black_png_len);
  loadGraphic(&sprites->cleared, renderer, img_cleared_png, img_cleared_png_len);
  loadGraphic(&sprites->help, renderer, img_help_png, img_help_png_len);
  loadGraphic(&sprites->solved, renderer, img_solved_png, img_solved_png_len);
  loadGraphic(&sprites->nosolution, renderer, img_nosol_png, img_nosol_png_len);
  loadGraphic(&sprites->congrats, renderer, img_congrats_png, img_congrats_png_len);
  loadGraphic(&sprites->copiedtoclipboard, renderer, img_copiedtoclipboard_png, img_copiedtoclipboard_png_len);
  loadGraphic(&sprites->snapshottoclipboard, renderer, img_snapshottoclipboard_png, img_snapshottoclipboard_png_len);

  /* load walls */
  for (x = 0; x < 16; x++) sprites->walls[x] = NULL;
  loadGraphic(&sprites->walls[0],  renderer, skin_wall0_png,  skin_wall0_png_len);
  loadGraphic(&sprites->walls[1],  renderer, skin_wall1_png,  skin_wall1_png_len);
  loadGraphic(&sprites->walls[2],  renderer, skin_wall2_png,  skin_wall2_png_len);
  loadGraphic(&sprites->walls[3],  renderer, skin_wall3_png,  skin_wall3_png_len);
  loadGraphic(&sprites->walls[4],  renderer, skin_wall4_png,  skin_wall4_png_len);
  loadGraphic(&sprites->walls[5],  renderer, skin_wall5_png,  skin_wall5_png_len);
  loadGraphic(&sprites->walls[6],  renderer, skin_wall6_png,  skin_wall6_png_len);
  loadGraphic(&sprites->walls[7],  renderer, skin_wall7_png,  skin_wall7_png_len);
  loadGraphic(&sprites->walls[8],  renderer, skin_wall8_png,  skin_wall8_png_len);
  loadGraphic(&sprites->walls[9],  renderer, skin_wall9_png,  skin_wall9_png_len);
  loadGraphic(&sprites->walls[10], renderer, skin_wall10_png, skin_wall10_png_len);
  loadGraphic(&sprites->walls[11], renderer, skin_wall11_png, skin_wall11_png_len);
  loadGraphic(&sprites->walls[12], renderer, skin_wall12_png, skin_wall12_png_len);
  loadGraphic(&sprites->walls[13], renderer, skin_wall13_png, skin_wall13_png_len);
  loadGraphic(&sprites->walls[14], renderer, skin_wall14_png, skin_wall14_png_len);
  loadGraphic(&sprites->walls[15], renderer, skin_wall15_png, skin_wall15_png_len);

  /* load font */
  for (x = 0; x < 128; x++) sprites->font[x] = NULL;
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
  loadGraphic(&sprites->font[char2fontid('A')], renderer, font_aa_png, font_aa_png_len);
  loadGraphic(&sprites->font[char2fontid('B')], renderer, font_bb_png, font_bb_png_len);
  loadGraphic(&sprites->font[char2fontid('C')], renderer, font_cc_png, font_cc_png_len);
  loadGraphic(&sprites->font[char2fontid('D')], renderer, font_dd_png, font_dd_png_len);
  loadGraphic(&sprites->font[char2fontid('E')], renderer, font_ee_png, font_ee_png_len);
  loadGraphic(&sprites->font[char2fontid('F')], renderer, font_ff_png, font_ff_png_len);
  loadGraphic(&sprites->font[char2fontid('G')], renderer, font_gg_png, font_gg_png_len);
  loadGraphic(&sprites->font[char2fontid('H')], renderer, font_hh_png, font_hh_png_len);
  loadGraphic(&sprites->font[char2fontid('I')], renderer, font_ii_png, font_ii_png_len);
  loadGraphic(&sprites->font[char2fontid('J')], renderer, font_jj_png, font_jj_png_len);
  loadGraphic(&sprites->font[char2fontid('K')], renderer, font_kk_png, font_kk_png_len);
  loadGraphic(&sprites->font[char2fontid('L')], renderer, font_ll_png, font_ll_png_len);
  loadGraphic(&sprites->font[char2fontid('M')], renderer, font_mm_png, font_mm_png_len);
  loadGraphic(&sprites->font[char2fontid('N')], renderer, font_nn_png, font_nn_png_len);
  loadGraphic(&sprites->font[char2fontid('O')], renderer, font_oo_png, font_oo_png_len);
  loadGraphic(&sprites->font[char2fontid('P')], renderer, font_pp_png, font_pp_png_len);
  loadGraphic(&sprites->font[char2fontid('Q')], renderer, font_qq_png, font_qq_png_len);
  loadGraphic(&sprites->font[char2fontid('R')], renderer, font_rr_png, font_rr_png_len);
  loadGraphic(&sprites->font[char2fontid('S')], renderer, font_ss_png, font_ss_png_len);
  loadGraphic(&sprites->font[char2fontid('T')], renderer, font_tt_png, font_tt_png_len);
  loadGraphic(&sprites->font[char2fontid('U')], renderer, font_uu_png, font_uu_png_len);
  loadGraphic(&sprites->font[char2fontid('V')], renderer, font_vv_png, font_vv_png_len);
  loadGraphic(&sprites->font[char2fontid('W')], renderer, font_ww_png, font_ww_png_len);
  loadGraphic(&sprites->font[char2fontid('X')], renderer, font_xx_png, font_xx_png_len);
  loadGraphic(&sprites->font[char2fontid('Y')], renderer, font_yy_png, font_yy_png_len);
  loadGraphic(&sprites->font[char2fontid('Z')], renderer, font_zz_png, font_zz_png_len);
  loadGraphic(&sprites->font[char2fontid(':')], renderer, font_sym_col_png, font_sym_col_png_len);
  loadGraphic(&sprites->font[char2fontid('!')], renderer, font_sym_excl_png, font_sym_excl_png_len);
  loadGraphic(&sprites->font[char2fontid('$')], renderer, font_sym_doll_png, font_sym_doll_png_len);
  loadGraphic(&sprites->font[char2fontid('.')], renderer, font_sym_dot_png, font_sym_dot_png_len);
  loadGraphic(&sprites->font[char2fontid('&')], renderer, font_sym_ampe_png, font_sym_ampe_png_len);
  loadGraphic(&sprites->font[char2fontid('*')], renderer, font_sym_star_png, font_sym_star_png_len);
  loadGraphic(&sprites->font[char2fontid(',')], renderer, font_sym_comm_png, font_sym_comm_png_len);
  loadGraphic(&sprites->font[char2fontid('(')], renderer, font_sym_par1_png, font_sym_par1_png_len);
  loadGraphic(&sprites->font[char2fontid(')')], renderer, font_sym_par2_png, font_sym_par2_png_len);
  loadGraphic(&sprites->font[char2fontid('[')], renderer, font_sym_bra1_png, font_sym_bra1_png_len);
  loadGraphic(&sprites->font[char2fontid(']')], renderer, font_sym_bra2_png, font_sym_bra2_png_len);
  loadGraphic(&sprites->font[char2fontid('-')], renderer, font_sym_minu_png, font_sym_minu_png_len);
  loadGraphic(&sprites->font[char2fontid('_')], renderer, font_sym_unde_png, font_sym_unde_png_len);
  loadGraphic(&sprites->font[char2fontid('/')], renderer, font_sym_slas_png, font_sym_slas_png_len);

  /* Hide the mouse cursor, disable mouse events and make sure DropEvents are enabled (sometimes they are not) */
  SDL_ShowCursor(SDL_DISABLE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  if (argc == 2) levelfile = argv[1];

  gameslist = malloc(sizeof(struct sokgame *) * MAXLEVELS);
  if (gameslist == NULL) {
    puts("Memory allocation failed!");
    return(1);
  }

  states = sok_newstates();
  if (states == NULL) return(1);

  GametypeSelectMenu:
  levelscount = -1;
  tilesize = nativetilesize;
  if (levelfile != NULL) {
      levelscount = sok_loadfile(gameslist, MAXLEVELS, levelfile, NULL, levcomment, LEVCOMMENTMAXLEN);
    } else {
      unsigned char *xsblevelptr;
      xsblevelptr = selectgametype(renderer, sprites, window, tilesize, &levelfile);
      if ((xsblevelptr == NULL) && (levelfile == NULL)) {
          exitflag = 1;
        } else {
          levelscount = sok_loadfile(gameslist, MAXLEVELS, levelfile, xsblevelptr, levcomment, LEVCOMMENTMAXLEN);
      }
  }

  if ((levelscount < 1) && (exitflag == 0)) {
    SDL_RenderClear(renderer);
    printf("Failed to load the level file [%d]: %s\n", levelscount, sok_strerr(levelscount));
    draw_string("Failed to load the level file!", sprites, renderer, DRAWSTRING_CENTER, DRAWSTRING_CENTER, window);
    wait_for_a_key(-1, renderer);
    exitflag = 1;
  }

  /* printf("Loaded %d levels '%s'\n", levelscount, levcomment); */

  LevelSelectMenu:
  tilesize = nativetilesize;
  if (exitflag == 0) exitflag = flush_events();

  if (exitflag == 0) {
    curlevel = selectlevel(gameslist, sprites, renderer, window, tilesize, levcomment, levelscount);
    if (curlevel == SELECTLEVEL_BACK) {
      if (levelfile == NULL) {
          goto GametypeSelectMenu;
        } else {
          exitflag = 1;
      }
    }
    if (curlevel == SELECTLEVEL_QUIT) exitflag = 1;
  }
  if (exitflag == 0) loadlevel(&game, gameslist[curlevel], states);

  if ((curlevel == 0) && (game.solution == NULL)) showhelp = 1;
  playsolution = 0;
  drawscreenflags = 0;
  if (exitflag == 0) lastlevelleft = islevelthelastleft(gameslist, curlevel, levelscount);

  while (exitflag == 0) {
    if (playsolution > 0) {
        drawscreenflags |= DRAWSCREEN_PLAYBACK;
      } else {
        drawscreenflags &= ~DRAWSCREEN_PLAYBACK;
    }
    draw_screen(&game, states, sprites, renderer, window, tilesize, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
    if (showhelp != 0) {
      exitflag = displaytexture(renderer, sprites->help, window, -1, DISPLAYCENTERED, 255);
      draw_screen(&game, states, sprites, renderer, window, tilesize, 0, 0, 0, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
      showhelp = 0;
    }
    if (debugmode != 0) printf("history: %s\n", states->history);

    /* Wait for an event - but ignore 'KEYUP' and 'MOUSEMOTION' events, since they are worthless in this game */
    for (;;) {
      if (SDL_WaitEventTimeout(&event, 80) == 0) {
        if (playsolution == 0) continue;
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = SDLK_F10;
      }
      if ((event.type != SDL_KEYUP) && (event.type != SDL_MOUSEMOTION)) break;
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
            if (playsolution == 0) sok_undo(&game, states);
            break;
          case SDLK_r:
            playsolution = 0;
            loadlevel(&game, gameslist[curlevel], states);
            break;
          case SDLK_F5: /* dump level & solution (if any) to clipboard */
            dumplevel2clipboard(gameslist[curlevel], gameslist[curlevel]->solution);
            exitflag = displaytexture(renderer, sprites->copiedtoclipboard, window, 2, DISPLAYCENTERED, 255);
            break;
          case SDLK_F9: /* save current state of level to clipboard */
            dumplevel2clipboard(&game, states->history);
            exitflag = displaytexture(renderer, sprites->snapshottoclipboard, window, 2, DISPLAYCENTERED, 255);
            break;
          case SDLK_PAGEUP:
            if (tilesize < 255) tilesize += 2;
            break;
          case SDLK_PAGEDOWN:
            if (tilesize > 4) tilesize -= 2;
            break;
          case SDLK_s:
            if (playsolution == 0) {
              if (game.solution != NULL) { /* only allow if there actually is a solution */
                  loadlevel(&game, gameslist[curlevel], states);
                  playsolution = 1;
                } else {
                  exitflag = displaytexture(renderer, sprites->nosolution, window, 1, DISPLAYCENTERED, 255);
              }
            }
            break;
          case SDLK_F1:
            if (playsolution == 0) showhelp = 1;
            break;
          case SDLK_F2:
            if ((drawscreenflags & DRAWSCREEN_NOBG) && (drawscreenflags & DRAWSCREEN_NOTXT)) {
                drawscreenflags &= ~(DRAWSCREEN_NOBG | DRAWSCREEN_NOTXT);
              } else if (drawscreenflags & DRAWSCREEN_NOBG) {
                drawscreenflags |= DRAWSCREEN_NOTXT;
              } else if (drawscreenflags & DRAWSCREEN_NOTXT) {
                drawscreenflags &= ~DRAWSCREEN_NOTXT;
                drawscreenflags |= DRAWSCREEN_NOBG;
              } else {
                drawscreenflags |= DRAWSCREEN_NOTXT;
            }
            break;
          case SDLK_F11:
            switchfullscreen(window);
            break;
          case SDLK_ESCAPE:
            goto LevelSelectMenu;
            break;
        }
        if (playsolution > 0) {
          movedir = 0;
          switch (game.solution[playsolution - 1]) {
            case 'u':
            case 'U':
              movedir = sokmoveUP;
              break;
            case 'r':
            case 'R':
              movedir = sokmoveRIGHT;
              break;
            case 'd':
            case 'D':
              movedir = sokmoveDOWN;
              break;
            case 'l':
            case 'L':
              movedir = sokmoveLEFT;
              break;
          }
          playsolution += 1;
          if (game.solution[playsolution - 1] == 0) playsolution = 0;
        }
        if (movedir != 0) {
          rotatePlayer(sprites, &game, states, movedir, renderer, window, tilesize, levcomment, drawscreenflags);
          res = sok_move(&game, movedir, 1, states);
          if (res >= 0) { /* do animations */
            int offset, offsetx = 0, offsety = 0, scrolling;
            int modulator = tilesize / 8;
            if (modulator < 2) modulator = 2;
            if (res & sokmove_pushed) drawscreenflags |= DRAWSCREEN_PUSH;
            /* How will I need to move? */
            if (movedir == sokmoveUP) offsety = -1;
            if (movedir == sokmoveRIGHT) offsetx = 1;
            if (movedir == sokmoveDOWN) offsety = 1;
            if (movedir == sokmoveLEFT) offsetx = -1;
            /* Will I need to move the player, or the entire field? */
            for (offset = 0; offset != tilesize * offsetx; offset += offsetx) {
              if (offset % modulator == 0) {
                SDL_Delay(10);
                scrolling = scrollneeded(&game, window, tilesize, offsetx, offsety);
                draw_screen(&game, states, sprites, renderer, window, tilesize, offset, 0, scrolling, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
              }
            }
            for (offset = 0; offset != tilesize * offsety; offset += offsety) {
              if (offset % modulator == 0) {
                SDL_Delay(10);
                scrolling = scrollneeded(&game, window, tilesize, offsetx, offsety);
                draw_screen(&game, states, sprites, renderer, window, tilesize, 0, offset, scrolling, DRAWSCREEN_REFRESH | drawscreenflags, levcomment);
              }
            }
          }
          res = sok_move(&game, movedir, 0, states);
          if ((res >= 0) && (res & sokmove_solved)) {
            int alphaval;
            SDL_Texture *tmptex;
            /* display a congrats message */
            if (lastlevelleft != 0) {
                tmptex = sprites->congrats;
              } else {
                tmptex = sprites->cleared;
            }
            flush_events();
            for (alphaval = 0; alphaval < 255; alphaval += 30) {
              draw_screen(&game, states, sprites, renderer, window, tilesize, 0, 0, 0, 0, levcomment);
              exitflag = displaytexture(renderer, tmptex, window, 0, DISPLAYCENTERED, alphaval);
              SDL_Delay(25);
              if (exitflag != 0) break;
            }
            if (exitflag == 0) {
              draw_screen(&game, states, sprites, renderer, window, tilesize, 0, 0, 0, 0, levcomment);
              /* if this was the last level left, display a congrats screen */
              if (lastlevelleft != 0) {
                  exitflag = displaytexture(renderer, sprites->congrats, window, 10, DISPLAYCENTERED, 255);
                } else {
                  exitflag = displaytexture(renderer, sprites->cleared, window, 3, DISPLAYCENTERED, 255);
              }
              /* fade out to black */
              if (exitflag == 0) {
                fade2texture(renderer, window, sprites->black);
                exitflag = flush_events();
              }
            }
            /* load the new level and reset states */
            goto LevelSelectMenu;
          }
        }
        drawscreenflags &= ~DRAWSCREEN_PUSH;
    }

    if (exitflag != 0) break;
  }

  /* free the states struct */
  sok_freestates(states);

  /* free all textures */
  if (sprites->atom) SDL_DestroyTexture(sprites->atom);
  if (sprites->atom_on_goal) SDL_DestroyTexture(sprites->atom_on_goal);
  if (sprites->floor) SDL_DestroyTexture(sprites->floor);
  if (sprites->goal) SDL_DestroyTexture(sprites->goal);
  if (sprites->player) SDL_DestroyTexture(sprites->player);
  if (sprites->intro) SDL_DestroyTexture(sprites->intro);
  if (sprites->bg) SDL_DestroyTexture(sprites->bg);
  if (sprites->black) SDL_DestroyTexture(sprites->black);
  if (sprites->nosolution) SDL_DestroyTexture(sprites->nosolution);
  if (sprites->cleared) SDL_DestroyTexture(sprites->cleared);
  if (sprites->help) SDL_DestroyTexture(sprites->help);
  if (sprites->congrats) SDL_DestroyTexture(sprites->congrats);
  if (sprites->copiedtoclipboard) SDL_DestroyTexture(sprites->copiedtoclipboard);
  if (sprites->snapshottoclipboard) SDL_DestroyTexture(sprites->snapshottoclipboard);
  for (x = 0; x < 16; x++) if (sprites->walls[x]) SDL_DestroyTexture(sprites->walls[x]);
  for (x = 0; x < 128; x++) if (sprites->font[x]) SDL_DestroyTexture(sprites->font[x]);

  /* clean up SDL */
  flush_events();
  IMG_Quit();  /* clean up SDL_image */
  SDL_DestroyWindow(window);
  SDL_Quit();

  return(0);
}
