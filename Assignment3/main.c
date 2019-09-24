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
  /* Method used to print help text */
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
  // Parameter parsing
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

  // Initialize the MPI environment
  MPI_Init(NULL, NULL);

  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  
  // Create the bmpImage
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

    // Extract from the loaded image an average over all colors
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
  
  // Array of rows to be sendt to each process
  int *rowSplit = calc_split(world_size, image);

  // Process specific numbers
  int rowsToRecv = rowSplit[world_rank];
  int bytesToRecv = rowsToRecv * image->width;
  
  // Arrays needed by Scatterv
  int *bytesSplit = calloc(world_size, sizeof(int));
  int *displs = calloc(world_size, sizeof(int));
  displs[0] = 0;
  for (unsigned int i = 0; i < world_size; i++) {
    bytesSplit[i] = rowSplit[i] * image->width;
    if (i > 0) {
      displs[i] = displs[i - 1] + bytesSplit[i - 1];
    }
  }
    
  // ImageChannel to be processed by each process
  bmpImageChannel *subChannel = newBmpImageChannel(image->width, rowsToRecv);
  
  // In root process, set a pointer to the data being sent
  unsigned char *sendPtr;
  if (world_rank == 0) {
    sendPtr = imageChannel->rawdata;
  }
  
  // Scatter the data to all processes
  MPI_Scatterv(
    sendPtr,
    bytesSplit,
    displs,
    MPI_BYTE,
    subChannel->rawdata,
    bytesToRecv,
    MPI_BYTE,
    0,
    MPI_COMM_WORLD
  );

  // Allocate temporary storage after each iteration
  bmpImageChannel *processImageChannel = newBmpImageChannel(subChannel->width, subChannel->height);

  unsigned char *topHalo = calloc(subChannel->width, sizeof(char));
  unsigned char *bottomHalo = calloc(subChannel->width, sizeof(char));

  // Apply the kernel to the image for i iterations
  for (unsigned int i = 0; i < iterations; i ++) {

    // Recieve ghost cells for the top row
    if (world_rank > 0) {
      MPI_Recv(
        topHalo,
        subChannel->width,
        MPI_BYTE,
        world_rank - 1,
        0,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
      );
    }

    // Send bottom row as top ghost cells to rank below
    if (world_rank < world_size - 1) {
      MPI_Send(
        subChannel->data[subChannel->height - 1],
        subChannel->width,
        MPI_BYTE,
        world_rank + 1,
        0,
        MPI_COMM_WORLD
      );
    }
  
    // Recieve ghost cells for the bottom row
    if (world_rank < world_size - 1) {
      MPI_Recv(
        bottomHalo,
        subChannel->width,
        MPI_BYTE,
        world_rank + 1,
        0,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
      );
    }
    
    // Send top row as bottom ghost cells to rank above
    if (world_rank > 0) {
      MPI_Send(
        subChannel->data[0],
        subChannel->width,
        MPI_BYTE,
        world_rank - 1,
        0,
        MPI_COMM_WORLD
      );
    }
    
    // Apply kernel
    applyKernel(processImageChannel->data,
        subChannel->data,
        subChannel->width,
        subChannel->height,
        topHalo,
        bottomHalo,
        (int *)laplacian1Kernel, 3, laplacian1KernelFactor
        // (int *)laplacian2Kernel, 3, laplacian2KernelFactor
        // (int *)laplacian3Kernel, 3, laplacian3KernelFactor
        // (int *)gaussianKernel, 5, gaussianKernelFactor
        );
    swapImageChannel(&processImageChannel, &subChannel);
  }
  freeBmpImageChannel(processImageChannel);
  
  // Gather the result into the root process
  MPI_Gatherv(
    subChannel->data[0], 
    bytesToRecv,
    MPI_BYTE,
    sendPtr,
    bytesSplit,
    displs,
    MPI_BYTE,
    0,
    MPI_COMM_WORLD
  );

  // In the root process map and save the received image
  if (world_rank == 0) {
    // Map our single color image back to a normal BMP image with 3 color channels
    if (mapImageChannel(image, imageChannel, mapEqual) != 0) {
      fprintf(stderr, "Could not map image channel!\n");
      freeBmpImage(image);
      freeBmpImageChannel(imageChannel);
      goto error_exit;
    }

    // Write the image back to disk
    if (saveBmpImage(image, output) != 0) {
      fprintf(stderr, "Could not save output to '%s'!\n", output);
      freeBmpImage(image);
      goto error_exit;
    };
  }
  
  // Free all allocated memory
  freeBmpImageChannel(subChannel);
  freeBmpImage(image);
  free(rowSplit);
  free(bytesSplit);
  free(displs);
  
  // Finalize MPI environment
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
