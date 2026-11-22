#pragma once

#include "core/Score.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace piano
{
bool parseMidi (const std::uint8_t* data, std::size_t size, Score& out, std::string& error);

}
