#include "MMD.h"
#include "util/Log.h"
#include "render/opengl/Pipeline.h"
#include "render/opengl/RenderContext.h"

#ifdef MMD_ENABLE_STACKTRACE
#include "backward.hpp"
#endif

extern "C" int gladLoadGL();

namespace mmd {

static InitArgs sInitArgs;

void init(const InitArgs& args) {
    sInitArgs = args;
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
