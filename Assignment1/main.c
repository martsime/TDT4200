#include <stdlib.h>
#include <stdio.h>
#include "bitmap.h"

#define XSIZE 2560 // Size of before image
#define YSIZE 2048

int main() {
	uchar *image = calloc(XSIZE * YSIZE * 3, 1); // Three uchars per pixel (RGB)
	readbmp("before.bmp", image);

    // Altering image
    for (int i = 0; i < XSIZE * YSIZE * 3; i++) {
        // Invert the color
        image[i] = 255 - image[i];
    }

	savebmp("after.bmp", image, XSIZE, YSIZE);
	free(image);
	return 0;
}
