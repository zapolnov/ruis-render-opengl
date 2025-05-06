// this file is to test basic usage of the library,
// mainly test that linking to the library works, so just call any
// non-inline function of the library to test it.

#include <ruis/render/opengl/context.hpp>

int main(int argc, const char** argv){
    ruis::render::opengl::context c;

    std::cout << "scissor enabled = " << c.is_scissor_enabled() << std::endl;

    return 0;
}
