#pragma once

#include <cstdint>

namespace performance
{
    class GpuQueryBackendInterface
    {
    public:
        virtual ~GpuQueryBackendInterface() = default;
        virtual bool IsTimestampSupported() const = 0;
        virtual int counter_bits() const = 0;
        virtual void Allocate(uint32_t count, uint32_t* names) = 0;
        virtual void Delete(uint32_t count, const uint32_t* names) = 0;
        virtual void Write(uint32_t name) = 0;
        virtual bool IsReady(uint32_t name) = 0;
        virtual uint64_t Read(uint32_t name) = 0;
    };

    class GpuQueryBackend : public GpuQueryBackendInterface
    {
    private:
        bool supported_{};
        int counter_bits_{};

    public:
        void Probe();
        void Clear();
        bool IsTimestampSupported() const override;
        int counter_bits() const override;
        void Allocate(uint32_t count, uint32_t* names) override;
        void Delete(uint32_t count, const uint32_t* names) override;
        void Write(uint32_t name) override;
        bool IsReady(uint32_t name) override;
        uint64_t Read(uint32_t name) override;
    };
} // namespace performance
