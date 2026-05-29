#define GLFW_INCLUDE_NONE
#define STB_IMAGE_IMPLEMENTATION
#include <stdio.h>
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <time.h>

#include "core/hash_table.h"
#include "core/vector.h"

#define WORKER_ANT 0
#define QUEEN_ANT  1
#define DEAD_ANT   2

typedef struct {
    int num_nests;
    short num_rows;
    short num_columns;
    int nest_food;
    int num_food;
    int num_ants;
    int pheromone_strength;
    int pheromone_decay;

    // ADDITIONAL PARAMETERS
    int food_capacity;
    int food_amount;
} SimulationParameters;

typedef struct {
    GLubyte r, g, b;
} Color;

typedef struct {
    short x, y;
    short nest_x, nest_y;
    int food;
    int food_capacity;
    char type;
} Ant;

typedef struct {
    int food;
    int num_queens;
    Vector ants;
} Nest;

typedef struct {
    int strength;
    int ant;
} Pheromone;

int readFile(const char* filename, char* buffer, const int len_buffer) {
    FILE* file = fopen(filename, "r");
    if (!fgets(buffer, len_buffer, file))
        return -1;
    return 0;
}

int createShader(const GLenum shader_type, const char* shader_buffer, GLuint* shader_id) {
    // load shader
    *shader_id = glCreateShader(shader_type);
    if (!*shader_id || *shader_id == GL_INVALID_ENUM) return -1;
    glShaderSource(*shader_id, 1, &shader_buffer, NULL);
    glCompileShader(*shader_id);

    // check for errors
    int success;
    glGetShaderiv(*shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char message[256];
        glGetShaderInfoLog(*shader_id, 256, NULL, message);
        glDeleteShader(*shader_id);
        printf("%s", message);
        return -1;
    }

    return 0;
}

int createShaderProgram(const char* vertex_shader, const char* fragment_shader, GLuint* shader_program_id) {
    // read shaders
    char vertex_buffer[256], fragment_buffer[256];
    if (readFile(vertex_shader, vertex_buffer, 256) || readFile(fragment_shader, fragment_buffer, 256))
        return -1;

    // compile shaders
    GLuint vertex_shader_id, fragment_shader_id;
    if (createShader(GL_VERTEX_SHADER, vertex_buffer, &vertex_shader_id)
        || createShader(GL_FRAGMENT_SHADER, fragment_buffer, &fragment_shader_id))
        return -1;

    // create shader program
    *shader_program_id = glCreateProgram();
    if (!*shader_program_id)
        return -1;

    // link shader program
    glAttachShader(*shader_program_id, vertex_shader_id);
    glAttachShader(*shader_program_id, fragment_shader_id);
    glLinkProgram(*shader_program_id);

    // delete compiled things
    glDeleteShader(vertex_shader_id);
    glDeleteProgram(fragment_shader_id);

    return 0;
}

int createTexture(const char* texture, GLuint* texture_id) {
    // load texture data using STBI
    stbi_set_flip_vertically_on_load(1);
    int width, height, num_channels;
    unsigned char* image_data = stbi_load(texture, &width, &height, &num_channels, 4);
    if (!image_data)
        return -1;

    // create empty texture
    glGenTextures(1, texture_id);
    glBindTexture(GL_TEXTURE_2D, *texture_id);

    // set texture parameters for wrapping and zooming
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // send image data to OpenGL and create mipmap (different levels of detail)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image_data);
    return 0;
}

void clearBuffer(Color* buffer, const short width, const short height, const Color* clear_color) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            memcpy(buffer + x + y * width, clear_color, sizeof(Color));
        }
    }
}

void attenuateColor(const Color* in_color, Color* out_color, const int factor) {
    out_color->r = in_color->r - (in_color->r >> factor);
    out_color->g = in_color->g - (in_color->g >> factor);
    out_color->b = in_color->b - (in_color->b >> factor);
}

GLubyte linearInterpolate(const GLubyte a, const GLubyte b, const float factor) {
    return a + (GLubyte)((float)(b - a) * factor);
}

void attenuateColorF(const Color* background, const Color* in_color, Color* out_color, const float factor) {
    const float exp_factor = 1.0f - powf(0.5f, factor);

    out_color->r = linearInterpolate(background->r, in_color->r, exp_factor);
    out_color->g = linearInterpolate(background->g, in_color->g, exp_factor);
    out_color->b = linearInterpolate(background->b, in_color->b, exp_factor);
}

void readBuffer(const Color* buffer, const short x, const short y, const short width, Color* color) {
    memcpy(color, buffer + x + y * width, sizeof(Color));
}

void writeBuffer(Color* buffer, const short x, const short y, const short width, const Color* color) {
    memcpy(buffer + x + y * width, color, sizeof(Color));
}

void updateBuffer(Color* buffer, const Color* background, const short width, const Vector* ants, const HashTable* food, const HashTable* nests, const HashTable* pheromones) {
    // major hashtable bodge. but avoids making a new array each iteration etc.
    const Color food_color = {
        0xAA, 0x00, 0x00
    };
    for (int i = 0; i < food->size; i++) {
        const HashNode* loop_node = food->array[i];
        while (loop_node) {
            Color attenuated_color;
            const float food_level = (float)(int)loop_node->data;
            attenuateColorF(background, &food_color, &attenuated_color, food_level / 500.0f);
            writeBuffer(buffer, loop_node->x, loop_node->y, width, &attenuated_color);
            loop_node = loop_node->next;
        }
    }

    // more hashtable bodging
    const Color pheromone_color = {
        0xAA, 0x00, 0xAA
    };
    for (int i = 0; i < pheromones->size; i++) {
        const HashNode* loop_node = pheromones->array[i];
        while (loop_node) {
            Color attenuated_color;
            const Pheromone* pheromone = loop_node->data;
            attenuateColorF(background, &pheromone_color, &attenuated_color, (float)pheromone->strength / 1000.0f);
            writeBuffer(buffer, loop_node->x, loop_node->y, width, &attenuated_color);
            loop_node = loop_node->next;
        }
    }

    const Color nest_color = {
        0xFF, 0xAA, 0x00
    };
    for (int i = 0; i < nests->size; i++) {
        const HashNode* loop_node = nests->array[i];
        while (loop_node) {
            writeBuffer(buffer, loop_node->x, loop_node->y, width, &nest_color);
            loop_node = loop_node->next;
        }
    }

    for (int i = 0; i < ants->num_items; i++) {
        Ant* ant;
        vectorGetPtr(ants, i, (void**)&ant);

        if (ant->type == DEAD_ANT)
            continue;

        const int index = ant->x + ant->y * width;
        // ants should be black
        // every additional ant makes the tile 50% darker.
        buffer[index].r >>= 1;
        buffer[index].g >>= 1;
        buffer[index].b >>= 1;
    }
}

void removeDeadPheromones(const SimulationParameters* sp, HashTable* pheromones) {
    for (int i = 0; i < pheromones->size; i++) {
        HashNode* loop_node = pheromones->array[i];
        HashNode** next = &pheromones->array[i];

        while (loop_node) {
            // decay
            Pheromone* pheromone = loop_node->data;
            pheromone->strength -= sp->pheromone_decay;

            // remove if decayed to nothing
            if (pheromone->strength <= 0) {
                *next = loop_node->next;
                free(loop_node);
                loop_node = *next;
                free(pheromone);
            } else {
                next = &loop_node->next;
                loop_node = loop_node->next;
            }
        }
    }
}

int getRandomFood(const int cell_food, const int curr_food, const int max_food) {
    // better method than randomly picking an amount of food and checking if it works.
    int max = max_food - curr_food;
    if (max > cell_food) max = cell_food;

    if (max <= 1)
        return max;

    return rand() % (max - 1) + 1;
}

void moveAntToNest(Ant* ant) {
    if (ant->x > ant->nest_x) {
        ant->x -= 1;
    } else if (ant->x < ant->nest_x) {
        ant->x += 1;
    }

    if (ant->y > ant->nest_y) {
        ant->y -= 1;
    } else if (ant->y < ant->nest_y) {
        ant->y += 1;
    }
}

void moveAntToPheromones(const SimulationParameters* sp, Ant* ant, const HashTable* pheromones) {
    short sx = 0, sy = 0;
    int strongest = 0;
    for (short y = -1; y <= 1; y++) {
        for (short x = -1; x <= 1; x++) {
            if (x == 0 && y == 0)
                continue;

            // check if pheromone exists
            Pheromone* pheromone;
            if (hashTableGet(pheromones, (short)(ant->x + x), (short)(ant->y + y), (void**)&pheromone)) {
                if (pheromone->strength > strongest) {
                    sx = x;
                    sy = y;
                    strongest = pheromone->strength;
                }
            }
        }
    }

    // if no strongest pheromone, move randomly
    while (sx == 0 && sy == 0) {
        sx = (short)(rand() % 3 - 1);
        sy = (short)(rand() % 3 - 1);

        const int next_x = ant->x + sx;
        const int next_y = ant->y + sy;
        if (next_x < 0 || next_x >= sp->num_rows) {
            sx = 0;
        }
        if (next_y < 0 || next_y >= sp->num_columns) {
            sy = 0;
        }
    }

    ant->x = (short)(ant->x + sx);
    ant->y = (short)(ant->y + sy);
}

int createPheromone(const SimulationParameters* sp, HashTable* pheromones, const int ant, const short x, const short y) {
    Pheromone* pheromone = malloc(sizeof(Pheromone));
    if (!pheromone)
        return -1;

    pheromone->strength = sp->pheromone_strength;
    pheromone->ant = ant;

    hashTablePut(pheromones, x, y, pheromone);

    return 0;
}

int updatePheromone(const SimulationParameters* sp, HashTable* pheromones, const int ant, const short x, const short y) {
    int num_pheromones;
    Pheromone** pheromone_array;
    hashTableGetAll(pheromones, x, y, (void***)&pheromone_array, &num_pheromones);
    if (!num_pheromones) {
        return createPheromone(sp, pheromones, ant, x, y);
    }
    for (int i = 0; i < num_pheromones; i++) {
        if (pheromone_array[i]->ant == ant) {
            pheromone_array[i]->strength += sp->pheromone_strength;
            break;
        }
    }
    return 0;
}

void advanceAnts(const SimulationParameters* sp, const Vector* ants, HashTable* food, const HashTable* nests, HashTable* pheromones) {
    for (int i = 0; i < ants->num_items; i++) {
        Ant* ant;
        vectorGetPtr(ants, i, (void**)&ant);
        if (ant->type == QUEEN_ANT || ant->type == DEAD_ANT)
            continue;

        if (ant->food > 0 && ant->x == ant->nest_x && ant->y == ant->nest_y) {
            Nest* nest;
            hashTableGet(nests, ant->nest_x, ant->nest_y, (void**)&nest);

            // update food in the sensible way, unlike real code.
            nest->food += ant->food;
            ant->food = 0;
        } else {
            void* cell_food = 0;
            hashTableGet(food, ant->x, ant->y, &cell_food);
            if (cell_food > 0 && ant->food == 0) {
                const int food_gathered = getRandomFood((int)cell_food, ant->food, ant->food_capacity);
                hashTablePut(food, ant->x, ant->y, cell_food - food_gathered);
                ant->food = food_gathered;
            } else {
                if (ant->food > 0) {
                    createPheromone(sp, pheromones, i, ant->x, ant->y);
                    moveAntToNest(ant);
                } else {
                    moveAntToPheromones(sp, ant, pheromones);
                }
            }
        }
    }
}

void removeNestAnt(const int i, Nest* nest, const Vector* ants) {
    int ant_id;
    vectorGet(&nest->ants, i, &ant_id);
    Ant* ant;
    vectorGetPtr(ants, ant_id, (void**)&ant);
    if (ant->type == QUEEN_ANT)
        nest->num_queens--;
    ant->type = DEAD_ANT;
}

void createAnt(const SimulationParameters* sp, Ant* ant, const short nest_x, const short nest_y, char type) {
    ant->x = nest_x;
    ant->y = nest_y;
    ant->nest_x = nest_x;
    ant->nest_y = nest_y;
    ant->type = type;
    ant->food = 0;
    switch (type) {
    case WORKER_ANT:
        ant->food_capacity = sp->food_capacity;
        break;
    case QUEEN_ANT:
    default:
        ant->food_capacity = 0;
        break;
    }
}


void addNestAnt(const SimulationParameters* sp, Nest* nest, const short nest_x, const short nest_y, Vector* ants) {
    const int random_create = rand() % 100;
    if (random_create < 50) {
        Ant ant = {};
        const int random_queen = rand() % 100;
        if (random_queen < 2) {
            createAnt(sp, &ant, nest_x, nest_y, QUEEN_ANT);
            nest->num_queens++;
        } else {
            createAnt(sp, &ant, nest_x, nest_y, WORKER_ANT);
        }
        vectorAdd(&nest->ants, &ants->num_items);
        vectorAdd(ants, &ant);
    }
}

void advanceNest(const SimulationParameters* sp, Nest* nest, const short nest_x, const short nest_y, Vector* ants) {
    int food_needed = 0;

    for (int i = 0; i < nest->ants.num_items; i++) {
        int ant_id;
        vectorGet(&nest->ants, i, (void**)&ant_id);

        Ant* ant;
        vectorGetPtr(ants, ant_id, (void**)&ant);

        if (ant->type == QUEEN_ANT) {
            food_needed += 10;
        } else if (ant->type == WORKER_ANT) {
            food_needed += 2;
        }
    }

    nest->food -= food_needed;
    if (nest->food < 0) nest->food = 0;

    int ants_to_cull = 0;
    if (nest->food == 0 && nest->ants.num_items > 0)
        ants_to_cull++;
    if (nest->food < nest->ants.num_items)
        ants_to_cull++;
    if (nest->food < nest->ants.num_items * 5) {
        ants_to_cull++;
        if (ants_to_cull > nest->ants.num_items) {
            // effectively clear the ants list (without memory operations)
            for (int i = 0; i < nest->ants.num_items; i++) {
                removeNestAnt(i, nest, ants);
            }
            nest->ants.num_items = 0;
        } else {
            for (int i = 0; i < ants_to_cull; i++) {
                const int index = rand() % nest->ants.num_items;
                removeNestAnt(index, nest, ants);
                vectorRemove(&nest->ants, index);
            }
        }
    } else {
        for (int i = 0; i < nest->num_queens; i++) {
            addNestAnt(sp, nest, nest_x, nest_y, ants);
        }
    }
}

void advanceNests(const SimulationParameters* sp, Vector* ants, HashTable* food, HashTable* nests, HashTable* pheromones) {
    if (ants->num_items == 0)
        return;

    for (int i = 0; i < nests->num_items; i++) {
        HashNode* loop_node = nests->array[i];

        while (loop_node) {
            Nest* nest = loop_node->data;

            advanceNest(sp, nest, loop_node->x, loop_node->y, ants);

            loop_node = loop_node->next;
        }
    }
}

void advanceStage(const SimulationParameters* sp, Vector* ants, HashTable* food, HashTable* nests, HashTable* pheromones) {
    removeDeadPheromones(sp, pheromones);

    advanceAnts(sp, ants, food, nests, pheromones);

    advanceNests(sp, ants, food, nests, pheromones);
}

int run(const SimulationParameters* sp, Vector* ants, HashTable* food, HashTable* nests, HashTable* pheromones) {
    GLFWwindow* window = glfwCreateWindow(1000, 1000, "Hello World", NULL, NULL);
    if (!window)
        return -1;

    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
        return -1;

    // get window size
    int window_width, window_height;
    glfwGetFramebufferSize(window, &window_width, &window_height);

    const short width = sp->num_rows, height = sp->num_columns;

    // generate buffer that is that size
    Color* buffer = calloc(sizeof(Color), width * height);
    if (!buffer)
        return -1;

    glPixelZoom(
        (float)window_width / (float)width,
        (float)window_height / (float)height
    );
    glWindowPos2i(0, 0);
    glMatrixMode(GL_MODELVIEW);
    if (width % 4 != 0 || height % 4 != 0)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const Color background = {
        0xFF, 0xFF, 0xFF
    };

    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (!glfwWindowShouldClose(window)) {
        struct timespec curr;
        clock_gettime(CLOCK_MONOTONIC, &curr);

        const long delta = (curr.tv_sec - last.tv_sec) * 1000000000l + (curr.tv_nsec - last.tv_nsec);

        if (delta > 0) {
            advanceStage(sp, ants, food, nests, pheromones);
            clock_gettime(CLOCK_MONOTONIC, &last);

            clearBuffer(buffer, width, height, &background);
            updateBuffer(buffer, &background, width, ants, food, nests, pheromones);
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
        glFlush();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    free(buffer);

    return 0;
}

int setupNest(const SimulationParameters* sp, Vector* ants, HashTable* nests, const short x, const short y) {
    // allocate new nest
    Nest* nest = malloc(sizeof(Nest));
    if (!nest)
        return -1;

    // create ant pointer array
    if (vectorCreate(&nest->ants, 1, sizeof(int))) {
        free(nest);
        return -1;
    }

    hashTablePut(nests, x, y, nest);
    nest->food = sp->nest_food;
    nest->num_queens = 0;

    // num items in the ants vector is the index of the next added ant.
    Ant ant;
    createAnt(sp, &ant, x, y, QUEEN_ANT);
    vectorAdd(&nest->ants, &ants->num_items);
    vectorAdd(ants, &ant);

    createAnt(sp, &ant, x, y, WORKER_ANT);
    for (int i = 1; i < sp->num_ants; i++) {
        vectorAdd(&nest->ants, &ants->num_items);
        vectorAdd(ants, &ant);
    }

    return 0;
}

int setupSimulation(const SimulationParameters* sp, Vector* ants, HashTable* food, HashTable* nests) {
    for (int i = 0; i < sp->num_nests; i++) {
        // place nests in areas that are not already taken
        short x;
        short y;

        do {
            x = (short)(rand() % sp->num_rows);
            y = (short)(rand() % sp->num_columns);
        } while (hashTableHas(nests, x, y));

        if (setupNest(sp, ants, nests, x, y))
            return -1;
    }

    for (int i = 0; i < sp->num_food; i++) {
        // place nests in areas that are not already taken
        short x;
        short y;

        do {
            x = (short)(rand() % sp->num_rows);
            y = (short)(rand() % sp->num_columns);
        } while (hashTableHas(nests, x, y));

        void* food_amount;
        if (hashTableHas(food, x, y)) {
            hashTableGet(food, x, y, &food_amount);
        } else {
            food_amount = 0;
        }
        food_amount += sp->food_amount;
        hashTablePut(food, x, y, food_amount);
    }

    return 0;
}

int main(void) {
    if (!glfwInit())
        return -1;

    Vector ants;
    HashTable food, nests, pheromones;

    const SimulationParameters sp = {
        // standard
        2, 10, 10, 500, 3, 6, 1000, 25,

        // additional
        30, 500
    };

    const int failure = vectorCreate(&ants, 1, sizeof(Ant))
    || hashTableCreate(&food, 1, DISALLOW_DUPLICATE)
    || hashTableCreate(&nests, 1, DISALLOW_DUPLICATE)
    || hashTableCreate(&pheromones, 1, ALLOW_DUPLICATE)
    || setupSimulation(&sp, &ants, &food, &nests);

    if (!failure)
        run(&sp, &ants, &food, &nests, &pheromones);

    glfwTerminate();

    vectorDelete(&ants);
    hashTableDelete(&food);
    hashTableFreeAndDelete(&nests);
    hashTableFreeAndDelete(&pheromones);

    return 0;
}