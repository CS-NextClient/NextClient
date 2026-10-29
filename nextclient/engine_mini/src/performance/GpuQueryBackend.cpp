#include "performance/GpuQueryBackend.h"

#include <glad/glad.h>

namespace performance
{
    void GpuQueryBackend::Probe()
    {
        Clear();
        if (!(GLAD_GL_VERSION_3_3 || GLAD_GL_ARB_timer_query) || !glGenQueries || !glDeleteQueries || !glQueryCounter || !glGetQueryiv ||
            !glGetQueryObjectiv || !glGetQueryObjectui64v)
        {
            return;
        }

        glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &counter_bits_);
        supported_ = counter_bits_ > 0;
    }

    void GpuQueryBackend::Clear()
    {
        supported_ = false;
        counter_bits_ = 0;
    }

    bool GpuQueryBackend::IsTimestampSupported() const
    {
        return supported_;
    }

    int GpuQueryBackend::counter_bits() const
    {
        return counter_bits_;
    }

    void GpuQueryBackend::Allocate(uint32_t count, uint32_t* names)
    {
        glGenQueries(int(count), names);
    }

    void GpuQueryBackend::Delete(uint32_t count, const uint32_t* names)
    {
        glDeleteQueries(int(count), names);
    }

    void GpuQueryBackend::Write(uint32_t name)
    {
        glQueryCounter(name, GL_TIMESTAMP);
    }

    bool GpuQueryBackend::IsReady(uint32_t name)
    {
        int ready = 0;
        glGetQueryObjectiv(name, GL_QUERY_RESULT_AVAILABLE, &ready);
        return ready != 0;
    }

    uint64_t GpuQueryBackend::Read(uint32_t name)
    {
        uint64_t timestamp = 0;
        glGetQueryObjectui64v(name, GL_QUERY_RESULT, &timestamp);
        return timestamp;
    }
} // namespace performance
