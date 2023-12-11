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

static const std::map<int, std::string> FaceNames{{0, "+X"}, {1, "-X"}, {2, "+Y"},
                                                  {3, "-Y"}, {4, "+Z"}, {5, "-Z"}};

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

using MipmapFileList = std::vector<std::pair<int, std::string>>;

std::optional<MipmapFileList> SearchDirForLevels(const path& filePath) {
    const auto& [parent, fileName, ext] = SplitFilePath(filePath);

    std::regex regex(std::format("{}_(\\d+){}", fileName, ext));
    std::smatch match;

    MipmapFileList fileList;
    for (auto& dirEntry : directory_iterator(parent)) {
        if (!dirEntry.is_regular_file())
            continue;

        std::string file = dirEntry.path().filename();
        if (std::regex_match(file, match, regex)) {
            auto numLvl = std::stoi(match[1].str());
            fileList.emplace_back(numLvl, file);
        }
    }

    if (fileList.size() > 0) {
        std::sort(fileList.begin(), fileList.end());
        return fileList;
    } else {
        return {};
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
        return {.width = 6 * cubeSide,
                .height = cubeSide,
                .mapping{
                    {0, {0, 0}},
                    {1, {cubeSide, 0}},
                    {2, {2 * cubeSide, 0}},
                    {3, {3 * cubeSide, 0}},
                    {4, {4 * cubeSide, 0}},
                    {5, {5 * cubeSide, 0}},
                }};
    };
}

template<>
MappingFunc getMapping<VerticalSequence>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = cubeSide,
                .height = 6 * cubeSide,
                .mapping{
                    {0, {0, 0}},
                    {1, {0, cubeSide}},
                    {2, {0, 2 * cubeSide}},
                    {3, {0, 3 * cubeSide}},
                    {4, {0, 4 * cubeSide}},
                    {5, {0, 5 * cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<HorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 4 * cubeSide,
                .height = 3 * cubeSide,
                .mapping{
                    {0, {2 * cubeSide, cubeSide}},
                    {1, {0, cubeSide}},
                    {2, {cubeSide, 0}},
                    {3, {cubeSide, 2 * cubeSide}},
                    {4, {cubeSide, cubeSide}},
                    {5, {3 * cubeSide, cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<InvHorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 4 * cubeSide,
                .height = 3 * cubeSide,
                .mapping{
                    {0, {3 * cubeSide, cubeSide}},
                    {1, {cubeSide, cubeSide}},
                    {2, {2 * cubeSide, 0}},
                    {3, {2 * cubeSide, 2 * cubeSide}},
                    {4, {2 * cubeSide, cubeSide}},
                    {5, {0, cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<VerticalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 3 * cubeSide,
                .height = 4 * cubeSide,
                .mapping{
                    {0, {cubeSide, cubeSide}},
                    {1, {cubeSide, 3 * cubeSide}},
                    {2, {cubeSide, 0}},
                    {3, {cubeSide, 2 * cubeSide}},
                    {4, {0, cubeSide}},
                    {5, {2 * cubeSide, cubeSide}},
                }};
    };
}

std::map<CubeLayoutType, MappingFunc> CubeMappings{
    {Sequence, getMapping<Sequence>()},
    {VerticalSequence, getMapping<VerticalSequence>()},
    {HorizontalCross, getMapping<HorizontalCross>()},
    {InvHorizontalCross, getMapping<InvHorizontalCross>()},
    {VerticalCross, getMapping<VerticalCross>()}};

void ExportSeparate(const path& filePath, const CubeImage& cube) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto NameOutput = [&](int face) -> auto {
        return std::format("{}_{}{}", fname, FaceNames.at(face), ext);
    };

    for (const auto& [face, name] : FaceNames) {
        SaveMipmappedImage(parent / NameOutput(face), cube.face(face));
    }
}

// clang-format off
void ExportCombined(const path& filePath, MappingFunc mapFunc, const CubeImage& cube) {
    ImageFormat cubeFmt = cube.imgFormat();
    FaceMapping map = mapFunc(cubeFmt.width);
    Image crossImg{{.width = map.width,
                    .height = map.height,
                    .numChannels = cubeFmt.numChannels,
                    .compSize = cubeFmt.compSize,
                    .levels = cube.levels()}};

    for (int lvl = 0; lvl < cube.levels(); ++lvl) {
        cubeFmt = cube.imgFormat(lvl);
        map = mapFunc(cubeFmt.width);

        for (const auto& [face, coords] : map.mapping) {
            auto& [x, y] = coords;

            // Copy face image into portion of the cross/layout chosen
            auto& faceImg = cube.face(face);
            crossImg.copy({.toX = x, .toY = y,
                           .fromX = 0, .fromY = 0,
                           .sizeX = cubeFmt.width,
                           .sizeY = cubeFmt.height},
                          faceImg, lvl, lvl);
        }
    }

    SaveMipmappedImage(filePath, crossImg);
}

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

void SaveCubeFormat(const path& filePath, const Image& img) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto imgFmt = img.format();

    CubeHeader header;
    header.fmt = 0;
    header.width = imgFmt.width;
    header.height = imgFmt.height;
    header.compSize = imgFmt.compSize;
    header.numChannels = imgFmt.numChannels;
    header.totalSize = img.size();
    header.levels = imgFmt.levels;

    auto outName = std::format("{}{}", fname, ".cube");
    std::ofstream file(parent / outName, std::ios_base::out | std::ios_base::binary);
    file.write((const char*)&header, sizeof(CubeHeader));
    file.write(reinterpret_cast<const char*>(img.data()), header.totalSize);
    file.close();
}

void ExportCustom(const path& filePath, const CubeImage& cube) {
    auto cubeFmt = cube.imgFormat();

    auto mapping = CubeMappings.at(VerticalSequence);
    auto map = mapping(cubeFmt.width);

    ImageFormat reqFmt{.width = map.width,
                       .height = map.height,
                       .numChannels = cubeFmt.numChannels,
                       .compSize = cubeFmt.compSize,
                       .levels = cubeFmt.levels};

    Image fullImg{reqFmt};

    for (int lvl = 0; lvl < cubeFmt.levels; ++lvl) {
        cubeFmt = cube.imgFormat(lvl);
        map = mapping(cubeFmt.width);

        for (const auto& [face, coords] : map.mapping) {
            auto& [x, y] = coords;

            // Copy face image into portion of the cross/layout chosen
            auto& faceImg = cube.face(face);
            fullImg.copy({.toX = x, .toY = y,
                          .fromX = 0, .fromY = 0,
                          .sizeX = cubeFmt.width,
                          .sizeY = cubeFmt.height},
                         faceImg, lvl);
        }
    }

    SaveCubeFormat(filePath, fullImg);
}
// clang-format on

void ibl::ExportCubemap(const std::string& filePath, CubeLayoutType type,
                        const CubeImage& cube) {
    using enum CubeLayoutType;

    switch (type) {
    case Separate:
        ExportSeparate(filePath, cube);
        break;
    case Sequence:
    case VerticalSequence:
    case HorizontalCross:
    case InvHorizontalCross:
    case VerticalCross:
        ExportCombined(filePath, CubeMappings.at(type), cube);
        break;
    case Custom:
        ExportCustom(filePath, cube);
        break;
    default:
        break;
    };
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
            cube = std::make_unique<CubeImage>(imgFace->format());

        cube->setFace(face, std::move(*imgFace));
    }

    return cube;
}

// clang-format off
auto ImportCombined(const path& filePath, CubeLayoutType type, MappingFunc mapFunc,
                    ImageFormat* fmt) {
    auto srcImg = LoadImage(filePath, fmt);

    if (!ValidateMapping(type, srcImg->width, srcImg->height))
        ExitWithError("Provided cubemap layout type does not match input file.");

    auto cubeSide = GetFaceSide(type, srcImg->width, srcImg->height);
    ImageFormat faceFmt{cubeSide, cubeSide, 0, srcImg->numChan, srcImg->compSize};

    auto cube = std::make_unique<CubeImage>(faceFmt);

    FaceMapping map = mapFunc(cubeSide);
    for (const auto& [face, coords] : map.mapping) {
        auto& [x, y] = coords;

        auto& cubeFace = cube->face(face);

        // Copy a portion from a cross/cube layout into new single face image
        cubeFace.copy({.toX = 0, .toY = 0,
                       .fromX = x, .fromY = y,
                       .sizeX = faceFmt.width,
                       .sizeY = faceFmt.height}, 
                      *srcImg);
    }

    return cube;
}

auto ImportCombinedLevels(const path& filePath, CubeLayoutType type, MappingFunc mapFunc,
                          ImageFormat* fmt) {

    auto results = SearchDirForLevels(filePath);
    if (!results)
        return std::unique_ptr<CubeImage>(nullptr);

    auto fileList = results.value();
    int numLevels = fileList.size();

    std::unique_ptr<CubeImage> cube = nullptr;

    for (const auto& [lvl, name] : fileList) {
        auto srcImg = LoadImage(name, fmt);

        if (!ValidateMapping(type, srcImg->width, srcImg->height))
            ExitWithError("Provided cubemap layout type does not match input file.");

        auto cubeSide = GetFaceSide(type, srcImg->width, srcImg->height);
        ImageFormat faceFmt{cubeSide, cubeSide, 0, srcImg->numChan, srcImg->compSize};

        if (!cube) {
            cube = std::make_unique<CubeImage>(faceFmt, numLevels);
        }

        FaceMapping map = mapFunc(cubeSide);
        for (const auto& [face, coords] : map.mapping) {
            auto& [x, y] = coords;

            // Copy a portion from a cross/cube layout into a single face image
            auto& cubeFace = cube->face(face);
            cubeFace.copy({.toX = 0, .toY = 0,
                           .fromX = x, .fromY = y,
                           .sizeX = faceFmt.width,
                           .sizeY = faceFmt.height}, 
                          *srcImg, lvl);
        }
    }

    return cube;
}
// clang-format on

std::unique_ptr<CubeImage> ibl::ImportCubeMap(const std::string& filePath,
                                              CubeLayoutType type, ImageFormat* reqFmt) {
    using enum CubeLayoutType;
    switch (type) {
    case Separate:
        return ImportSeparate(filePath, reqFmt);
    case Sequence:
    case VerticalSequence:
    case HorizontalCross:
    case InvHorizontalCross:
    case VerticalCross:
        return ImportCombined(filePath, type, CubeMappings.at(type), reqFmt);
    default:
        return nullptr;
    };
}