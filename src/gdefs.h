#ifndef GDEFS_H
#define GDEFS_H

#ifndef GRID_WIDTH
#define GRID_WIDTH 15
#endif

#ifndef GRID_HEIGHT
#define GRID_HEIGHT 15
#endif

#define GRID_CELL_COUNT (GRID_WIDTH * GRID_HEIGHT)

#define DELETE_COPY(ClassName) \
  ClassName(const ClassName&) = delete;\
  ClassName& operator=(const ClassName&) = delete;

#endif
