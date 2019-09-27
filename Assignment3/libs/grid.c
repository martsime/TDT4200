#include <stdlib.h>
#include "grid.h"

int* calcSplit(int processes, int totalCells) {
  /* Calculate how many cells to send to each process */

  int *cells = calloc(processes, sizeof(int));
  int cellsPerProcess = totalCells / processes;

  // Check if the cells can be split evenly among the number of processes
  if (totalCells % processes == 0) {
    for (unsigned int i = 0; i < processes; i++) {
      cells[i] = cellsPerProcess;
    }
  } else {
    // Let the processes with higher ranks receive more work than the ones
    // with lower ranks if the cells is not evenly divisible by the number
    // of processes
    for (int i = processes - 1; i >= 0; i--) {
      // Check if the remaining cells are evenly divisble by the rest of the
      // processes which havent been assigned a number of cells yet
      if (totalCells % (i + 1) == 0) {
        cells[i] = cellsPerProcess;
      } else {
        cells[i] = cellsPerProcess + 1;
      }
      totalCells -= cells[i];
    }
  }
  return cells;
}

void createImageGrid(int processes, int* gridWidth, int* gridHeight) {
  /* Creates a grid out of the number of processes */

  // The algorithm prefers more rows to columns, but the numbers are as close as possible
  // I.e 12 processes -> 4 rows and 3 columns
  int rows = processes;
  int columns = 1;
  for (unsigned int i = processes - 1; i > 0; i--) {
    if (processes % i == 0) {
      int r = i;
      int c = processes / i;
      if (r >= c && r < rows && c > columns) {
        rows = r;
        columns = c;
      }
    }
  }
  *gridHeight = rows;
  *gridWidth = columns;
}
