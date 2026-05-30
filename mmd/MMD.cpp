#include "MMD.h"

#include "render/opengl/RenderContext.h"

#ifdef MMD_ENABLE_STACKTRACE
#include "backward.hpp"
#endif

namespace mmd {

static InitArgs sInitArgs;

void init(const InitArgs& args) {
    sInitArgs = args;
#ifdef MMD_ENABLE_STACKTRACE
    static backward::SignalHandling sh;
#endif
    RenderContext::instance().init(args.shaderDir, args.toonDir);
}

void dispose() {
    RenderContext::instance().release();
}

const InitArgs& initArgs() {
    return sInitArgs;
}

}  // namespace mmd
