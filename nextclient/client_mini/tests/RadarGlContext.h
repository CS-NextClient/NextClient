#pragma once

#include <memory>
#include <string>

class RadarGlContext
{
private:
    struct Implementation;

    std::unique_ptr<Implementation> implementation_;

public:
    RadarGlContext();
    ~RadarGlContext();

    RadarGlContext(const RadarGlContext&) = delete;
    RadarGlContext& operator=(const RadarGlContext&) = delete;

    bool Initialize(std::string& error, bool double_buffer = false, int samples = 0, int width = 1, int height = 1);
    bool MakeCurrent();
    bool ClearCurrent();
    void* native_context() const;
    static void* Current();
};
