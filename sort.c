
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__); fflush(stdout);

#define DJ_OPENGL_IMPLEMENTATION
#include "dj_opengl.h"

#ifndef PATH_TO_SRC_DIRECTORY
#   define PATH_TO_SRC_DIRECTORY "./"
#endif

struct Window {
    const char *name;
    int32_t width, height;
    struct {
        int32_t major, minor;
    } glversion;
    GLFWwindow* handle;
} g_window = {
    "Bitonic Sort",
    720, 720,
    {4, 5},
    NULL
};

enum {
    BUFFER_ARRAY,

    BUFFER_COUNT
};

enum {
    PROGRAM_SORT,

    PROGRAM_COUNT
};

struct OpenGL {
    GLuint programs[PROGRAM_COUNT];
    GLuint buffers[BUFFER_COUNT];
} g_gl = {
    {0},
    {0}
};

typedef struct {
    uint32_t *data;
    uint32_t size;
} Array;

#define PATH_TO_SHADER_DIRECTORY PATH_TO_SRC_DIRECTORY "./"

static bool LoadSortProgram()
{
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_SORT];

    djgp_push_string(djgp, "#define LOCATION_ARRAY %i\n", BUFFER_ARRAY);
    djgp_push_file(djgp, PATH_TO_SHADER_DIRECTORY "sort.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadPrograms()
{
    bool success = true;

    if (success) success = LoadSortProgram();

    return success;
}
#undef PATH_TO_SHADER_DIRECTORY

GLuint LoadArrayBuffer(const Array *array)
{
    GLuint *buffer = &g_gl.buffers[BUFFER_ARRAY];

    if (glIsBuffer(*buffer))
        glDeleteBuffers(1, buffer);

    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    array->size * sizeof(array->data[0]),
                    array->data,
                    GL_MAP_READ_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_ARRAY, *buffer);

    return glGetError() == GL_NO_ERROR;
}

bool LoadBuffers(const Array *array)
{
    bool success = true;

    if (success) success = LoadArrayBuffer(array);

    return success;
}

bool Load(const Array *array)
{
    bool success = true;

    if (success) success = LoadPrograms();
    if (success) success = LoadBuffers(array);

    return success;
}

void Release()
{
    glDeleteBuffers(BUFFER_COUNT, g_gl.buffers);

    for (int i = 0; i < PROGRAM_COUNT; ++i)
        glDeleteProgram(g_gl.programs[i]);
}

void SortArray(const Array *array)
{
    const GLuint program = g_gl.programs[PROGRAM_SORT];
    glUseProgram(program);
    glUniform1ui(glGetUniformLocation(program, "u_ArraySize"), array->size);

    for (uint32_t d2 = 1u; d2 < array->size; d2*= 2u) {
        for (uint32_t d1 = d2; d1 >= 1u; d1/= 2u) {
            glUniform2ui(glGetUniformLocation(program, "u_LoopValue"), d1, d2);
            glDispatchCompute(array->size / 64, 1, 1);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }
    }
}

typedef struct {
    double min, max, median, mean;
} BenchStats;

static int CompareCallback(const void * a, const void * b)
{
    if (*(double*)a > *(double*)b) {
        return 1;
    } else if (*(double*)a < *(double*)b) {
        return -1;
    } else {
        return 0;
    }
}

BenchStats Bench(void (*SortCallback)(const Array *array), const Array *array)
{
#ifdef FLAG_BENCH
    const int32_t runCount = 100;
#else
    const int32_t runCount = 1;
#endif
    double *times = (double *)malloc(sizeof(*times) * runCount);
    double timesTotal = 0.0;
    BenchStats stats = {0.0, 0.0, 0.0, 0.0};
    djg_clock *clock = djgc_create();

    for (int32_t runID = 0; runID < runCount; ++runID) {
        double cpuTime = 0.0, gpuTime = 0.0;

        LoadArrayBuffer(array);
        glFinish();
        djgc_start(clock);
        (*SortCallback)(array);
        djgc_stop(clock);
        glFinish();
        djgc_ticks(clock, &cpuTime, &gpuTime);

        times[runID] = gpuTime;
        timesTotal+= gpuTime;
    }

    qsort(times, runCount, sizeof(times[0]), &CompareCallback);

    stats.min = times[0];
    stats.max = times[runCount - 1];
    stats.median = times[runCount / 2];
    stats.mean = timesTotal / runCount;

    free(times);
    djgc_release(clock);

    return stats;
}

void GetArray(Array *array)
{
    const GLuint *buffer = &g_gl.buffers[BUFFER_ARRAY];
    const uint32_t *data = (uint32_t *)glMapNamedBuffer(*buffer, GL_READ_ONLY);

    memcpy(array->data,
           data,
           array->size * sizeof(array->data[0]));

    glUnmapNamedBuffer(*buffer);
}


int main(int argc, char **argv)
{
    Array array = { NULL, 1u << 18 };

    if (argc > 1) {
        array.size = 1u << atoi(argv[1]);
    }

    array.data = (uint32_t *)malloc(array.size * sizeof(uint32_t));
    srand(1219);

    for (uint32_t dataID = 0; dataID < array.size; ++dataID) {
        array.data[dataID] = rand();
    }

    LOG("Loading {OpenGL Window}");
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, g_window.glversion.major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, g_window.glversion.minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
    g_window.handle = glfwCreateWindow(g_window.width,
                                       g_window.height,
                                       g_window.name,
                                       NULL, NULL);
    if (g_window.handle == NULL) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }
    glfwMakeContextCurrent(g_window.handle);

    // load OpenGL functions
    LOG("Loading {OpenGL Functions}");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }

    // initialize
    LOG("Loading {Demo}");
    if (!Load(&array)) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }

    // execute sorting kernel
    {
        const BenchStats stats = Bench(&SortArray, &array);

        LOG("Sort -- median/mean/min/max (ms): %f / %f / %f / %f",
            stats.median * 1e3,
            stats.mean * 1e3,
            stats.min * 1e3,
            stats.max * 1e3);

        GetArray(&array);

        for (uint32_t dataID = 1; dataID < array.size; ++dataID) {
            if (array.data[dataID] < array.data[dataID - 1]) {
                printf("failed!\n");
            }
        }
    }

    LOG("All done!");

    Release();
    glfwTerminate();
    free(array.data);

    return 0;
}
