#pragma once
#include <cstdio>

#define MMD_INFO(mod, fmt, ...)  printf("[INFO][%s] " fmt "\n", mod, ##__VA_ARGS__)
#define MMD_WARN(mod, fmt, ...)  printf("[WARN][%s] " fmt "\n", mod, ##__VA_ARGS__)
#define MMD_ERROR(mod, fmt, ...) fprintf(stderr, "[ERROR][%s] " fmt "\n", mod, ##__VA_ARGS__)

#ifdef MMD_DEBUG_ENABLED
#define MMD_DEBUG(mod, fmt, ...) printf("[DEBUG][%s] " fmt "\n", mod, ##__VA_ARGS__)
#else
#define MMD_DEBUG(mod, fmt, ...) ((void)0)
#endif
