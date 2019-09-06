#include <stdlib.h>
#include <stdio.h>
#include "bitmap.h"

#define XSIZE 2560 // Size of before image
#define YSIZE 2048


int main() {
    uchar *image = calloc(XSIZE * YSIZE * 3, 1); // Three uchars per pixel (RGB)
    readbmp("before.bmp", image);

    // Invert the image
    invertbmp(image, XSIZE, YSIZE, 3);

    // Scale image
    unsigned int scale_factor = 2;
    uchar *new_image = scalebmp(image, XSIZE, YSIZE, 3, scale_factor);

    savebmp("after.bmp", new_image, XSIZE * scale_factor, YSIZE * scale_factor);
    free(image);
    free(new_image);
    return 0;
}
