#pragma once

#include "core/KeyLearner.h"
#include "core/Transkun.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace piano
{
class TranskunModel
{
public:
    TranskunModel();
    ~TranskunModel();

    bool load (const std::filesystem::path& folder, std::string& error);
    bool loaded() const noexcept;

    std::vector<HeardNote> transcribe (const float* audio, int n, double startTime);

private:
    struct Session;
    std::unique_ptr<Session> session;
    transkun::Features features;
};

}
