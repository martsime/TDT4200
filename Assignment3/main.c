#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <mpi.h>
#include "libs/bitmap.h"
#include "libs/kernel.h"
#include "libs/halo.h"
#include "libs/grid.h"

// Setting to enable/disable border exchange
const int BORDER_EXCHANGE = 1;

// Exchaning borders every HALO_COUNT iterations
const int HALO_COUNT = 1;

// Which kernel to use
int *kernel = (int *)laplacian1Kernel;
const int kernelSize = 3;
const float kernelFactor = laplacian1KernelFactor;

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

  // Pointer to the original image
  bmpImage *image = NULL;

  // Load image in root process
  if (world_rank == 0) {
    image = newBmpImage(0, 0);
    if (image == NULL) {
      fprintf(stderr, "Could not allocate new image!\n");
    }
    if (loadBmpImage(image, input) != 0) {
      fprintf(stderr, "Could not load bmp image '%s'!\n", input);
      freeBmpImage(image);
      goto error_exit;
    }
  }

  // Buffer for distributing image size 
  int imageSize[2] = {};

  // Fill buffer in root process
  if (world_rank == 0) {
    imageSize[0] = image->width;
    imageSize[1] = image->height;
  }

  // Broadcast image size
  MPI_Bcast(imageSize, 2, MPI_INT, 0, MPI_COMM_WORLD);

  int imageWidth = imageSize[0];
  int imageHeight = imageSize[1];

  // Extract image channel in root process
  bmpImageChannel *imageChannel = NULL;
  if (world_rank == 0) {
    // Create a single color channel image. It is easier to work just with one color
    imageChannel = newBmpImageChannel(imageWidth, imageHeight);
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
  } 

  // Creates a grid out of the number of processes
  int gridHeight;
  int gridWidth;
  createImageGrid(world_size, &gridWidth, &gridHeight);

  // Current rank's index into the grid
  int rankRowNumber = world_rank / gridWidth;
  int rankColNumber = world_rank % gridWidth;

  // Arrays of number of rows and columns to be sendt to each process
  int *rowSplit = calcSplit(gridHeight, imageHeight);
  int *colSplit = calcSplit(gridWidth, imageWidth);

  // Process specific numbers
  int rowsToRecv = rowSplit[rankRowNumber];
  int colsToRecv = colSplit[rankColNumber];
  int bytesToRecv = rowsToRecv * colsToRecv;

  // Arrays needed by Scatterv
  int *bytesSplit = calloc(world_size, sizeof(int));
  int *displ = calloc(world_size, sizeof(int));
  displ[0] = 0;
  for (unsigned int r = 0; r < gridHeight; r++) {
    for (unsigned int c = 0; c < gridWidth; c++) {
      int rankNumber = r * gridWidth + c;
      bytesSplit[rankNumber] = rowSplit[r] * colSplit[c];
      if (rankNumber > 0) {
        displ[rankNumber] = displ[rankNumber - 1] + bytesSplit[rankNumber - 1];
      }
    }
  }

  // ImageChannel to be processed by each process
  bmpImageChannel *subChannel = newBmpImageChannel(colsToRecv, rowsToRecv);

  // Pointer to the data being sent
  unsigned char *sendPtr = NULL;

  // Since the displacement array requires each sub sqaure of the image to be
  // in contigious memory, we have to rearrange the image into a new buffer
  bmpImageChannel *sendChannel = NULL;
  if (world_rank == 0) {
    sendChannel = newBmpImageChannel(imageWidth, imageHeight);
    unsigned char *insertPtr = sendChannel->rawdata;

    // Origin of the sub square in the original image
    int xOrigin = 0;
    int yOrigin = 0;
    for (unsigned int r = 0; r < gridHeight; r++) {
      xOrigin = 0;
      for (unsigned int c = 0; c < gridWidth; c++) {
        int subWidth = colSplit[c];
        int subHeight = rowSplit[r];

        for (unsigned int y = 0; y < subHeight; y++) {
          for (unsigned int x = 0; x < subWidth; x++) {
            *insertPtr = imageChannel->data[yOrigin + y][xOrigin + x];
            insertPtr++; 
          }
        }
        xOrigin += colSplit[c]; 
      }
      yOrigin += rowSplit[r];
    }
    sendPtr = sendChannel->rawdata;
  }

  // Scatter the data to all processes
  MPI_Scatterv(
      sendPtr,
      bytesSplit,
      displ,
      MPI_BYTE,
      subChannel->rawdata,
      bytesToRecv,
      MPI_BYTE,
      0,
      MPI_COMM_WORLD
      );
  

  // Allocate temporary storage after each iteration
  bmpImageChannel *processImageChannel = newBmpImageChannel(subChannel->width, subChannel->height);
  
  int haloWidth = (kernelSize - 1) / 2 * HALO_COUNT;
  // Struct with recieve buffers
  imageHalo *recvHalo = newImageHalo(subChannel->width, subChannel->height, haloWidth);
  imageHalo *sendHalo = newImageHalo(subChannel->width, subChannel->height, haloWidth);
  
  // Numbers of elements to send for east and west 
  int hCount = haloWidth * recvHalo->height;

  // Numbers of elements to send for north and south
  int vCount = (recvHalo->width + 2*haloWidth) * haloWidth;

  // Apply the kernel to the image for i iterations
  for (int i = 0; i < iterations; i++) {

    // Check if border exchange should be done
    if (BORDER_EXCHANGE && HALO_COUNT > 0 && (i == 0 || i % HALO_COUNT == 0)) {
      if (rankColNumber > 0) {
        // Recv and send west
        MPI_Recv(
            recvHalo->rawwest,
            hCount,
            MPI_BYTE,
            world_rank - 1,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
        createWestHalo(sendHalo->rawwest, sendHalo->count, subChannel);
        MPI_Send(
            sendHalo->rawwest,
            hCount,
            MPI_BYTE,
            world_rank - 1,
            0,
            MPI_COMM_WORLD
        );
      }

      if (rankColNumber < gridWidth - 1) {
        // Send and recv east
        createEastHalo(sendHalo->raweast, sendHalo->count, subChannel);
        MPI_Send(
            sendHalo->raweast,
            hCount,
            MPI_BYTE,
            world_rank + 1,
            0,
            MPI_COMM_WORLD
        );
        MPI_Recv(
            recvHalo->raweast,
            hCount,
            MPI_BYTE,
            world_rank + 1,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
      }

      if (rankRowNumber > 0) {
        // Recv and send north
        MPI_Recv(
            recvHalo->rawnorth,
            vCount,
            MPI_BYTE,
            world_rank - gridWidth,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
        createNorthHalo(sendHalo->rawnorth, sendHalo->count, subChannel, recvHalo);
        MPI_Send(
            sendHalo->rawnorth,
            vCount,
            MPI_BYTE,
            world_rank - gridWidth,
            0,
            MPI_COMM_WORLD
        );
      }

      if (rankRowNumber < gridHeight - 1) {
        // Send and recv south
        createSouthHalo(sendHalo->rawsouth, sendHalo->count, subChannel, recvHalo);
        MPI_Send(
            sendHalo->rawsouth,
            vCount,
            MPI_BYTE,
            world_rank + gridWidth,
            0,
            MPI_COMM_WORLD
        );
        MPI_Recv(
            recvHalo->rawsouth,
            vCount,
            MPI_BYTE,
            world_rank + gridWidth,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
      }
    }
    
    // Apply kernel
    applyKernel(
      processImageChannel->data,
      subChannel->data,
      sendHalo,
      recvHalo,
      kernel,
      kernelSize,
      kernelFactor
    );

    // Swap channel and halo
    swapImageChannel(&processImageChannel, &subChannel);
    swapHalo(&sendHalo, &recvHalo);
  }
  freeBmpImageChannel(processImageChannel);

  // Gather the result into the root process
  MPI_Gatherv(
    subChannel->rawdata, 
    bytesToRecv,
    MPI_BYTE,
    sendPtr,
    bytesSplit,
    displ,
    MPI_BYTE,
    0,
    MPI_COMM_WORLD
  );
  
  freeImageHalo(recvHalo);
  freeImageHalo(sendHalo);

  // Whole image gathered is stored such that each process'
  // sub image
  if (world_rank == 0) {
    unsigned char *recvPtr = sendPtr;

    // Origin of the sub square in the original image
    int xOrigin = 0;
    int yOrigin = 0;
    for (unsigned int r = 0; r < gridHeight; r++) {
      xOrigin = 0;
      for (unsigned int c = 0; c < gridWidth; c++) {
        int subWidth = colSplit[c];
        int subHeight = rowSplit[r];

        for (unsigned int y = 0; y < subHeight; y++) {
          for (unsigned int x = 0; x < subWidth; x++) {
            imageChannel->data[yOrigin + y][xOrigin + x] = *recvPtr;
            recvPtr++; 
          }
        }
        xOrigin += colSplit[c]; 
      }
      yOrigin += rowSplit[r];
    }
    freeBmpImageChannel(sendChannel);
  }

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
    freeBmpImage(image);
    freeBmpImageChannel(imageChannel);
  }

  // Free all allocated memory
  freeBmpImageChannel(subChannel);
  free(rowSplit);
  free(colSplit);
  free(bytesSplit);
  free(displ);

  // Finalize MPI environment
  MPI_Finalize();

graceful_exit:
  ret = 0;
  if (input)
    free(input);
  if (output)
    free(output);
  return ret;
error_exit:
  if (input)
    free(input);
  if (output)
    free(output);
  return ret;
};
