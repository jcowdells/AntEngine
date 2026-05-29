#pragma once

typedef struct {
    int size;
    int block_size;
    int num_items;
    unsigned char* array;
} Vector;

int vectorCreate(Vector* vector, int size, int block_size);
int vectorAdd(Vector* vector, const void* data);
int vectorGet(const Vector* vector, int index, void* data);
int vectorGetPtr(const Vector* vector, int index, void** data_ptr);
int vectorSet(const Vector* vector, int index, const void* data);
int vectorRemove(Vector* vector, int index);
int vectorDelete(Vector* vector);