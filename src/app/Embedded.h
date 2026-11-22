#pragma once

#include <cstddef>
#include <string>

namespace app::embedded
{
bool find (const char* name, const void*& data, std::size_t& size);
bool loadOnnxRuntime (std::string& error);
}
