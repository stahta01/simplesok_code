/*
 * This file is part of the 'Simple Sokoban' project.
 *
 * Copyright (C) Mateusz Viste 2014
 *
 * ----------------------------------------------------------------------
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crc32.h"
#include "save.h"
#include "sok_core.h"

static void trim(char *comment) {
  int x, firstrealchar = -1, lastrealchar = 0;
  if (comment == NULL) return;
  for (x = 0; comment[x] != 0; x++) {
    if ((firstrealchar < 0) && (comment[x] != ' ')) firstrealchar = x;
    if (comment[x] != ' ') lastrealchar = x;
  }
  /* RTRIM */
  comment[lastrealchar + 1] = 0;
  /* LTRIM */
  if (firstrealchar > 0) {
    for (x = 0; x < 1 + lastrealchar - firstrealchar; x++) comment[x] = comment[x + firstrealchar];
    comment[1 + lastrealchar - firstrealchar] = 0;
  }
}

static struct sokgame *sok_allocgame(void) {
  struct sokgame *result;
  result = malloc(sizeof(struct sokgame));
  return(result);
}

static void sok_freegame(struct sokgame *game) {
  if (game == NULL) return;
  if (game->solution != NULL) free(game->solution);
  free(game);
}

/* free a list of allocated games */
void sok_freefile(struct sokgame **gamelist, int gamescount) {
  int x;
  for (x = 0; x < gamescount; x++) {
    sok_freegame(gamelist[x]);
  }
}

/* reads a byte from memory of from a file, whichever is passed as a parameter */
static int readbytefromfileormem(FILE *fd, unsigned char **memptr) {
  int result = -1;
  if (fd != NULL) {
      result = getc(fd);
    } else if (memptr != NULL) {
      result = **memptr;
      *memptr += 1;
      if (result == 0) result = -1;
  }
  return(result);
}

/* reads a single RLE chunk from file fd, fills bytebuff with the actual data byte and returns the amount of times it should be repeated. returns -1 on error (like end of file). */
static int readRLEbyte(FILE *fd, unsigned char **memptr, int *bytebuff) {
  int rleprefix = -1;
  for (;;) { /* RLE support */
      *bytebuff = readbytefromfileormem(fd, memptr);
      if (*bytebuff < 0) return(-1);
      if ((*bytebuff >= '0') && (*bytebuff <= '9')) {
        if (rleprefix > 0) {
            rleprefix *= 10;
          } else {
            rleprefix = 0;
        }
        rleprefix += (*bytebuff - '0');
      } else { /* not a RLE prefix */
        break;
    }
  } /* RLE parsing done */
  if (rleprefix < 0) rleprefix = 1;
  return(rleprefix);
}

/* floodfill algorithm to fill areas of a playfield that are not contained in walls */
static void floodFillField(struct sokgame *game, int x, int y) {
  if ((x >= 0) && (x < 64) && (y >= 0) && (y < 64) && (game->field[x][y] == field_floor)) {
    game->field[x][y] = 0; /* set the 'pixel' before starting recursion */
    floodFillField(game, x + 1, y);
    floodFillField(game, x - 1, y);
    floodFillField(game, x, y + 1);
    floodFillField(game, x, y - 1);
  }
}


/* loads the next level from open file fd. returns 0 on success, 1 on success with end of file reached, or -1 on error. */
static int loadlevelfromfile(struct sokgame *game, FILE *fd, unsigned char **memptr, char *comment, int maxcommentlen) {
  int leveldatastarted = 0, endoffile = 0;
  int x, y, bytebuff;
  int commentfound = 0;
  char *origcomment = comment;
  game->positionx = -1;
  game->positiony = -1;
  game->field_width = 0;
  game->field_height = 0;
  game->solution = NULL;
  if ((comment != NULL) && (maxcommentlen > 0)) *comment = 0;

  /* Fill the area with floor */
  for (y = 0; y < 64; y++) {
    for (x = 0; x < 64; x++) {
      game->field[x][y] = field_floor;
    }
  }

  x = 0;
  y = 0;

  for (;;) {
    int rleprefix;
    rleprefix = readRLEbyte(fd, memptr, &bytebuff);
    if (rleprefix < 0) endoffile = 1;
    if (endoffile != 0) break;
    for (; rleprefix > 0; rleprefix--) {
      switch (bytebuff) {
        case ' ': /* empty space */
        case '-': /* dash (-) and underscore (_) are sometimes used to denote empty spaces */
        case '_':
          game->field[x + 1][y + 1] |= field_floor;
          x += 1;
          break;
        case '#': /* wall */
          game->field[x + 1][y + 1] |= field_wall;
          x += 1;
          break;
        case '@': /* player */
          game->field[x + 1][y + 1] |= field_floor;
          game->positionx = x;
          game->positiony = y;
          x += 1;
          break;
        case '*': /* atom on goal */
          game->field[x + 1][y + 1] |= field_goal;
        case '$': /* atom */
          game->field[x + 1][y + 1] |= field_atom;
          x += 1;
          break;
        case '+': /* player on goal */
          game->positionx = x;
          game->positiony = y;
        case '.': /* goal */
        game->field[x + 1][y + 1] |= field_goal;
          x += 1;
          break;
        case '\n': /* next row */
        case '|':  /* some variants of the xsb format use | as the 'new row' separator (mostly when used with RLE) */
          if (leveldatastarted != 0) y += 1;
          x = 0;
          break;
        default: /* anything else is a comment -> skip until end of line or end of file */
          if (leveldatastarted != 0) leveldatastarted = -1;
          if ((commentfound == 0) && (comment != NULL)) commentfound = -1;
          for (;;) {
            bytebuff = readbytefromfileormem(fd, memptr);
            if (bytebuff == '\n') break;
            if (bytebuff < 0) {
              endoffile = 1;
              break;
            }
            if (commentfound == -1) {
              if (maxcommentlen > 1) {
                  maxcommentlen--;
                  *comment = bytebuff;
                  comment += 1;
                } else {
                  commentfound = -2;
              }
            }
          }
          if (commentfound < 0) {
            commentfound = 1;
            *comment = 0;
            trim(origcomment);
          }
          break;
      }
      if ((leveldatastarted < 0) || (endoffile != 0)) break;
      if (x > 0) leveldatastarted = 1;
      if (x >= 62) return(-1);
      if (y >= 62) return(-1);
      if (x > game->field_width) game->field_width = x;
      if ((y >= game->field_height) && (x > 0)) game->field_height = y + 1;
    }
    if ((leveldatastarted < 0) || (endoffile != 0)) break;
  }

  /* check if the loaded game looks sane */
  if (game->positionx < 0) return(-1);
  if (game->field_height < 1) return(-1);
  if (game->field_width < 1) return(-1);
  if (leveldatastarted == 0) return(-1);

  /* remove floors around the level */
  floodFillField(game, 63, 63);

  /* move the field by -1 vertically and horizontally to remove the additional row and column added for the fill function to be able to get around the field. */
  for (y = 0; y < 63; y++) {
    for (x = 0; x < 63; x++) {
      game->field[x][y] = game->field[x + 1][y + 1];
    }
  }
  /* compute the CRC32 of the field */
  game->crc32 = crc32_init();
  for (y = 0; y < game->field_width; y++) {
    for (x = 0; x < game->field_height; x++) {
      crc32_feed(&(game->crc32), &(game->field[x][y]), 1);
    }
  }
  crc32_finish(&(game->crc32));

  if (endoffile != 0) return(1);
  return(0);
}

/* load levels from a file, and put them into an array of up to maxlevels levels */
int sok_loadfile(struct sokgame **gamelist, int maxlevels, char *gamelevel, unsigned char *memptr, char *comment, int maxcommentlen) {
  FILE *fd = NULL;
  int level, loadres, errflag = 0;
  if (gamelevel != NULL) {
    fd = fopen(gamelevel, "r");
    if (fd == NULL) return(-1);
  }

  for (level = 0;; level++) { /* iterate to load games sequentially from the file */
    /* puts("loading level.."); */
    if (level == maxlevels) {
      level--;
      errflag = 1;
      break;
    }
    gamelist[level] = sok_allocgame();
    if (gamelist[level] == NULL) {
      level--;
      errflag = 1;
      break;
    }

    /* call loadlevelfromfile */
    loadres = loadlevelfromfile(gamelist[level], fd, &memptr, (level == 0) ? comment : NULL, maxcommentlen);

    if (loadres > 0) break; /* end of file */
    if (loadres < 0) { /* error loading level data */
      if (level == 0) errflag = 1;
      sok_freegame(gamelist[level]);
      level--;
      break;
    }

    /* write the level num and load the solution (if any) */
    gamelist[level]->level = level + 1;
    gamelist[level]->solution = solution_load(gamelist[level]->crc32);

  }

  if (fd != NULL) fclose(fd);

  if (errflag != 0) {
    sok_freefile(gamelist, level + 1);
    return(-1);
  }

  return(level + 1);
}


/* checks if level is solved yet. returns 0 if not, non-zero otherwise. */
int sok_checksolution(struct sokgame *game, struct sokgamestates *states) {
  int x, y;
  for (y = 0; y < game->field_height; y++) {
    for (x = 0; x < game->field_width; x++) {
      if (((game->field[x][y] & field_goal) != 0) && ((game->field[x][y] & field_atom) == 0)) return(0);
    }
  }
  /* no non-filled goal found = level completed! */
  if (states == NULL) return(1);
  /* Check if the solution is better than the one we have */
  if ((game->solution == NULL) || (strlen(game->solution) > (unsigned)states->movescount)) {
    solution_save(game->crc32, states->history);
  }
  return(1);
}


int sok_move(struct sokgame *game, enum SOKMOVE dir, int validitycheck, struct sokgamestates *states) {
  int res = 0;
  int x, y, vectorx = 0, vectory = 0, alreadysolved;
  char historychar = ' ';
  alreadysolved = sok_checksolution(game, NULL);
  x = game->positionx;
  y = game->positiony;
  switch (dir) {
    case sokmoveUP:
      vectory = -1;
      states->angle = 0;
      historychar = 'u';
      break;
    case sokmoveRIGHT:
      vectorx = 1;
      states->angle = 90;
      historychar = 'r';
      break;
    case sokmoveDOWN:
      vectory = 1;
      states->angle = 180;
      historychar = 'd';
      break;
    case sokmoveLEFT:
      vectorx = -1;
      states->angle = 270;
      historychar = 'l';
      break;
  }

  if (y < 1) return(-1);
  if (game->field[x + vectorx][y + vectory] & field_wall) return(-1);
  /* is there an atom on our way? */
  if (game->field[x + vectorx][y + vectory] & field_atom) {
    if (alreadysolved != 0) return(-1);
    if ((y + vectory < 1) || (y + vectory > 62) || (x + vectorx < 1) || (x + vectorx > 62)) return(-1);
    if (game->field[x + vectorx * 2][y + vectory * 2] & (field_wall | field_atom)) return(-1);
    res |= sokmove_pushed;
    if (game->field[x + vectorx * 2][y + vectory * 2] & field_goal) res |= sokmove_ongoal;
    if (validitycheck == 0) {
      historychar -= 32; /* change historical move to uppercase to mark a push action */
      game->field[x + vectorx][y + vectory] &= ~field_atom;
      game->field[x + vectorx * 2][y + vectory * 2] |= field_atom;
    }
  }
  if (validitycheck == 0) {
    if (states->movescount >= (int)sizeof(states->history) - 1) states->movescount = -1; /* 'undo' overflow protection */
    if (states->movescount >= 0) {
      states->history[states->movescount] = historychar;
      states->movescount += 1;
      states->history[states->movescount] = 0; /* makes it a null-terminated string in case anyone would want to print it as-is */
    }
    game->positiony += vectory;
    game->positionx += vectorx;
  }
  if ((alreadysolved == 0) && (sok_checksolution(game, states) != 0)) res |= sokmove_solved;
  return(res);
}

void sok_resetstates(struct sokgamestates *states) {
  memset(states, 0, sizeof(struct sokgamestates));
}

void sok_undo(struct sokgame *game, struct sokgamestates *states) {
  int movex = 0, movey = 0;
  if (states->movescount < 1) return;
  states->movescount -= 1;
  switch (states->history[states->movescount]) {
    case 'u':
    case 'U':
      movey = 1;
      states->angle = 0;
      break;
    case 'r':
    case 'R':
      movex = -1;
      states->angle = 90;
      break;
    case 'd':
    case 'D':
      movey = -1;
      states->angle = 180;
      break;
    case 'l':
    case 'L':
      movex = 1;
      states->angle = 270;
      break;
  }
  /* if it was a PUSH action, then move the atom back */
  if ((states->history[states->movescount] >= 'A') && ((states->history[states->movescount] <= 'Z'))) {
    game->field[game->positionx - movex][game->positiony - movey] &= ~field_atom;
    game->field[game->positionx][game->positiony] |= field_atom;
  }
  game->positionx += movex;
  game->positiony += movey;
  states->history[states->movescount] = 0;
}