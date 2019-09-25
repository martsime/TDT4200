#include "bitmap.h"

#ifndef HALO_H
#define HALO_H

// The Halo has the same layout as the bmpImageChannel where
// each directon has a pointer to the raw contigious data and
// one pointer to all the row pointers
//
// NOTE: The east and west halo are as tall as the image, but the 
// north and south are as wide as the image plus the east and west halo.

typedef struct {
  unsigned int width;
  unsigned int height;
  unsigned int count;
  unsigned char *rawnorth;
  unsigned char **north;
  unsigned char *rawsouth;
  unsigned char **south;
  unsigned char *raweast;
  unsigned char **east;
  unsigned char *rawwest;
  unsigned char **west;
} imageHalo;

void freeImageHalo(imageHalo *halo);
imageHalo * newImageHalo(int width, int height, int count);

void createEastHalo(
    unsigned char *buffer,
    int haloWidth,
    bmpImageChannel *channel
);
void createWestHalo(
    unsigned char *buffer,
    int haloWidth,
    bmpImageChannel *channel
);
void createNorthHalo(
  unsigned char *buffer,
  int haloWidth,
  bmpImageChannel *channel,
  imageHalo *channelHalo
);
void createSouthHalo(
  unsigned char *buffer,
  int haloWidth,
  bmpImageChannel *channel,
  imageHalo *channelHalo
);

void swapHalo(imageHalo **one, imageHalo **two);

#endif
