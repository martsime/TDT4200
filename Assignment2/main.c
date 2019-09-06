#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include "bitmap.h"

#define XSIZE 2560 // Size of before image
#define YSIZE 2048

// Save the sub image done by each process with the name "after{world_rank}.bmp"
#define SAVE_SUB_IMAGES 0

// Scales the image of the factor in both directions
#define SCALE_FACTOR 2

int main() {
    // Initialize the MPI Environment
    MPI_Init(NULL, NULL);

    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    // Raise an error if the number of processes is not a divisor of the image height
    if (YSIZE % world_size != 0) {
        printf("ERROR: Number of processes %d is not a divisor of image height %d\n", world_size, YSIZE);
        MPI_Finalize();
        return 1;
    }
    
    // Number of rows of the image to be processed by each process
    int num_rows = YSIZE / world_size;

    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
    // Declare image pointers
    uchar *old_image = NULL;
    uchar *new_image = NULL;
   
    // Initialize memory for the full images if root process
    if (world_rank == 0) {
        old_image = calloc(XSIZE * YSIZE * 3, 1); // Three uchars per pixel (RGB)
        new_image = calloc(XSIZE * YSIZE * 3 * SCALE_FACTOR * SCALE_FACTOR, 1);
        readbmp("before.bmp", old_image);
    }
    
    // Number of bytes sent to each process
    int bytes_per_process = XSIZE * num_rows * 3;

    // Number of bytes sent from each process and back to root after scaling of image
    int new_bytes_per_process = bytes_per_process * SCALE_FACTOR * SCALE_FACTOR;

    // Initialize buffer for sub image received from root process
    uchar *sub_image = calloc(bytes_per_process, 1);
    
    // Scatter the full image to all processes
    MPI_Scatter(old_image, bytes_per_process, MPI_BYTE, sub_image, bytes_per_process, MPI_BYTE, 0, MPI_COMM_WORLD);

    // Inverting image
    invertbmp(sub_image, XSIZE, num_rows, 3);

    // Initialize memory for the new scaled sub image
    uchar *new_sub_image = calloc(new_bytes_per_process, 1);

    // Scaling image
    scalebmp(sub_image, new_sub_image, XSIZE, num_rows, 3, SCALE_FACTOR);
    
    // Save processed sub images
    if (SAVE_SUB_IMAGES) {
        char filename [20];
        sprintf(filename, "after%d.bmp", world_rank);
        savebmp(filename, new_sub_image, XSIZE * SCALE_FACTOR, num_rows * SCALE_FACTOR);
    }
    
    // Gather all scaled sub images back into the root process and store them in the new_image buffer
    MPI_Gather(new_sub_image, new_bytes_per_process, MPI_BYTE, new_image, new_bytes_per_process, MPI_BYTE, 0, MPI_COMM_WORLD);
    
    // The root process saves the image
    if (world_rank == 0) {
        savebmp("after.bmp", new_image, XSIZE * SCALE_FACTOR, YSIZE * SCALE_FACTOR);
    }
    
    // Free all the buffers used
    free(old_image);
    free(new_image);
    free(sub_image);
    free(new_sub_image);

    // Finalize MPI 
    MPI_Finalize();

	return 0;
}
