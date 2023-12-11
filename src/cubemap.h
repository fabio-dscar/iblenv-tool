#ifndef __IBL_CUBEMAP_H__
#define __IBL_CUBEMAP_H__

#include <string>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <image.h>

namespace ibl {

struct ImageFormat;
class Texture;

static const std::vector CubeMapViews{
    glm::lookAt(glm::vec3{0}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}),
    glm::lookAt(glm::vec3{0}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}),
    glm::lookAt(glm::vec3{0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
    glm::lookAt(glm::vec3{0}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}),
    glm::lookAt(glm::vec3{0}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}),
    glm::lookAt(glm::vec3{0}, {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f})};

enum class CubeLayoutType {
    HorizontalCross = 0,
    InvHorizontalCross = 1,
    Sequence = 2,
    Separate = 3,
    VerticalSequence = 4,
    VerticalCross = 5,
    Custom = 6
};

void ExportCubemap(const std::string& filePath, CubeLayoutType type,
                    const CubeImage& cube);
std::unique_ptr<CubeImage> ImportCubeMap(const std::string& filePath,
                                          CubeLayoutType type, ImageFormat* reqFmt);

} // namespace ibl

#endif // __IBL_CUBEMAP_H__