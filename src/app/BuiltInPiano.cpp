#include "app/SamplePiano.h"

#if defined (_WIN32) && PIANO_BUILT_IN_PIANO
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace app
{

bool builtInPianoData (const void*& data, size_t& size)
{
#if defined (_WIN32) && PIANO_BUILT_IN_PIANO
    const auto module = GetModuleHandleW (nullptr);
    const auto resource = FindResourceW (module, L"PIANO_SAMPLES", MAKEINTRESOURCEW (10));
    if (resource == nullptr)
        return false;
    const auto loaded = LoadResource (module, resource);
    if (loaded == nullptr)
        return false;
    data = LockResource (loaded);
    size = (size_t) SizeofResource (module, resource);
    return data != nullptr && size > 0;
#else
    data = nullptr;
    size = 0;
    return false;
#endif
}

}
