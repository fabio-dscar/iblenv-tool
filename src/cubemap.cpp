#include <cubemap.h>

#include <functional>
#include <map>
#include <format>
#include <filesystem>
#include <regex>
#include <iostream>

#include <texture.h>
#include <util.h>
#include <fstream>

using namespace ibl;
using namespace ibl::util;
using namespace std::filesystem;

static const std::map<int, std::string> FaceNames{
    {0, "+X"},
    {1, "-X"},
    {2, "+Y"},
    {3, "-Y"},
    {4, "+Z"},
    {5, "-Z"}
};

int GetFaceSide(CubeLayoutType type, int width, int height) {
    using enum CubeLayoutType;

    auto smallSide = std::min(width, height);
    switch (type) {
    case Separate:
    case Sequence:
    case VerticalSequence:
        return smallSide;
    case VerticalCross:
    case HorizontalCross:
    case InvHorizontalCross:
        return smallSide / 3;
    default:
        return 0;
    }
}

bool ValidateMapping(CubeLayoutType type, int width, int height) {
    using enum CubeLayoutType;
    switch (type) {
    case Sequence:
        return width / 6 == height;
    case VerticalSequence:
        return height / 6 == width;
    case HorizontalCross:
    case InvHorizontalCross:
        return width / 4 == height / 3;
    case VerticalCross:
        return height / 4 == width / 3;
    default:
        return false;
    }
}

struct FaceMapping {
    int width = 0, height = 0;
    std::map<int, std::pair<int, int>> mapping;
};

using MappingFunc = std::function<FaceMapping(int)>;
using enum CubeLayoutType;

template<CubeLayoutType T>
MappingFunc getMapping();

template<>
MappingFunc getMapping<Sequence>() {
    return [](int cubeSide) -> FaceMapping {
        return {
            .width = 6 * cubeSide,
            .height = cubeSide,
            .mapping{
                     {0, {0, 0}},
                     {1, {cubeSide, 0}},
                     {2, {2 * cubeSide, 0}},
                     {3, {3 * cubeSide, 0}},
                     {4, {4 * cubeSide, 0}},
                     {5, {5 * cubeSide, 0}},
                     }
        };
    };
}

template<>
MappingFunc getMapping<VerticalSequence>() {
    return [](int cubeSide) -> FaceMapping {
        return {
            .width = cubeSide,
            .height = 6 * cubeSide,
            .mapping{
                     {0, {0, 0}},
                     {1, {0, cubeSide}},
                     {2, {0, 2 * cubeSide}},
                     {3, {0, 3 * cubeSide}},
                     {4, {0, 4 * cubeSide}},
                     {5, {0, 5 * cubeSide}},
                     }
        };
    };
}

template<>
MappingFunc getMapping<HorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {
            .width = 4 * cubeSide,
            .height = 3 * cubeSide,
            .mapping{
                     {0, {2 * cubeSide, cubeSide}},
                     {1, {0, cubeSide}},
                     {2, {cubeSide, 0}},
                     {3, {cubeSide, 2 * cubeSide}},
                     {4, {cubeSide, cubeSide}},
                     {5, {3 * cubeSide, cubeSide}},
                     }
        };
    };
}

template<>
MappingFunc getMapping<InvHorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {
            .width = 4 * cubeSide,
            .height = 3 * cubeSide,
            .mapping{
                     {0, {3 * cubeSide, cubeSide}},
                     {1, {cubeSide, cubeSide}},
                     {2, {2 * cubeSide, 0}},
                     {3, {2 * cubeSide, 2 * cubeSide}},
                     {4, {2 * cubeSide, cubeSide}},
                     {5, {0, cubeSide}},
                     }
        };
    };
}

template<>
MappingFunc getMapping<VerticalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {
            .width = 3 * cubeSide,
            .height = 4 * cubeSide,
            .mapping{
                     {4, {cubeSide, cubeSide}},
                     {5, {cubeSide, 3 * cubeSide}},
                     {2, {cubeSide, 0}},
                     {3, {cubeSide, 2 * cubeSide}},
                     {1, {0, cubeSide}},
                     {0, {2 * cubeSide, cubeSide}},
                     }
        };
    };
}

static const std::map<CubeLayoutType, MappingFunc> CubeMappings{
    {Sequence,           getMapping<Sequence>()          },
    {VerticalSequence,   getMapping<VerticalSequence>()  },
    {HorizontalCross,    getMapping<HorizontalCross>()   },
    {InvHorizontalCross, getMapping<InvHorizontalCross>()},
    {VerticalCross,      getMapping<VerticalCross>()     }
};

void ExportSeparate(const path& filePath, const CubeImage& cube) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto NameOutput = [&](int face) -> auto {
        return std::format("{}_{}{}", fname, FaceNames.at(face), ext);
    };

    for (const auto& [face, name] : FaceNames) {
        SaveMipmappedImage(parent / NameOutput(face), cube[face]);
    }
}

// clang-format off
void ExportCombined(const path& filePath, MappingFunc mapFunc, const CubeImage& cube) {
    auto cubeFmt = cube.imgFormat();
    auto map = mapFunc(cubeFmt.width);
    Image crossImg{{.pFmt      = cubeFmt.pFmt,
                    .width     = map.width,
                    .height    = map.height,
                    .nChannels = cubeFmt.nChannels}, 
                   cube.numLevels()};

    for (int lvl = 0; lvl < cube.numLevels(); ++lvl) {
        cubeFmt = cube.imgFormat(lvl);
        map = mapFunc(cubeFmt.width);

        for (const auto& [face, coords] : map.mapping) {
            auto& [x, y] = coords;

            // Copy face image into portion of the cross/layout chosen
            crossImg.copy({.toX   = x, .toY   = y,
                           .fromX = 0, .fromY = 0,
                           .sizeX = cubeFmt.width,
                           .sizeY = cubeFmt.height},
                          cube[face], lvl, lvl);
        }
    }

    SaveMipmappedImage(filePath, crossImg);
}
// clang-format on

struct CubeHeader {
    char id[4] = {'C', 'U', 'B', 'E'};
    unsigned int fmt;
    unsigned int width;
    unsigned int height;
    unsigned int compSize;
    unsigned int numChannels;
    unsigned int totalSize;
    unsigned int levels;
};

void ExportCustom(const path& filePath, const CubeImage& cube) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto imgFmt = cube.imgFormat();
    auto faceSize = cube[0].size();

    CubeHeader header;
    header.fmt = 0;
    header.width = imgFmt.width;
    header.height = imgFmt.height;
    header.compSize = ComponentSize(imgFmt.pFmt);
    header.numChannels = imgFmt.nChannels;
    header.totalSize = faceSize * 6;
    header.levels = cube.numLevels();

    auto outName = std::format("{}{}", fname, ".cube");
    std::ofstream file(parent / outName, std::ios_base::out | std::ios_base::binary);
    file.write((const char*)&header, sizeof(CubeHeader));

    for (int face = 0; face < 6; ++face)
        file.write(reinterpret_cast<const char*>(cube[face].data()), faceSize);

    file.close();
}

typedef std::chrono::high_resolution_clock Clock;

void ibl::ExportCubemap(const std::string& filePath, CubeLayoutType type,
                        CubeImage& cube) {

    if (type == CubeLayoutType::Separate)
        ExportSeparate(filePath, cube);
    else if (type == CubeLayoutType::Custom)
        ExportCustom(filePath, cube);
    else {
        // Special case: invert -Z in both axis
        if (type == CubeLayoutType::VerticalCross)
            cube[5].flipXY();

        ExportCombined(filePath, CubeMappings.at(type), cube);
    }
}

auto ImportSeparate(const path& filePath, ImageFormat* fmt) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto NameInput = [&](int face) -> auto {
        return std::format("{}_{}{}", fname, FaceNames.at(face), ext);
    };

    std::unique_ptr<CubeImage> cube = nullptr;

    for (const auto& [face, name] : FaceNames) {
        auto inputName = NameInput(face);
        auto imgFace = LoadImage(filePath, fmt);

        if (!cube)
            cube = std::make_unique<CubeImage>(imgFace->format(), 1);

        (*cube)[face] = std::move(*imgFace);
    }

    return cube;
}

// clang-format off
auto ImportCombined(const path& filePath, CubeLayoutType type, MappingFunc mapFunc,
                    ImageFormat* fmt) {
                        
    auto srcImg = LoadImage(filePath, fmt);
    auto srcFmt = srcImg->format();

    if (!ValidateMapping(type, srcFmt.width, srcFmt.height))
        FATAL("Provided cubemap layout type does not match input file.");

    auto cubeSide = GetFaceSide(type, srcFmt.width, srcFmt.height);
    ImageFormat faceFmt{srcFmt.pFmt, cubeSide, cubeSide, srcFmt.nChannels};

    auto cube = std::make_unique<CubeImage>(faceFmt, 1);

    FaceMapping map = mapFunc(cubeSide);
    for (const auto& [face, coords] : map.mapping) {
        auto& [x, y] = coords;

        auto& cubeFace = (*cube)[face];

        // Copy a portion from a cross/cube layout into new single face image
        cubeFace.copy({.toX   = 0, .toY   = 0,
                       .fromX = x, .fromY = y,
                       .sizeX = faceFmt.width,
                       .sizeY = faceFmt.height}, 
                      *srcImg);
    }

    return cube;
}
// clang-format on

std::unique_ptr<CubeImage> ibl::ImportCubeMap(const std::string& filePath,
                                              CubeLayoutType type, ImageFormat* reqFmt) {

    if (type == CubeLayoutType::Separate)
        return ImportSeparate(filePath, reqFmt);

    auto cube = ImportCombined(filePath, type, CubeMappings.at(type), reqFmt);

    // Handle special case, invert -Z face for vertical cross
    if (type == CubeLayoutType::VerticalCross)
        (*cube)[5].flipXY();

    return cube;
}