#define GLFW_INCLUDE_NONE
#define STB_IMAGE_IMPLEMENTATION
#include <stdio.h>
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "core/hash_table.h"
#include "core/vector.h"

#define WORKER_ANT 0
#define QUEEN_ANT  1

typedef struct {
    int num_nests;
    int num_rows;
    int num_columns;
    int nest_food;
    int num_food;
    int num_ants;
    int pheromone_strength;
    int pheromone_decay;
} SimulationParameters;

typedef struct {
    GLubyte r, g, b;
} Color;

typedef struct {
    short x, y;
    short nest_x, nest_y;
    int food_capacity;
    char type;
} Ant;

typedef struct {
    int food;
    int num_queens;
    Vector ants;
} Nest;

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

int run(Vector* ants, HashTable* food, HashTable* nests, HashTable* pheromones) {
    GLFWwindow* window = glfwCreateWindow(1000, 1000, "Hello World", NULL, NULL);
    if (!window)
        return -1;

    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
        return -1;

    // get window size
    int window_width, window_height;
    glfwGetFramebufferSize(window, &window_width, &window_height);

    // generate buffer that is that size
    Color* buffer = calloc(sizeof(Color), window_width * window_height);
    if (!buffer)
        return -1;

    //glPixelZoom(2.0f, 2.0f);
    glWindowPos2i(0, 0);
    glMatrixMode(GL_MODELVIEW);


    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawPixels(window_width, window_height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
        glFlush();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    free(buffer);

    return 0;
}

void createAnt(Ant* ant, const short nest_x, const short nest_y, char type) {
    ant->x = nest_x;
    ant->y = nest_y;
    ant->nest_x = nest_x;
    ant->nest_y = nest_y;
    ant->type = type;
    switch (type) {
    case WORKER_ANT:
        ant->food_capacity = 30;
        break;
    case QUEEN_ANT:
    default:
        ant->food_capacity = 0;
        break;
    }
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

    // num items in the ants vector is the index of the next added ant.
    Ant ant;
    createAnt(&ant, x, y, QUEEN_ANT);
    vectorAdd(&nest->ants, &ants->num_items);
    vectorAdd(ants, &ant);

    createAnt(&ant, x, y, WORKER_ANT);
    for (int i = 1; i < sp->num_ants; i++) {
        vectorAdd(&nest->ants, &ants->num_items);
        vectorAdd(ants, &ant);
    }

    return 0;
}

int setupSimulation(const SimulationParameters* sp, Vector* ants, HashTable* food, HashTable* nests, HashTable* pheromones) {
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
        food_amount += 500;
        hashTablePut(food, x, y, food_amount);
    }

    return 0;
}

int main(void) {
    if (!glfwInit())
        return -1;

    Vector ants;
    HashTable food, nests, pheromones;

    const int failure = vectorCreate(&ants, 1, sizeof(Ant))
    || hashTableCreate(&food, 1, DISALLOW_DUPLICATE)
    || hashTableCreate(&nests, 1, DISALLOW_DUPLICATE)
    || hashTableCreate(&pheromones, 1, ALLOW_DUPLICATE);

    if (!failure)
        run(&ants, &food, &nests, &pheromones);

    glfwTerminate();

    vectorDelete(&ants);
    hashTableDelete(&food);
    hashTableFreeAndDelete(&nests);
    hashTableDelete(&pheromones);

    return 0;
}