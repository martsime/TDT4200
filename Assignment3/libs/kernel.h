// Convolutional Kernel Examples, each with dimension 3,
// gaussian kernel with dimension 5
// If you apply another kernel, remember not only to exchange
// the kernel but also the kernelFactor and the correct dimension.

static int const sobelYKernel[] = {
  -1, -2, -1,
  0, 0, 0,
  1, 2, 1
};
static float const sobelYKernelFactor = (float) 1.0;

static int const sobelXKernel[] = {
  -1, -0, -1,
  -2,  0, -2,
  -1,  0, -1,
  0
};
static float const sobelXKernelFactor = (float) 1.0;

static int const laplacian1Kernel[] = {
  -1,  -4,  -1,
  -4,  20,  -4,
  -1,  -4,  -1
};
static float const laplacian1KernelFactor = (float) 1.0;

static int const laplacian2Kernel[] = {
  0,  1,  0,
  1, -4,  1,
  0,  1,  0
};
static float const laplacian2KernelFactor = (float) 1.0;

static int const laplacian3Kernel[] = {
  -1,  -1,  -1,
  -1,   8,  -1,
  -1,  -1,  -1
};
static float const laplacian3KernelFactor = (float) 1.0;


//Bonus Kernel:

static int const gaussianKernel[] = {
  1,  4,  6,  4, 1,
  4, 16, 24, 16, 4,
  6, 24, 36, 24, 6,
  4, 16, 24, 16, 4,
  1,  4,  6,  4, 1
};
static float const gaussianKernelFactor = (float) 1.0 / 256.0;

void applyKernel(
  unsigned char **out,
  unsigned char **in,
  unsigned int width,
  unsigned int height,
  unsigned char *topHalo,
  unsigned char *bottomHalo,
  int *kernel,
  unsigned int kernelDim,
  float kernelFactor
);
