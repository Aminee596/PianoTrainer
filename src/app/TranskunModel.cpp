#include "app/TranskunModel.h"
#include "app/Embedded.h"

#include <array>
#include <cstring>
#include <fstream>

#if PIANO_HAS_ONNXRUNTIME
 #include <onnxruntime_cxx_api.h>
#endif

namespace piano
{
namespace
{
    std::vector<float> readFloats (const std::filesystem::path& file)
    {
        std::ifstream in (file, std::ios::binary | std::ios::ate);
        if (! in) return {};
        const auto bytes = (std::size_t) in.tellg();
        std::vector<float> v (bytes / sizeof (float));
        in.seekg (0);
        in.read (reinterpret_cast<char*> (v.data()), (std::streamsize) (v.size() * sizeof (float)));
        return in ? v : std::vector<float> {};
    }

    std::vector<float> embeddedFloats (const char* name)
    {
        const void* data = nullptr;
        std::size_t size = 0;
        if (! app::embedded::find (name, data, size))
            return {};
        std::vector<float> v (size / sizeof (float));
        std::memcpy (v.data(), data, v.size() * sizeof (float));
        return v;
    }
}

#if PIANO_HAS_ONNXRUNTIME

struct TranskunModel::Session
{
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "PianoTrainer" };
    std::unique_ptr<Ort::Session> model;
};

TranskunModel::TranskunModel() = default;
TranskunModel::~TranskunModel() = default;

bool TranskunModel::load (const std::filesystem::path& folder, std::string& error)
{
    session = nullptr;
    if (! app::embedded::loadOnnxRuntime (error))
        return false;

    const void* modelData = nullptr;
    std::size_t modelSize = 0;
    const bool embedded = app::embedded::find ("TRANSKUN_ONNX", modelData, modelSize);
    const bool tables = embedded ? features.load (embeddedFloats ("TRANSKUN_MELS"), embeddedFloats ("TRANSKUN_WINDOWS"))
                                 : features.load (readFloats (folder / "freq2mels.f32"), readFloats (folder / "windows.f32"));
    if (! tables)
    {
        error = embedded ? std::string ("damaged model tables in the app") : "missing or damaged freq2mels.f32 / windows.f32 in " + folder.string();
        return false;
    }
    try
    {
        auto s = std::make_unique<Session>();
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads (2);
        options.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (embedded)
        {
            s->model = std::make_unique<Ort::Session> (s->env, modelData, modelSize, options);
        }
        else
        {
            const auto file = folder / "transkun.onnx";
            s->model = std::make_unique<Ort::Session> (s->env, file.c_str(), options);
        }
        session = std::move (s);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool TranskunModel::loaded() const noexcept
{
    return session != nullptr && features.ready();
}

std::vector<HeardNote> TranskunModel::transcribe (const float* audio, int n, double startTime)
{
    std::vector<HeardNote> notes;
    if (! loaded() || n <= 0)
        return notes;
    int frames = 0;
    auto input = features.compute (audio, n, frames);
    try
    {
        const auto memory = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<std::int64_t, 4> shape { 1, frames, transkun::kMels, transkun::kWindows };
        auto tensor = Ort::Value::CreateTensor<float> (memory, input.data(), input.size(), shape.data(), shape.size());
        const char* inputs[] = { "featuresBatch" };
        const char* outputs[] = { "S" };
        auto result = session->model->Run (Ort::RunOptions { nullptr }, inputs, &tensor, 1, outputs, 1);
        const auto dims = result[0].GetTensorTypeAndShapeInfo().GetShape();
        if (dims.size() != 3 || dims[2] != transkun::kTracks)
            return notes;
        const int T = (int) dims[0];
        const auto intervals = transkun::decode (result[0].GetTensorData<float>(), T);
        const double frame = (double) transkun::kHop / transkun::kRate;
        for (int track = 2; track < transkun::kTracks; ++track)
            for (const auto& iv : intervals[(std::size_t) track])
                if (iv.begin > 0)
                    notes.push_back ({ transkun::trackPitch (track), startTime + iv.begin * frame });
    }
    catch (const std::exception&)
    {
        notes.clear();
    }
    return notes;
}

#else

struct TranskunModel::Session {};
TranskunModel::TranskunModel() = default;
TranskunModel::~TranskunModel() = default;

bool TranskunModel::load (const std::filesystem::path&, std::string& error)
{
    error = "built without ONNX Runtime";
    return false;
}

bool TranskunModel::loaded() const noexcept { return false; }
std::vector<HeardNote> TranskunModel::transcribe (const float*, int, double) { return {}; }

#endif

}
