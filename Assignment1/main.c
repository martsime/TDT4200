#include <stdlib.h>
#include <stdio.h>
#include "bitmap.h"

#define XSIZE 2560 // Size of before image
#define YSIZE 2048


uchar* scale(uchar* old_image, unsigned int scale_factor) {
    uchar *new_image = calloc(scale_factor * scale_factor * XSIZE * YSIZE * 3, 1);

    const int NEW_XSIZE = XSIZE * scale_factor;

    // Loop over old image
    for (int y = 0; y < YSIZE; y++) { // Row number
        for (int x = 0; x < XSIZE; x++) { // Column number
            for (int c = 0; c < 3; c++) { // Channel number

                // 3D index = [row][column][channe]
                // 3D index -> 1D index
                int index = y * XSIZE * 3 + x * 3 + c;
                uchar color = old_image[index];

                // Set color of all new pixels
                for (int i = 0; i < scale_factor; i++) {
                    for (int j = 0; j < scale_factor; j++) {
                        int new_x = x * scale_factor + i; // New row number
                        int new_y = y * scale_factor + j; // New column number
                        int new_index = new_y * NEW_XSIZE * 3 + new_x * 3 + c;
                        new_image[new_index] = color;
                    }
                }
            }
        }
    }

    return new_image;
}

int main() {
    uchar *image = calloc(XSIZE * YSIZE * 3, 1); // Three uchars per pixel (RGB)
    readbmp("before.bmp", image);

    // Altering image
    for (int i = 0; i < XSIZE * YSIZE * 3; i++) {
        // Invert the color
        image[i] = 255 - image[i];
    }

    // Scale image
    unsigned int scale_factor = 2;
    uchar *new_image = scale(image, scale_factor);

    savebmp("after.bmp", new_image, XSIZE * scale_factor, YSIZE * scale_factor);
    free(image);
    free(new_image);
    return 0;
}
