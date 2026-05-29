#include "vector.h"

#include <stdlib.h>
#include <string.h>

int vectorCreate(Vector* vector, const int size, const int block_size) {
    // initialise to 0
    vector->size = 0;
    vector->block_size = 0;
    vector->num_items = 0;
    vector->array = 0;

    // checks
    if (size <= 0 || block_size <= 0)
        return -1;

    // mallocate buffer
    vector->array = malloc(size * block_size);
    if (!vector->array)
        return -1;

    // write important data
    vector->size = size;
    vector->block_size = block_size;

    return 0;
}

static int vectorDoubleSize(Vector* vector) {
    // realloc array
    unsigned char* new_array = realloc(vector->array, vector->block_size * vector->size * 2);
    if (!new_array)
        return -1;

    // copy over new data
    vector->array = new_array;
    vector->size = vector->size * 2;

    return 0;
}

static void vectorWrite(const Vector* vector, const int index, const void* data) {
    memcpy(vector->array + index * vector->block_size, data, vector->block_size);
}

static void vectorRead(const Vector* vector, const int index, void* data) {
    memcpy(data, vector->array + index * vector->block_size, vector->block_size);
}

int vectorAdd(Vector* vector, const void* data) {
    if (!vector->array)
        return -1;

    // increase size if needed
    while (vector->num_items >= vector->size) {
        if (vectorDoubleSize(vector))
            return -1;
    }

    // write new item
    vectorWrite(vector, vector->num_items, data);
    vector->num_items += 1;

    return 0;
}

int vectorGet(const Vector* vector, const int index, void* data) {
    // ensure index in range
    if (index < 0 || index >= vector->num_items)
        return -1;

    // copy out data
    vectorRead(vector, index, data);
    return 0;
}

int vectorGetPtr(const Vector* vector, const int index, void** data_ptr) {
    // ensure index in range
    if (index < 0 || index >= vector->num_items)
        return -1;

    *data_ptr = vector->array + index * vector->block_size;
    return 0;
}

int vectorSet(const Vector* vector, const int index, const void* data) {
    // ensure index in range
    if (index < 0 || index >= vector->num_items)
        return -1;

    // copy in data
    vectorWrite(vector, index, data);
    return 0;
}

int vectorRemove(Vector* vector, const int index) {
    // ensure index in range
    if (index < 0 || index >= vector->num_items)
        return -1;

    memmove(
        vector->array + index * vector->block_size,          // overwrite current index
        vector->array + (index + 1) * vector->block_size,    // from back of array
        (vector->num_items - index - 1) * vector->block_size // size of block after this index
    );

    vector->num_items--;

    return 0;
}

int vectorDelete(Vector* vector) {
    free(vector->array);
    vector->size = 0;
    vector->block_size = 0;
    vector->num_items = 0;
    vector->array = 0;
    return 0;
}