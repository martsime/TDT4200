#ifndef BITMAP_H
#define BITMAP_H


typedef unsigned char uchar;
void savebmp(char *name, uchar *buffer, int x, int y);
void readbmp(char *filename, uchar *array);
void invertbmp(uchar* image, int width, int height, int channels);
uchar* scalebmp(uchar* image, int width, int height, int channels, int scale_factor);
#endif
