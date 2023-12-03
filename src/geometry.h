#ifndef __IBL_GEOMETRY_H__
#define __IBL_GEOMETRY_H__

#include <glad/glad.h>
#include <vector>

namespace ibl {

class Quad {
public:
    ~Quad();

    void setup();
    void draw() const;

private:
    unsigned int vao = 0, vbo = 0;
};

class Cube {
public:
    ~Cube();

    void setup();
    void draw() const;
private:
    unsigned int vao = 0, vbo = 0;
};

} // namespace ibl

#endif // __IBL_GEOMETRY_H__