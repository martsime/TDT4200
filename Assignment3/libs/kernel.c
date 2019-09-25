#include <stdio.h>
#include "kernel.h"

// Apply convolutional kernel on image data
void applyKernel(
  unsigned char **out,
  unsigned char **in,
  imageHalo *outHalo,
  imageHalo *inHalo,
  int *kernel,
  unsigned int kernelDim,
  float kernelFactor
) {
  unsigned int const kernelCenter = (kernelDim / 2);
  int count = inHalo->count;
  int width = inHalo->width;
  int height = inHalo->height;
  int startX = -count;
  int endX = width + count;
  int startY = -count;
  int endY = height + count;


  for (int y = startY; y < endY; y++) {
    for (int x = startX; x < endX; x++) {
      int aggregate = 0;
      for (unsigned int ky = 0; ky < kernelDim; ky++) {
        int nky = kernelDim - 1 - ky;
        for (unsigned int kx = 0; kx < kernelDim; kx++) {
          int nkx = kernelDim - 1 - kx;

          int yy = y + (ky - kernelCenter);
          int xx = x + (kx - kernelCenter);

          // Inside the original data
          if (xx >= 0 && xx < width && yy >= 0 && yy < height) {
            aggregate += in[yy][xx] * kernel[nky * kernelDim + nkx];

          // Inside north halo
          } else if (yy < 0 && yy >= startY && xx >= startX && xx < endX) {
            // printf("N Halo: %d, k: %d \n", inHalo->north[yy + count][xx + count], kernel[nky * kernelDim + nkx]);
            aggregate += inHalo->north[yy + count][xx + count] * kernel[nky * kernelDim + nkx]; // TODO : Verify xx or xx + count

          // Inside south halo
          } else if (yy >= height && yy < endY && xx >= startX && xx < endX) {
            // printf("S Halo: %d, k: %d \n", inHalo->south[yy - height][xx + count], kernel[nky * kernelDim + nkx]);
            aggregate += inHalo->south[yy - height][xx + count] * kernel[nky * kernelDim + nkx]; // TODO : Verify xx or xx + count
            
          // Inside west halo
          } else if (yy >= 0 && yy < height && xx >= startX && xx < 0) {
            // printf("E Halo: %d, k: %d \n", inHalo->east[yy][xx + count], kernel[nky * kernelDim + nkx]);
            aggregate += inHalo->west[yy][xx + count] * kernel[nky * kernelDim + nkx];

          // Inside east halo
          } else if (yy >= 0 && yy < height && xx >= width && xx < endX) {
            // printf("W Halo: %d, k: %d \n", inHalo->east[yy][xx - width], kernel[nky * kernelDim + nkx]);
            aggregate += inHalo->east[yy][xx - width] * kernel[nky * kernelDim + nkx];
          }
        }
      }

      // printf("Aggregate: %d\n", aggregate);
      aggregate *= kernelFactor;
      if (aggregate > 0) {
        aggregate = (aggregate > 255) ? 255 : aggregate;
      } else {
        aggregate = 0;
      }
      if (x >= 0 && x < width && y >= 0 && y < height) {
        //printf("Orginal: (%d, %d) agg: %d \n", y, x, aggregate);
        out[y][x] = aggregate;
      } 
      else if (y < 0) {
        // printf("North: (%d, %d) agg: %d\n", y + count, x + count, aggregate);
        outHalo->north[y + count][x + count] = aggregate;
      } else if (y >= height) {
        // printf("South: (%d, %d) agg: %d \n", y - height, x + count, aggregate);
        outHalo->south[y - height][x + count] = aggregate;
      } else if (y >= 0 && y < height && x < 0) {
        // printf("East: (%d, %d) agg: %d \n", y, x + count, aggregate);
        outHalo->west[y][x + count] = aggregate;
      } else if (y >= 0 && y < height && x >= width) {
        // printf("West: (%d, %d) agg: %d \n", y, x - width, aggregate);
        outHalo->east[y][x - width] = aggregate;
      }
    }
  }
}
