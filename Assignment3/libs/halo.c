#include <stdlib.h>
#include <stdio.h>
#include "halo.h"

void freeHaloData(imageHalo *halo) {
  /* Free all allocated memory of halo */
  if (halo->rawnorth != NULL) {
    free(halo->rawnorth);
    halo->rawnorth = NULL;
  }
  if (halo->north != NULL) {
    free(halo->north);
    halo->north = NULL;
  }
  if (halo->rawsouth != NULL) {
    free(halo->rawsouth);
    halo->rawsouth = NULL;
  }
  if (halo->south != NULL) {
    free(halo->south);
    halo->south = NULL;
  }
  if (halo->raweast != NULL) {
    free(halo->raweast);
    halo->raweast = NULL;
  }
  if (halo->east != NULL) {
    free(halo->east);
    halo->east = NULL;
  }
  if (halo->rawwest != NULL) {
    free(halo->rawwest);
    halo->rawwest = NULL;
  }
  if (halo->west != NULL) {
    free(halo->west);
    halo->west = NULL;
  }
}

void freeImageHalo(imageHalo *halo) {
  freeHaloData(halo);
  if (halo != NULL) {
    free(halo);
  }
}

imageHalo* newImageHalo(int width, int height, int count) {
  imageHalo *halo = malloc(sizeof(imageHalo));
  if (halo == NULL) {
    return NULL;
  }

  halo->width = width;
  halo->height = height;
  halo->count = count;

  // Width including halo on each side
  int totalWidth = halo->width + count * 2;
  halo->rawnorth = calloc(
    totalWidth * count,
    sizeof(unsigned char)
  );
  halo->rawsouth = calloc(
    totalWidth * count,
    sizeof(unsigned char)
  );
  halo->raweast = calloc(
    halo->height * count,
    sizeof(unsigned char)
  );
  halo->rawwest = calloc(
    halo->height * count,
    sizeof(unsigned char)
  );
  halo->north = malloc(halo->count * sizeof(unsigned char *));
  halo->south = malloc(halo->count * sizeof(unsigned char *));
  halo->east = malloc(halo->height * sizeof(unsigned char *));
  halo->west = malloc(halo->height * sizeof(unsigned char *));
    
  for (unsigned int i = 0; i < count; i++) {
    halo->north[i] = &(halo->rawnorth[i * totalWidth]);
    halo->south[i] = &(halo->rawsouth[i * totalWidth]);
  }

  for (unsigned int i = 0; i < (int)halo->height; i++) {
    halo->east[i] = &(halo->raweast[i * count]);
    halo->west[i] = &(halo->rawwest[i * count]);
  }
  return halo;
}

void createWestHalo(unsigned char *buffer, int haloWidth, bmpImageChannel *channel) {
  unsigned char *insertPtr = buffer;

  for (unsigned int y = 0; y < (int)channel->height; y++) {
    for (unsigned int x = 0; x < haloWidth; x++) {
      *insertPtr = channel->data[y][x];
      insertPtr++;
    }
  }
}

void createEastHalo(unsigned char *buffer, int haloWidth, bmpImageChannel *channel) {
  unsigned char *insertPtr = buffer;
  int cx = (int)channel->width - 1 - haloWidth;
  for (unsigned int y = 0; y < (int)channel->height; y++) {
    for (unsigned int x = 0; x < haloWidth; x++) {
      *insertPtr = channel->data[y][cx + x];
      insertPtr++;
    }
  }
}

void createNorthHalo(
  unsigned char *buffer,
  int haloWidth,
  bmpImageChannel *channel,
  imageHalo *channelHalo
) {
  unsigned char *insertPtr = buffer;
  for (int y = 0; y < haloWidth; y++) {
    for (int x = -haloWidth; x < (int)channel->width + haloWidth; x++) {
      if (x < 0) {
        *insertPtr = channelHalo->west[y][x + haloWidth];
      } else if (x >= 0 && x < (int)channel->width) {
        *insertPtr = channel->data[y][x];
      } else if (x >= (int)channel->width) {
        *insertPtr = channelHalo->east[y][x - (int)channel->width];
      }
      insertPtr++;
    }
  }
}

void createSouthHalo(
  unsigned char *buffer,
  int haloWidth,
  bmpImageChannel *channel,
  imageHalo *channelHalo
) {
  unsigned char *insertPtr = buffer;
  int cy = (int)channel->height - 1 - haloWidth;
  for (int y = 0; y < haloWidth; y++) {
    for (int x = -haloWidth; x < (int)channel->width + haloWidth; x++) {
      if (x < 0) {
        *insertPtr = channelHalo->west[cy + y][x + haloWidth];
      } else if (x >= 0 && x < (int)channel->width) {
        *insertPtr = channel->data[cy + y][x];
      } else if (x >= (int)channel->width) {
        *insertPtr = channelHalo->east[cy + y][x - (int)channel->width];
      }
      insertPtr++;
    }
  }
}

void swapHalo(imageHalo **one, imageHalo **two) {
  imageHalo *tmp = *two;
  *two = *one;
  *one = tmp;
}


