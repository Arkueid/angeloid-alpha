#include "IGpuDevice.h"

namespace Gpu {

static std::unique_ptr<IGpuDevice> sDevice;

IGpuDevice* device() {
    return sDevice.get();
}

void setDevice(std::unique_ptr<IGpuDevice> dev) {
    sDevice = std::move(dev);
}
}
