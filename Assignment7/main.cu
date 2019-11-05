#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
extern "C" {
    #include "libs/bitmap.h"
}

#define ERROR_EXIT -1

#define cudaErrorCheck(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true)
{
   if (code != cudaSuccess)
   {
      fprintf(stderr,"GPUassert: %s %s %s %d\n", cudaGetErrorName(code), cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

const int GPU = 1;
const int CPU = 0;

// Convolutional Filter Examples, each with dimension 3,
// gaussian filter with dimension 5
// If you apply another filter, remember not only to exchange
// the filter but also the filterFactor and the correct dimension.

int const sobelYFilter[] = {-1, -2, -1,
                             0,  0,  0,
                             1,  2,  1};
float const sobelYFilterFactor = (float) 1.0;

int const sobelXFilter[] = {-1, -0, -1,
                            -2,  0, -2,
                            -1,  0, -1 , 0};
float const sobelXFilterFactor = (float) 1.0;


int const laplacian1Filter[] = {  -1,  -4,  -1,
                                 -4,  20,  -4,
                                 -1,  -4,  -1};

float const laplacian1FilterFactor = (float) 1.0;

int const laplacian2Filter[] = { 0,  1,  0,
                                 1, -4,  1,
                                 0,  1,  0};
float const laplacian2FilterFactor = (float) 1.0;

int const laplacian3Filter[] = { -1,  -1,  -1,
                                  -1,   8,  -1,
                                  -1,  -1,  -1};
float const laplacian3FilterFactor = (float) 1.0;


//Bonus Filter:

int const gaussianFilter[] = { 1,  4,  6,  4, 1,
                               4, 16, 24, 16, 4,
                               6, 24, 36, 24, 6,
                               4, 16, 24, 16, 4,
                               1,  4,  6,  4, 1 };

float const gaussianFilterFactor = (float) 1.0 / 256.0;


// Apply convolutional filter on image data
void applyFilter(unsigned char **out, unsigned char **in, unsigned int width, unsigned int height, int *filter, unsigned int filterDim, float filterFactor) {
  unsigned int const filterCenter = (filterDim / 2);
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++) {
      int aggregate = 0;
      for (unsigned int ky = 0; ky < filterDim; ky++) {
        int nky = filterDim - 1 - ky;
        for (unsigned int kx = 0; kx < filterDim; kx++) {
          int nkx = filterDim - 1 - kx;

          int yy = y + (ky - filterCenter);
          int xx = x + (kx - filterCenter);
          if (xx >= 0 && xx < (int) width && yy >=0 && yy < (int) height)
            aggregate += in[yy][xx] * filter[nky * filterDim + nkx];
        }
      }
      aggregate *= filterFactor;
      if (aggregate > 0) {
        out[y][x] = (aggregate > 255) ? 255 : aggregate;
      } else {
        out[y][x] = 0;
      }
    }
  }
}

__global__ void applyFilterCuda(unsigned char *out, unsigned char *in, unsigned int width, unsigned int height, int *filter, unsigned int filterDim, float filterFactor) { 
    int iy = blockDim.y * blockIdx.y + threadIdx.y;
    int ix = blockDim.x * blockIdx.x + threadIdx.x;

    // Boundary check
    if (ix < 0 || ix >= (int) width || iy < 0 || iy >= (int) height)
        return;

    unsigned int const filterCenter = (filterDim / 2);
    int aggregate = 0;
    for (unsigned int ky = 0; ky < filterDim; ky++) {
        int nky = filterDim - 1 - ky;
        for (unsigned int kx = 0; kx < filterDim; kx++) {
            int nkx = filterDim - 1 - kx;
            int y = iy + (ky - filterCenter);
            int x = ix + (kx - filterCenter);
            if (x >= 0 && x < (int) width && y >=0 && y < (int) height) {
                aggregate += in[y * width + x] * filter[nky * filterDim + nkx];
            }
        }
    }
    aggregate *= filterFactor;
  if (aggregate > 0) {
    out[iy * width + ix] = (aggregate > 255) ? 255 : aggregate;
  } else {
    out[iy * width + ix] = 0;
  }
}

void help(char const *exec, char const opt, char const *optarg) {
    FILE *out = stdout;
    if (opt != 0) {
        out = stderr;
        if (optarg) {
            fprintf(out, "Invalid parameter - %c %s\n", opt, optarg);
        } else {
            fprintf(out, "Invalid parameter - %c\n", opt);
        }
    }
    fprintf(out, "%s [options] <input-bmp> <output-bmp>\n", exec);
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -i, --iterations <iterations>    number of iterations (1)\n");

    fprintf(out, "\n");
    fprintf(out, "Example: %s in.bmp out.bmp -i 10000\n", exec);
}

int main(int argc, char **argv) {
  /*
    Parameter parsing, don't change this!
   */
  unsigned int iterations = 1;
  char *output = NULL;
  char *input = NULL;
  int ret = 0;

  static struct option const long_options[] =  {
      {"help",       no_argument,       0, 'h'},
      {"iterations", required_argument, 0, 'i'},
      {0, 0, 0, 0}
  };

  static char const * short_options = "hi:";
  {
    char *endptr;
    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
      switch (c) {
      case 'h':
        help(argv[0],0, NULL);
        return 0;
      case 'i':
        iterations = strtol(optarg, &endptr, 10);
        if (endptr == optarg) {
          help(argv[0], c, optarg);
          return ERROR_EXIT;
        }
        break;
      default:
        abort();
      }
    }
  }

  if (argc <= (optind+1)) {
    help(argv[0],' ',"Not enough arugments");
    return ERROR_EXIT;
  }
  input = (char *)calloc(strlen(argv[optind]) + 1, sizeof(char));
  strncpy(input, argv[optind], strlen(argv[optind]));
  optind++;

  output = (char *)calloc(strlen(argv[optind]) + 1, sizeof(char));
  strncpy(output, argv[optind], strlen(argv[optind]));
  optind++;

  /*
    End of Parameter parsing!
   */

  /*
    Create the BMP image and load it from disk.
   */
  bmpImage *image = newBmpImage(0,0);
  if (image == NULL) {
    fprintf(stderr, "Could not allocate new image!\n");
  }

  if (loadBmpImage(image, input) != 0) {
    fprintf(stderr, "Could not load bmp image '%s'!\n", input);
    freeBmpImage(image);
    return ERROR_EXIT;
  }


  // Create a single color channel image. It is easier to work just with one color
  bmpImageChannel *imageChannel = newBmpImageChannel(image->width, image->height);
  if (imageChannel == NULL) {
    fprintf(stderr, "Could not allocate new image channel!\n");
    freeBmpImage(image);
    return ERROR_EXIT;
  }

  // Extract from the loaded image an average over all colors - nothing else than
  // a black and white representation
  // extractImageChannel and mapImageChannel need the images to be in the exact
  // same dimensions!
  // Other prepared extraction functions are extractRed, extractGreen, extractBlue
  if(extractImageChannel(imageChannel, image, extractAverage) != 0) {
    fprintf(stderr, "Could not extract image channel!\n");
    freeBmpImage(image);
    freeBmpImageChannel(imageChannel);
    return ERROR_EXIT;
  }

  int imageSize = image->width * image->height * sizeof(unsigned char);
  unsigned char *cudaRawInImage;
  unsigned char *cudaRawOutImage;
  if (GPU) {
      cudaErrorCheck(cudaMalloc(&cudaRawInImage, imageSize));
      cudaErrorCheck(cudaMalloc(&cudaRawOutImage, imageSize));
      cudaErrorCheck(cudaMemcpy(cudaRawInImage, imageChannel->rawdata, imageSize, cudaMemcpyHostToDevice));
  }

  // Specify which filter to use
  int *filter = (int *) laplacian1Filter;
  int filterSize = sizeof(laplacian1Filter);
  unsigned int filterDim = 3;
  float filterFactor = laplacian1FilterFactor;

  // Copy the filter to device
  int *cudaFilter;
  if (GPU) {
      cudaErrorCheck(cudaMalloc(&cudaFilter, filterSize));
      cudaErrorCheck(cudaMemcpy(cudaFilter, filter, filterSize, cudaMemcpyHostToDevice));
  }

  dim3 threadsPerBlock(8, 8);
  dim3 numBlocks(imageChannel->width / threadsPerBlock.x + 1, imageChannel->height / threadsPerBlock.y + 1);


  //Here we do the actual computation!
  // imageChannel->data is a 2-dimensional array of unsigned char which is accessed row first ([y][x])
  bmpImageChannel *processImageChannel;
  if (CPU) {
    processImageChannel = newBmpImageChannel(imageChannel->width, imageChannel->height);
  }
  for (unsigned int i = 0; i < iterations; i ++) {
    if (CPU) {
        applyFilter(processImageChannel->data,
                    imageChannel->data,
                    imageChannel->width,
                    imageChannel->height,
                    filter, filterDim, filterFactor
     //               (int *)laplacian2Filter, 3, laplacian2FilterFactor
     //               (int *)laplacian3Filter, 3, laplacian3FilterFactor
     //               (int *)gaussianFilter, 5, gaussianFilterFactor
                    );
        // Swap imageChannels for cpu
        swapBmpImageChannels(imageChannel, processImageChannel);
    }

    if (GPU) {
        applyFilterCuda<<<numBlocks, threadsPerBlock>>>(cudaRawOutImage, cudaRawInImage, imageChannel->width, imageChannel->height, cudaFilter, filterDim, filterFactor);

        // Swap the data pointers for gpu
        unsigned char *tmp = cudaRawInImage;
        cudaRawInImage = cudaRawOutImage;
        cudaRawOutImage = tmp;
    }

  }
  if (CPU) {
    freeBmpImageChannel(processImageChannel);
  }
  bmpImageChannel *cudaResultImageChannel;
  if (GPU) { 
      cudaResultImageChannel = newBmpImageChannel(imageChannel->width, imageChannel->height);
        
      cudaErrorCheck(cudaMemcpy(cudaResultImageChannel->rawdata, cudaRawInImage, imageSize, cudaMemcpyDeviceToHost));

      // Free cuda memory
      cudaErrorCheck(cudaFree(cudaRawInImage));
      cudaErrorCheck(cudaFree(cudaRawOutImage));
      cudaErrorCheck(cudaFree(cudaFilter)); 
  }

  if (GPU && CPU) {
      for (unsigned int y = 0; y < imageChannel->height; y++) {
        for (unsigned int x = 0; x < imageChannel->width; x++) {
          if (imageChannel->data[y][x] != cudaResultImageChannel->data[y][x]) {
              unsigned char cpu = imageChannel->data[y][x];
              unsigned char gpu = cudaResultImageChannel->data[y][x];
              printf("cpu: %d != gpu: %d at index (%d, %d)\n", cpu, gpu, x, y);
          }
        }
      }
  }

  // Map our single color image back to a normal BMP image with 3 color channels
  // mapEqual puts the color value on all three channels the same way
  // other mapping functions are mapRed, mapGreen, mapBlue
  if (mapImageChannel(image, imageChannel, mapEqual) != 0) {
    fprintf(stderr, "Could not map image channel!\n");
    freeBmpImage(image);
    freeBmpImageChannel(imageChannel);
    return ERROR_EXIT;
  }
  freeBmpImageChannel(imageChannel);
  if (GPU) {
    freeBmpImageChannel(cudaResultImageChannel);
  }

  //Write the image back to disk
  if (saveBmpImage(image, output) != 0) {
    fprintf(stderr, "Could not save output to '%s'!\n", output);
    freeBmpImage(image);
    return ERROR_EXIT;
  };

  ret = 0;
  if (input)
    free(input);
  if (output)
    free(output);
  return ret;
};
