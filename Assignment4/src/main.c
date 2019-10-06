
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <complex.h>
#include <time.h>
#include "libs/bitmap.h"
#include "libs/utilities.h"
#include "main.h"

// colourGradientSteps is defined in main.h
// first step MUST be 0.0
// last step MUST be 1.0
static const struct colourGradientStep colourGradient[colourGradientSteps] = {
	{ .step = 0.0 , { .r = 0  , .g = 0  , .b = 0   }},
	{ .step = 0.03, { .r = 0  , .g = 7  , .b = 100 }},
	{ .step = 0.16, { .r = 32 , .g = 107, .b = 203 }},
	{ .step = 0.42, { .r = 237, .g = 255, .b = 255 }},
	{ .step = 0.64, { .r = 255, .g = 170, .b = 0   }},
	{ .step = 0.86, { .r = 0  , .g = 2  , .b = 0   }},
	{ .step = 1.0,  { .r = 0  , .g = 0  , .b = 0   }}
};

// Gets initialized after parameters are parsed
static pixel *colourMap = NULL;
static unsigned int colourMapSize = 0;

static unsigned int res = 2048;
static unsigned int maxDwell = 512;

// Const log2
static const double const_log2= 0.693147180559945309417232121458176568075500134360255254120;

void computeDwellBuffer(unsigned long long **buffer, double complex cmin, double complex cmax) {
  
  // Calculate these values only once
  double crealDiff = creal(cmax - cmin);
  double crealMin = creal(cmin);
  double cimagDiff = cimag(cmax - cmin);
  double cimagMin = cimag(cmin);
  
  // Loop over every pixel
	for (unsigned int y = 0; y < res; y++) {
    for (unsigned int x = 0; x < res; x++) {
    
      double real = ((double) x / res) * crealDiff + crealMin;
      double imag = ((double) y / res) * cimagDiff + cimagMin;
      double complex initValue = real + imag * I;
      double complex z = real + imag * I;
      unsigned int const dwellInc = 1;
      unsigned long long dwell = 0;

      // Exit condition: dwell is maxDwell or |z| >= 4
      while(dwell < maxDwell && (cabs(z) < (4.0))) {
        // z = z² + initValue
        z = z * z + initValue;
        dwell += dwellInc;
      }

		  buffer[y][x] = dwell;
    }
  }
}

void mapDwellBuffer(bmpImage *image, unsigned long long **buffer) {

  // Allocate memory for colours only once
  pixel *rawColours = malloc(res * res * sizeof(pixel));
  pixel *colour = rawColours;
	for (unsigned int y = 0; y < res; y++) {
    for (unsigned int x = 0; x < res; x++) {
      
      // getDwellColour
      double complex z = x + y * I;
      unsigned long long index = buffer[y][x] + 1 - log(log(cabs(z)/const_log2));
      *colour = colourMap[index % colourMapSize];

			image->data[y][x].r = colour->r;
			image->data[y][x].g = colour->g;
			image->data[y][x].b = colour->b;
      colour++;
		}
	}
  free(rawColours);
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
	fprintf(out, "Mandelbrot Set Renderer\n\n");
	fprintf(out, "  -x [0;1]         Center of Re[-1.5;0.5] (default=0.5)\n");
	fprintf(out, "  -y [0;1]         Center of Im[-1;1] (default=0.5)\n");
	fprintf(out, "  -s (0;1]         Inverse scaling factor (default=1)\n");
	fprintf(out, "  -r [pixel]       Image resolution (default=2048)\n");
	fprintf(out, "  -i [iterations]  Iterations or max dwell (default=512)\n");
	fprintf(out, "  -c [colours]     Colour map iterations (default=1)\n");
	fprintf(out, "%s [options]  <output-bmp>\n", exec);
}

int main( int argc, char *argv[] )
{
	int ret = 1;
	/* Standard Values */
	char *output = NULL; //output image filepath
	bmpImage *image = NULL; //output image
	unsigned long long *rawDwellBuffer = NULL;
	unsigned long long **dwellBuffer = NULL;
	double x = 0.5, y = 0.5; // coordinates
	double scale = 1; // scaling factor
	unsigned int colourIterations = 1; //how many times the colour gradient is repeated
	bool quiet = false; //output something or not

	/* Parameter parsing... */
	{
		char c;
		while((c = getopt(argc,argv,"x:y:s:r:i:c:qh"))!=-1) {
			switch(c) {
				case 'x':
					x = clampDouble(atof(optarg),0.0,1.0);
					break;
				case 'y':
					y = clampDouble(atof(optarg),0.0,1.0);
					break;
				case 's':
					scale = clampDouble(atof(optarg),0.0,1.0);
					if (scale == 0) scale = 1;
					break;
				case 'r':
					res = atoi(optarg);
					break;
				case 'i':
					maxDwell = atoi(optarg);
					break;
				case 'c':
					colourIterations = atoi(optarg);
					break;
				case 'q':
					quiet = true;
					break;
				case 'h':
					help(argv[0],0, NULL);
					goto exit_graceful;
					break;
				default:
					abort();
			}
		}
	}

	if (argc <= (optind)) {
		help(argv[0],' ',"Not enough arugments");
		goto error_exit;
	}

	output = calloc(strlen(argv[optind]) + 1, sizeof(char));
	strncpy(output, argv[optind], strlen(argv[optind]));
	optind++;

	/* Initialize the colourMap, which assigns each possible dwell Value a colour */
	/* This might look complex but is just eye candy. */
	/* Could be made much simpler, thus less fancy */

	colourMapSize = maxDwell / colourIterations;
	colourMap = malloc(colourMapSize * sizeof(pixel));
	initColourMap(colourMap, colourMapSize, colourGradient);

	/* Putting some magic numbers in place...  */

	double const xmin = -3.5 + (2 * 2 * x);
	double const xmax = -1.5 + (2 * 2 * x);
	double const ymin = -3.0 + (2 * 2 * y);
	double const ymax = -1.0 + (2 * 2 * y);
	double const xlen = fabs(xmin - xmax);
	double const ylen = fabs(ymin - ymax);

	/* This is our complex area that is going to be plotted */
	double complex const cmin = (xmin + (0.5 * (1 - scale) * xlen)) + (ymin + (0.5 * (1 - scale) * ylen)) * I;
	double complex const cmax = (xmax - (0.5 * (1 - scale) * xlen)) + (ymax - (0.5 * (1 - scale) * ylen)) * I;

	/* Output useful informations... */
	if (!quiet) {
		printf("Center:      [%f,%f]\n",x,y);
		printf("Zoom:        %llu%%\n", (unsigned long long) (1/scale) * 100);
		printf("Iterations:  %u\n", maxDwell);
		printf("Window:      Re[%f,%f], Im[%f,%f]\n", creal(cmin), creal(cmax), cimag(cmin), cimag(cmax));
		printf("Output:      %s\n", output);
	}


	// Allocate bmp image
	image = newBmpImage(res, res);
	if (image == NULL) {
		fprintf(stderr, "ERROR: could not allocate bmp image space!\n");
		goto error_exit;
	}

	// Allocate the Dwell buffer
	rawDwellBuffer = malloc(res * res * sizeof(unsigned long long));
	if (rawDwellBuffer == NULL) {
		fprintf(stderr, "ERROR: could not allocate raw dwell buffer\n");
		goto error_exit;
	}
	dwellBuffer = malloc(res * sizeof(unsigned long long *));
  if (dwellBuffer == NULL) {
		fprintf(stderr, "ERROR: could not allocate dwell buffer\n");
    free(rawDwellBuffer);
		goto error_exit;
  }

  // Map 2D dwellBuffer to contiguous array rawDwellBuffer
	for (unsigned int y = 0; y < res; y++) {
    dwellBuffer[y] = &(rawDwellBuffer[y * res]);
	}

	//Compute the dwell buffer
	computeDwellBuffer(dwellBuffer, cmin, cmax);

	//Map the dwell buffer to the bmpImage with fancy colors
	mapDwellBuffer(image, dwellBuffer);
  
	// Save the Image
	if(saveBmpImage(image, output)) {
		fprintf(stderr, "ERROR: could not save image to %s\n", output);
		goto error_exit;
	}

	goto exit_graceful;
error_exit:
	ret = 1;
exit_graceful:
	if(image)
		freeBmpImage(image);
	if(colourMap)
		free(colourMap);

  free(dwellBuffer);
  free(rawDwellBuffer);

	return 0;
}

