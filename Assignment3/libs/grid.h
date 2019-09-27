#ifndef GRID_H
#define GRID_H

// Methods to  calculate how to split the image based on number of processes

int* calcSplit(int processes, int totalCells);
void createImageGrid(int processes, int* gridWidth, int* gridHeight);

#endif
