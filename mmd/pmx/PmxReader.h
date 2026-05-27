#pragma once

#include "pmx/PmxModel.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// --- BinaryReader: low-level PMX binary reading ---
class BinaryReader {
public:
    explicit BinaryReader(const std::filesystem::path& path);

    bool isEnd() const { return mPos >= mSize; }
    void seek(size_t pos) { mPos = pos; }
    size_t position() const { return mPos; }
    size_t size() const { return mSize; }

    int8_t  readS8();
    uint8_t readU8();
    int16_t readS16();
    uint16_t readU16();
    int32_t readS32();
    uint32_t readU32();
    float   readF32();

    int32_t readIndex(uint8_t size);  // signed, for most indices
    uint32_t readVertexIndex(uint8_t size); // unsigned for size<=2, signed for size=4

    Vec2 readVec2();
    Vec3 readVec3();
    Vec4 readVec4();
    Quat readQuat();

    std::string readText(uint8_t textEncoding);
    void readBytes(void* dst, size_t len);

private:
    template<typename T>
    T readVal() {
        T val;
        std::memcpy(&val, &mData[mPos], sizeof(T));
        mPos += sizeof(T);
        return val;
    }

    std::vector<uint8_t> mData;
    size_t mPos = 0;
    size_t mSize = 0;
};

struct PmxReader { static PmxModel load(const std::filesystem::path& path); };
