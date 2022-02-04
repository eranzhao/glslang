#pragma once

#if defined(_MSC_VER) && _MSC_VER >= 1900
    #pragma warning(disable : 4464) // relative include path contains '..'
#endif

#include "../Include/intermediate.h"

#include <string>
#include <vector>

namespace glslang {

// Translate AST to Vulkan glsl.
void AstToGlsl(const glslang::TIntermediate& intermediate, const char* baseName);

}

