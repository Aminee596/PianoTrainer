#include "app/Embedded.h"

#include <juce_core/juce_core.h>

#include <mutex>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#if PIANO_HAS_ONNXRUNTIME
 #include <onnxruntime_cxx_api.h>
#endif

namespace app::embedded
{

bool find (const char* name, const void*& data, std::size_t& size)
{
    data = nullptr;
    size = 0;
#if JUCE_WINDOWS
    const auto module = GetModuleHandleW (nullptr);
    const auto resource = FindResourceA (module, name, MAKEINTRESOURCEA (10));
    if (resource == nullptr)
        return false;
    const auto loaded = LoadResource (module, resource);
    if (loaded == nullptr)
        return false;
    data = LockResource (loaded);
    size = (std::size_t) SizeofResource (module, resource);
    return data != nullptr && size > 0;
#else
    juce::ignoreUnused (name);
    return false;
#endif
}

bool loadOnnxRuntime (std::string& error)
{
#if PIANO_HAS_ONNXRUNTIME && PIANO_EMBEDDED_ONNXRUNTIME && JUCE_WINDOWS
    static std::once_flag once;
    static bool ok = false;
    static std::string why;
    std::call_once (once, []
    {
        const void* data = nullptr;
        std::size_t size = 0;
        if (! find ("ONNXRUNTIME_DLL", data, size))
        {
            why = "ONNX Runtime is missing from the app";
            return;
        }
        const auto folder = juce::File::getSpecialLocation (juce::File::windowsLocalAppData)
                                .getChildFile ("PianoTrainer")
                                .getChildFile ("Runtime")
                                .getChildFile ("onnxruntime-" PIANO_ORT_VERSION_STRING);
        const auto dll = folder.getChildFile ("onnxruntime.dll");
        if (dll.getSize() != (juce::int64) size)
        {
            folder.createDirectory();
            const auto temp = folder.getNonexistentChildFile ("onnxruntime", ".tmp", false);
            if (temp.replaceWithData (data, size) && ! temp.moveFileTo (dll))
                temp.deleteFile();
        }
        if (LoadLibraryExW (dll.getFullPathName().toWideCharPointer(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) == nullptr)
        {
            why = "couldn't load " + dll.getFullPathName().toStdString();
            return;
        }
        Ort::InitApi();
        if (Ort::Global<void>::api_ == nullptr)
        {
            why = "ONNX Runtime " PIANO_ORT_VERSION_STRING " didn't start";
            return;
        }
        ok = true;
    });
    if (! ok)
        error = why;
    return ok;
#elif PIANO_HAS_ONNXRUNTIME && defined (ORT_API_MANUAL_INIT)
    static std::once_flag once;
    std::call_once (once, [] { Ort::InitApi(); });
    juce::ignoreUnused (error);
    return true;
#else
    juce::ignoreUnused (error);
    return true;
#endif
}

}
