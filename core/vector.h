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
int vectorSet(const Vector* vector, int index, const void* data);
int vectorDelete(Vector* vector);