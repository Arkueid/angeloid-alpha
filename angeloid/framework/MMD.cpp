#include "framework/MMD.h"
#include "core/util/Log.h"
#include "framework/opengl/Pipeline.h"
#include "framework/opengl/RenderContext.h"

#include <cstdio>   // printf, fprintf, vprintf, vfprintf

#ifdef MMD_ENABLE_STACKTRACE
#include "backward.hpp"
#endif

extern "C" int gladLoadGL();

namespace mmd {
namespace {

// Default printf-based logger. Registered in init() if user provides no custom LogFunc.
void defaultLogFunc(LogLevel level, const char* module,
                    const char* fmt, va_list args) {
    switch (level) {
    case LogLevel::Error:
        fprintf(stderr, "[ERROR][%s] ", module);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        break;
    case LogLevel::Warn:
        printf("[WARN][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    case LogLevel::Debug:
        printf("[DEBUG][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    case LogLevel::Info:
    default:
        printf("[INFO][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    }
}

}  // namespace

static InitArgs sInitArgs;

void init(const InitArgs& args) {
    sInitArgs = args;
    setLogFunc(args.logFunc ? args.logFunc : defaultLogFunc);
#ifdef MMD_ENABLE_STACKTRACE
    static backward::SignalHandling sh;
#endif
    RenderContext::instance().init(args.toonDir);
    Pipeline::instance().init(args.effectsCfg, args.shaderDir);
}

void glInit() {
    if (!gladLoadGL()) {
        MMD_ERROR("MMD", "Failed to initialize OpenGL (glad)");
    }
}

void dispose() {
    Pipeline::instance().clear();
    RenderContext::instance().release();
}

const InitArgs& initArgs() {
    return sInitArgs;
}

}  // namespace mmd
