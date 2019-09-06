#include <stdlib.h>
#include <stdio.h>
#include "bitmap.h"

// save 24-bits bmp file, buffer must be in bmp format: upside-down
void savebmp(char *name,uchar *buffer,int x,int y) {
	FILE *f=fopen(name,"wb");
	if(!f) {
		printf("Error writing image to disk.\n");
		return;
	}
	unsigned int size=x*y*3+54;
	uchar header[54]={'B','M',size&255,(size>>8)&255,(size>>16)&255,size>>24,0,
                    0,0,0,54,0,0,0,40,0,0,0,x&255,x>>8,0,0,y&255,y>>8,0,0,1,0,24,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	fwrite(header,1,54,f);
	fwrite(buffer,1,x*y*3,f);
	fclose(f);
}

// read bmp file and store image in contiguous array
void readbmp(char* filename, uchar* array) {
	FILE* img = fopen(filename, "rb");   //read the file
	uchar header[54];
	fread(header, sizeof(uchar), 54, img); // read the 54-byte header

  // extract image height and width from header
	int width = *(int*)&header[18];
	int height = *(int*)&header[22];
	int padding=0;
	while ((width*3+padding) % 4!=0) padding++;

	int widthnew=width*3+padding;
	uchar* data = calloc(widthnew, sizeof(uchar));

	for (int i=0; i<height; i++ ) {
		fread( data, sizeof(uchar), widthnew, img);
		for (int j=0; j<width*3; j+=3) {
			array[3 * i * width + j + 0] = data[j+0];
			array[3 * i * width + j + 1] = data[j+1];
			array[3 * i * width + j + 2] = data[j+2];
		}
	}
	fclose(img); //close the file
}

void invertbmp(uchar* image, int width, int height, int channels) {
    /* Inverts the image colors */
    for (int i = 0; i < width * height * channels; i++) {
        image[i] = 255 - image[i];
    } 
}

void scalebmp(uchar* image, uchar* new_image, int width, int height, int channels, int scale_factor) {
    /* Scales the image by the scale_factor in both directions */
    int new_width = width * scale_factor;

    // Loop over old image
    for (int y = 0; y < height; y++) { // Row number
        for (int x = 0; x < width; x++) { // Column number
            for (int c = 0; c < 3; c++) { // Channel number

                // 3D index = [row][column][channe]
                // 3D index -> 1D index
                int index = y * width * 3 + x * 3 + c;
                uchar color = image[index];

                // Set color of all new pixels
                for (int i = 0; i < scale_factor; i++) {
                    for (int j = 0; j < scale_factor; j++) {
                        int new_x = x * scale_factor + i; // New row number
                        int new_y = y * scale_factor + j; // New column number
                        int new_index = new_y * new_width * 3 + new_x * 3 + c;
                        new_image[new_index] = color;
                    }
                }
            }
        }
    }
}
