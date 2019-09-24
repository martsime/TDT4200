#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <mpi.h>
#include "libs/bitmap.h"
#include "libs/kernel.h"

int* calc_split(int processes, bmpImage *image) {
  /* Calculate how many rows to send to each process */
  int *rows = calloc(processes, sizeof(int));
  int height = image->height;
  
  int rows_per_process = height / processes;
  
  // Check if the image can be split evenly among the number of processes
  if (height % processes == 0) {
    for (unsigned int i = 0; i < processes; i++) {
      rows[i] = rows_per_process;
    }
  } else {
    // Let the processes with higher ranks receive more work than the ones
    // with lower ranks if the image is not evenly divisible by the number 
    // of processes
    for (int i = processes - 1; i >= 0; i--) {
      // Check if the remaining rows are evenly divisble by the rest of the
      // processes which havent been assigned a number of rows yet
      if (height % (i + 1) == 0) {
        rows[i] = rows_per_process;
      } else {
        rows[i] = rows_per_process + 1;
      }
      height -= rows[i];
    }
  }
  return rows;
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
          goto graceful_exit;
        case 'i':
          iterations = strtol(optarg, &endptr, 10);
          if (endptr == optarg) {
            help(argv[0], c, optarg);
            goto error_exit;
          }
          break;
        default:
          abort();
      }
    }
  }

  if (argc <= (optind+1)) {
    help(argv[0],' ',"Not enough arugments");
    goto error_exit;
  }
  input = calloc(strlen(argv[optind]) + 1, sizeof(char));
  strncpy(input, argv[optind], strlen(argv[optind]));
  optind++;

  output = calloc(strlen(argv[optind]) + 1, sizeof(char));
  strncpy(output, argv[optind], strlen(argv[optind]));
  optind++;

  /*
  End of Parameter parsing!
  */

  /*
  Initialize the MPI environment
  */

  MPI_Init(NULL, NULL);

  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  /*
  Create the BMP image and load it from disk.
  */
  bmpImage *image = newBmpImage(0, 0);
  if (image == NULL) {
    fprintf(stderr, "Could not allocate new image!\n");
  }
  bmpImageChannel *imageChannel = NULL;

  // Load and extract image in root process
  if (world_rank == 0) {
    if (loadBmpImage(image, input) != 0) {
      fprintf(stderr, "Could not load bmp image '%s'!\n", input);
      freeBmpImage(image);
      goto error_exit;
    }

    // Create a single color channel image. It is easier to work just with one color
    imageChannel = newBmpImageChannel(image->width, image->height);
    if (imageChannel == NULL) {
      fprintf(stderr, "Could not allocate new image channel!\n");
      freeBmpImage(image);
      goto error_exit;
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
      goto error_exit;
    }
  } else {
    // Load only the image size if not root process
    if (loadBmpImageSizeOnly(image, input) != 0) {
      fprintf(stderr, "Could not load bmp image '%s'!\n", input);
      freeBmpImage(image);
      goto error_exit;
    }
  }
  // TODO: Should be able to take arbitrary image sizes and number of processes
  int rowsPerProcess = image->height / (world_size - 1);
  int bytesPerProcess = rowsPerProcess * image->width;

  bmpImageChannel *subChannel = newBmpImageChannel(image->width, rowsPerProcess);
  unsigned char *channelBuffer = calloc(bytesPerProcess, 1);
  if (world_rank == 0) {
    for (unsigned int i = 1; i < world_size; i++) {
      unsigned char *dataPtr = imageChannel->data[rowsPerProcess * (i - 1)];
      MPI_Send(dataPtr, bytesPerProcess, MPI_BYTE, i, 0, MPI_COMM_WORLD);
    }
  } else {
    MPI_Recv(channelBuffer, bytesPerProcess, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    subChannel->rawdata = channelBuffer;

    if (unbufferBmpImageChannel(subChannel) != 0) {
      fprintf(stderr, "Could not unbuffer bmp image channel'%s'!\n", input);
      freeBmpImage(image);
      goto error_exit;
    }
  }

  // Here we do the actual computation!
  // imageChannel->data is a 2-dimensional array of unsigned char which is accessed row first ([y][x])

  if (world_rank > 0) {
    bmpImageChannel *processImageChannel = newBmpImageChannel(subChannel->width, subChannel->height);
    for (unsigned int i = 0; i < iterations; i ++) {
      applyKernel(processImageChannel->data,
          subChannel->data,
          subChannel->width,
          subChannel->height,
          (int *)laplacian1Kernel, 3, laplacian1KernelFactor
          // (int *)laplacian2Kernel, 3, laplacian2KernelFactor
          // (int *)laplacian3Kernel, 3, laplacian3KernelFactor
          // (int *)gaussianKernel, 5, gaussianKernelFactor
          );
      swapImageChannel(&processImageChannel, &subChannel);
    }
    freeBmpImageChannel(processImageChannel);
  }

  if (world_rank == 0) {
    for (unsigned int i = 1; i < world_size; i++) {
      MPI_Recv(imageChannel->data[rowsPerProcess * (i - 1)], bytesPerProcess, MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
  } else {
    MPI_Send(subChannel->rawdata, bytesPerProcess, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    freeBmpImageChannel(subChannel);
  }

  if (world_rank == 0) {
    // Map our single color image back to a normal BMP image with 3 color channels
    // mapEqual puts the color value on all three channels the same way
    // other mapping functions are mapRed, mapGreen, mapBlue
    if (mapImageChannel(image, imageChannel, mapEqual) != 0) {
      fprintf(stderr, "Could not map image channel!\n");
      freeBmpImage(image);
      freeBmpImageChannel(imageChannel);
      goto error_exit;
    }
    freeBmpImageChannel(imageChannel);

    //Write the image back to disk
    if (saveBmpImage(image, output) != 0) {
      fprintf(stderr, "Could not save output to '%s'!\n", output);
      freeBmpImage(image);
      goto error_exit;
    };
  }

  MPI_Finalize();

graceful_exit:
  ret = 0;
error_exit:
  if (input)
    free(input);
  if (output)
    free(output);
  return ret;
};
