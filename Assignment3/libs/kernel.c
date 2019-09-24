#include "kernel.h"

// Apply convolutional kernel on image data
void applyKernel(
  unsigned char **out,
  unsigned char **in,
  unsigned int width,
  unsigned int height,
  unsigned char *topHalo,
  unsigned char *bottomHalo,
  int *kernel,
  unsigned int kernelDim,
  float kernelFactor
) {
  unsigned int const kernelCenter = (kernelDim / 2);
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      int aggregate = 0;
      for (unsigned int ky = 0; ky < kernelDim; ky++) {
        int nky = kernelDim - 1 - ky;
        for (unsigned int kx = 0; kx < kernelDim; kx++) {
          int nkx = kernelDim - 1 - kx;

          int yy = y + (ky - kernelCenter);
          int xx = x + (kx - kernelCenter);
          if (xx >= 0 && xx < (int) width && yy >=0 && yy < (int) height) {
            aggregate += in[yy][xx] * kernel[nky * kernelDim + nkx];
          } else if (yy == -1 && xx >= 0 && xx < (int) width) {
            aggregate += topHalo[xx] * kernel[nky * kernelDim + nkx];
          } else if (yy == (int) height && xx >= 0 && xx < (int) width) {
            aggregate += bottomHalo[xx] * kernel[nky * kernelDim + nkx];
          }
        }
      }
      aggregate *= kernelFactor;
      if (aggregate > 0) {
        out[y][x] = (aggregate > 255) ? 255 : aggregate;
      } else {
        out[y][x] = 0;
      }
    }
  }
}
