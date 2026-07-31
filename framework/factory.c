#include <stdlib.h>
#include <string.h>
#include "factory.h"

int Factory_random_int(int min, int max) {
    if (min > max) return min;
    return min + (rand() % (max - min + 1));
}

double Factory_random_double(double min, double max) {
    if (min > max) return min;
    double scale = rand() / (double)RAND_MAX;
    return min + scale * (max - min);
}

const char *Factory_random_element(const char **elements) {
    if (!elements || !elements[0]) return "";
    int count = 0;
    while (elements[count] != NULL) {
        count++;
    }
    return elements[rand() % count];
}

const char *Factory_random_string(Arena *arena, int length) {
    if (length <= 0) return "";
    char *str = (char *)Arena_alloc(arena, length + 1);
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < length; i++) {
        int key = rand() % (sizeof(charset) - 1);
        str[i] = charset[key];
    }
    str[length] = '\0';
    return str;
}
