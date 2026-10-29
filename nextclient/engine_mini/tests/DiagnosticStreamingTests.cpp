#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <common/TracyProtocol.hpp>
#include <ncl_perf/profiler.h>
#include <nitro_utils/platform.h>

namespace
{
#ifdef _WIN32
    using socket_t = SOCKET;
    constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
    using socket_t = int;
    constexpr socket_t kInvalidSocket = -1;
#endif

    void StreamingTest_Close(socket_t socket)
    {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }

    bool StreamingTest_Read(socket_t socket, void* output, size_t size)
    {
        char* position = static_cast<char*>(output);
        while (size)
        {
            int received = recv(socket, position, int(size), 0);
            if (received <= 0)
            {
                return false;
            }
            position += received;
            size -= received;
        }
        return true;
    }

    void StreamingTest_FillText(std::array<char, 60000>& text, uint32_t& state)
    {
        for (char& character : text)
        {
            state = state * 1664525u + 1013904223u;
            character = char(state >> 24);
        }
    }

    template <typename Predicate>
    bool StreamingTest_Wait(Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate())
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    TracyCZoneCtx StreamingTest_Sentinel()
    {
        static const ___tracy_source_location_data location{"Zone sentinel", __func__, __FILE__, __LINE__, 0};
        TracyCZoneCtx context = ___tracy_emit_zone_begin(&location, 1);
        ___tracy_emit_zone_end(context);
        return context;
    }

    class StreamingZones
    {
    private:
        std::vector<char> dictionary_;
        std::vector<char> pending_;
        std::vector<int64_t> events_;
        mutable std::mutex mutex_;
        uint32_t thread_{};
        std::map<uint32_t, uint32_t> validations_;

    public:
        bool Consume(const char* data, size_t size)
        {
            size_t first = dictionary_.size();
            size_t input = 0;
            auto length = [&](size_t value) {
                if (value == 15)
                {
                    uint8_t extra = 255;
                    while (extra == 255 && input < size)
                    {
                        extra = uint8_t(data[input++]);
                        value += extra;
                    }
                }
                return value;
            };
            while (input < size)
            {
                uint8_t token = uint8_t(data[input++]);
                size_t literals = length(token >> 4);
                if (literals > size - input || dictionary_.size() - first + literals > tracy::TargetFrameSize)
                {
                    return false;
                }
                dictionary_.insert(dictionary_.end(), data + input, data + input + literals);
                input += literals;
                if (input == size)
                {
                    break;
                }
                if (size - input < 2)
                {
                    return false;
                }
                size_t offset = uint8_t(data[input]) | (size_t(uint8_t(data[input + 1])) << 8);
                input += 2;
                size_t matches = length(token & 15) + 4;
                if (!offset || offset > dictionary_.size() || dictionary_.size() - first + matches > tracy::TargetFrameSize)
                {
                    return false;
                }
                for (size_t index = 0; index < matches; ++index)
                {
                    dictionary_.push_back(dictionary_[dictionary_.size() - offset]);
                }
            }
            pending_.insert(pending_.end(), dictionary_.begin() + first, dictionary_.end());
            if (dictionary_.size() > 65536)
            {
                dictionary_.erase(dictionary_.begin(), dictionary_.end() - 65536);
            }

            size_t position = 0;
            while (position < pending_.size())
            {
                uint8_t type = uint8_t(pending_[position]);
                if (type >= uint8_t(tracy::QueueType::NUM_TYPES))
                {
                    return false;
                }
                tracy::QueueType kind = tracy::QueueType(type);
                size_t available = pending_.size() - position;
                size_t event_size = tracy::QueueDataSize[type];
                size_t size_offset = 0;
                size_t size_bytes = 0;
                size_t size_bias = 0;
                if (type >= uint8_t(tracy::QueueType::StringData))
                {
                    size_offset = 1 + sizeof(tracy::QueueStringTransfer);
                    size_bytes = kind == tracy::QueueType::FrameImageData || kind == tracy::QueueType::SymbolCode ||
                                         kind == tracy::QueueType::SourceCode
                                     ? 4
                                     : 2;
                }
                else if (kind == tracy::QueueType::SingleStringData || kind == tracy::QueueType::SecondStringData)
                {
                    size_offset = 1;
                    size_bytes = 2;
                    size_bias = tracy::ProtocolOffset8Bit;
                }
                else if (kind == tracy::QueueType::SingleStringData8 || kind == tracy::QueueType::SecondStringData8)
                {
                    size_offset = 1;
                    size_bytes = 1;
                }
                if (size_bytes)
                {
                    if (available < size_offset + size_bytes)
                    {
                        break;
                    }
                    uint32_t payload = 0;
                    std::memcpy(&payload, pending_.data() + position + size_offset, size_bytes);
                    event_size = size_offset + size_bytes + payload + size_bias;
                }
                if (available < event_size)
                {
                    break;
                }
                if (kind == tracy::QueueType::ThreadContext)
                {
                    std::memcpy(&thread_, pending_.data() + position + 1, sizeof(thread_));
                }
                else if (kind == tracy::QueueType::ZoneValidation)
                {
                    uint32_t& validation = validations_[thread_];
                    std::memcpy(&validation, pending_.data() + position + 1, sizeof(validation));
                }
                bool begin = kind == tracy::QueueType::ZoneBegin || kind == tracy::QueueType::ZoneBegin32 ||
                             kind == tracy::QueueType::ZoneBegin16 || kind == tracy::QueueType::ZoneBeginCallstack ||
                             kind == tracy::QueueType::ZoneBeginCallstack32 || kind == tracy::QueueType::ZoneBeginCallstack16 ||
                             kind == tracy::QueueType::ZoneBeginAllocSrcLoc || kind == tracy::QueueType::ZoneBeginAllocSrcLocCallstack;
                bool end = kind == tracy::QueueType::ZoneEnd || kind == tracy::QueueType::ZoneEnd32 || kind == tracy::QueueType::ZoneEnd16;
                if (begin || end)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    uint32_t& validation = validations_[thread_];
                    events_.push_back(begin ? int64_t(validation) : -int64_t(validation));
                    validation = 0;
                }
                position += event_size;
            }
            pending_.erase(pending_.begin(), pending_.begin() + position);
            return true;
        }

        std::vector<int64_t> Copy() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return events_;
        }

        bool HasEnded(TracyCZoneCtx context) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return std::find(events_.begin(), events_.end(), -int64_t(context.id)) != events_.end();
        }
    };

    class StreamingProbe
    {
    private:
        nitro_utils::SysModule module_{};

    public:
        using BeginFn_t = TracyCZoneCtx (*)();
        using EndFn_t = void (*)(TracyCZoneCtx);
        BeginFn_t begin{};
        EndFn_t end{};

        explicit StreamingProbe(const char* path) :
            module_(nitro_utils::LoadSysModule(path))
        {
            if (module_)
            {
                begin = reinterpret_cast<BeginFn_t>(nitro_utils::GetProcAddress(module_, "PerfTest_BeginCpu"));
                end = reinterpret_cast<EndFn_t>(nitro_utils::GetProcAddress(module_, "PerfTest_EndCpu"));
            }
        }

        ~StreamingProbe()
        {
            if (module_)
            {
                nitro_utils::UnloadSysModule(module_);
            }
        }
    };

    class DiagnosticStreamingTest : public testing::Test
    {
    private:
        std::string original_port_;
        bool had_port_{};

    protected:
        socket_t receiver_ = kInvalidSocket;
        std::thread reader_;
        std::atomic<uint32_t> blocks_{};
        std::atomic<bool> invalid_block_{};
        sockaddr_in address_{};
        std::unique_ptr<StreamingZones> zones_;

        void SetUp() override
        {
#ifdef _WIN32
            WSADATA data{};
            ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &data), 0);
#endif
            socket_t reservation = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            ASSERT_NE(reservation, kInvalidSocket);
            sockaddr_in& address = address_;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            ASSERT_EQ(bind(reservation, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
#ifdef _WIN32
            int address_size = sizeof(address);
#else
            socklen_t address_size = sizeof(address);
#endif
            ASSERT_EQ(getsockname(reservation, reinterpret_cast<sockaddr*>(&address), &address_size), 0);
            StreamingTest_Close(reservation);
            const char* original = std::getenv("TRACY_PORT");
            had_port_ = original != nullptr;
            original_port_ = original ? original : "";
            const std::string port = std::to_string(ntohs(address.sin_port));
#ifdef _WIN32
            _putenv_s("TRACY_PORT", port.c_str());
#else
            setenv("TRACY_PORT", port.c_str(), 1);
#endif
            ncl_perf::Profiling_Start();
            Connect();
        }

        void Connect()
        {
            ASSERT_TRUE(StreamingTest_Wait([&]() {
                socket_t connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (connection == kInvalidSocket)
                {
                    return false;
                }
                int receive_buffer = 1024;
                setsockopt(connection, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&receive_buffer), sizeof(receive_buffer));
                if (connect(connection, reinterpret_cast<const sockaddr*>(&address_), sizeof(address_)) != 0)
                {
                    StreamingTest_Close(connection);
                    return false;
                }
                receiver_ = connection;
                return true;
            }));
#ifdef _WIN32
            DWORD timeout = 5000;
#else
            timeval timeout{5, 0};
#endif
            setsockopt(receiver_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            ASSERT_EQ(
                send(receiver_, tracy::HandshakeShibboleth, int(tracy::HandshakeShibbolethSize), 0), int(tracy::HandshakeShibbolethSize)
            );
            const uint32_t protocol = tracy::ProtocolVersion;
            ASSERT_EQ(send(receiver_, reinterpret_cast<const char*>(&protocol), sizeof(protocol), 0), int(sizeof(protocol)));
            tracy::HandshakeStatus status{};
            ASSERT_TRUE(StreamingTest_Read(receiver_, &status, sizeof(status)));
            ASSERT_EQ(status, tracy::HandshakeWelcome);
            tracy::WelcomeMessage welcome{};
            tracy::OnDemandPayloadMessage payload{};
            ASSERT_TRUE(StreamingTest_Read(receiver_, &welcome, sizeof(welcome)));
            ASSERT_TRUE(StreamingTest_Read(receiver_, &payload, sizeof(payload)));
            ASSERT_TRUE(StreamingTest_Wait(ncl_perf::Profiling_IsConnected));
        }

        void TearDown() override
        {
            Disconnect();
            ncl_perf::Profiling_Stop();
#ifdef _WIN32
            _putenv_s("TRACY_PORT", original_port_.c_str());
            WSACleanup();
#else
            if (had_port_)
            {
                setenv("TRACY_PORT", original_port_.c_str(), 1);
            }
            else
            {
                unsetenv("TRACY_PORT");
            }
#endif
        }

        void Disconnect()
        {
            if (receiver_ != kInvalidSocket)
            {
#ifdef _WIN32
                shutdown(receiver_, SD_BOTH);
#else
                shutdown(receiver_, SHUT_RDWR);
#endif
            }
            if (reader_.joinable())
            {
                reader_.join();
            }
            if (receiver_ != kInvalidSocket)
            {
                StreamingTest_Close(receiver_);
                receiver_ = kInvalidSocket;
            }
        }

        void StartReader()
        {
            reader_ = std::thread([&]() {
                std::array<char, tracy::LZ4Size> compressed{};
                tracy::lz4sz_t size{};
                while (StreamingTest_Read(receiver_, &size, sizeof(size)))
                {
                    if (size == 0 || size > compressed.size())
                    {
                        invalid_block_.store(true);
                        break;
                    }
                    if (!StreamingTest_Read(receiver_, compressed.data(), size))
                    {
                        break;
                    }
                    ++blocks_;
                }
            });
        }

        void StartZoneReader()
        {
            zones_ = std::make_unique<StreamingZones>();
            reader_ = std::thread([&]() {
                std::array<char, tracy::LZ4Size> compressed{};
                tracy::lz4sz_t size{};
                while (StreamingTest_Read(receiver_, &size, sizeof(size)))
                {
                    if (!size || size > compressed.size() || !StreamingTest_Read(receiver_, compressed.data(), size) ||
                        !zones_->Consume(compressed.data(), size))
                    {
                        invalid_block_.store(true);
                        break;
                    }
                }
            });
        }
    };
} // namespace

TEST_F(DiagnosticStreamingTest, SharedCpuZonesCloseAbandonedChildrenBeforeTheirParent)
{
    StreamingProbe first(PERF_PROBE_A_PATH);
    StreamingProbe second(PERF_PROBE_B_PATH);
    ASSERT_NE(first.begin, nullptr);
    ASSERT_NE(first.end, nullptr);
    ASSERT_NE(second.begin, nullptr);
    ASSERT_NE(second.end, nullptr);
    StartZoneReader();
    StreamingZones& zones = *zones_;
    TracyCZoneCtx parent = first.begin();
    TracyCZoneCtx child = second.begin();
    TracyCZoneCtx nested = first.begin();
    first.end(parent);
    second.end(child);
    first.end(nested);
    first.end(parent);
    TracyCZoneCtx fresh = second.begin();
    first.end(nested);
    second.end(fresh);
    TracyCZoneCtx sentinel = StreamingTest_Sentinel();
    FrameMark;

    ASSERT_TRUE(StreamingTest_Wait([&]() { return zones.HasEnded(sentinel); }));
    EXPECT_EQ(
        zones.Copy(),
        (std::vector<int64_t>{
            parent.id,
            child.id,
            nested.id,
            -int64_t(nested.id),
            -int64_t(child.id),
            -int64_t(parent.id),
            fresh.id,
            -int64_t(fresh.id),
            sentinel.id,
            -int64_t(sentinel.id)
        })
    );
    EXPECT_FALSE(invalid_block_.load());
    Disconnect();
}

TEST_F(DiagnosticStreamingTest, OldCpuTokensCannotCloseZonesAfterReconnectOrRuntimeRestart)
{
    static const ___tracy_source_location_data location{"Lifetime CPU", __func__, __FILE__, __LINE__, 0};
    TracyCZoneCtx old = ___tracy_emit_zone_begin(&location, 1);
    for (bool restart : {false, true})
    {
        Disconnect();
        ASSERT_TRUE(StreamingTest_Wait([]() { return !ncl_perf::Profiling_IsConnected(); }));
        if (restart)
        {
            ncl_perf::Profiling_Stop();
            ncl_perf::Profiling_Start();
        }
        Connect();
        StartZoneReader();
        StreamingZones& zones = *zones_;
        TracyCZoneCtx fresh = ___tracy_emit_zone_begin(&location, 1);
        ___tracy_emit_zone_end(old);
        ___tracy_emit_zone_end(fresh);
        ___tracy_emit_zone_end(fresh);
        TracyCZoneCtx sentinel = StreamingTest_Sentinel();
        FrameMark;
        ASSERT_TRUE(StreamingTest_Wait([&]() { return zones.HasEnded(sentinel); }));
        EXPECT_EQ(zones.Copy(), (std::vector<int64_t>{fresh.id, -int64_t(fresh.id), sentinel.id, -int64_t(sentinel.id)}));
        EXPECT_FALSE(invalid_block_.load());
        Disconnect();
        old = fresh;
    }
}

TEST_F(DiagnosticStreamingTest, EveryCpuBeginVariantParticipatesInAbortRecovery)
{
    static const ___tracy_source_location_data location{"Variant CPU", __func__, __FILE__, __LINE__, 0};
    StartZoneReader();
    std::array<TracyCZoneCtx, 4> contexts;
    contexts[0] = ___tracy_emit_zone_begin(&location, 1);
    contexts[1] = ___tracy_emit_zone_begin_callstack(&location, 0, 1);
    uint64_t source = ___tracy_alloc_srcloc(__LINE__, __FILE__, std::strlen(__FILE__), __func__, std::strlen(__func__), 0);
    contexts[2] = ___tracy_emit_zone_begin_alloc(source, 1);
    source = ___tracy_alloc_srcloc(__LINE__, __FILE__, std::strlen(__FILE__), __func__, std::strlen(__func__), 0);
    contexts[3] = ___tracy_emit_zone_begin_alloc_callstack(source, 0, 1);
    ___tracy_emit_zone_end(contexts[0]);
    for (TracyCZoneCtx context : contexts)
    {
        ___tracy_emit_zone_end(context);
    }
    TracyCZoneCtx sentinel = StreamingTest_Sentinel();
    FrameMark;

    ASSERT_TRUE(StreamingTest_Wait([&]() { return zones_->HasEnded(sentinel); }));
    EXPECT_EQ(
        zones_->Copy(),
        (std::vector<int64_t>{
            contexts[0].id,
            contexts[1].id,
            contexts[2].id,
            contexts[3].id,
            -int64_t(contexts[3].id),
            -int64_t(contexts[2].id),
            -int64_t(contexts[1].id),
            -int64_t(contexts[0].id),
            sentinel.id,
            -int64_t(sentinel.id)
        })
    );
    EXPECT_FALSE(invalid_block_.load());
}

TEST_F(DiagnosticStreamingTest, CpuTokensCannotCloseAnotherThreadsZones)
{
    static const ___tracy_source_location_data location{"Thread CPU", __func__, __FILE__, __LINE__, 0};
    StartZoneReader();
    TracyCZoneCtx parent = ___tracy_emit_zone_begin(&location, 1);
    TracyCZoneCtx worker_context{};
    std::atomic<bool> opened{};
    std::atomic<bool> finish{};
    std::thread worker([&]() {
        worker_context = ___tracy_emit_zone_begin(&location, 1);
        opened.store(true);
        StreamingTest_Wait([&]() { return finish.load(); });
        ___tracy_emit_zone_end(worker_context);
    });
    bool ready = StreamingTest_Wait([&]() { return opened.load(); });
    if (ready)
    {
        ___tracy_emit_zone_end(worker_context);
    }
    ___tracy_emit_zone_end(parent);
    finish.store(true);
    worker.join();
    ASSERT_TRUE(ready);
    TracyCZoneCtx sentinel = StreamingTest_Sentinel();
    FrameMark;

    ASSERT_TRUE(StreamingTest_Wait([&]() { return zones_->HasEnded(sentinel) && zones_->Copy().size() >= 6; }));
    std::vector<int64_t> parent_events;
    std::vector<int64_t> worker_events;
    for (int64_t event : zones_->Copy())
    {
        if (event == parent.id || event == -int64_t(parent.id))
        {
            parent_events.push_back(event);
        }
        else if (event == worker_context.id || event == -int64_t(worker_context.id))
        {
            worker_events.push_back(event);
        }
    }
    EXPECT_EQ(parent_events, (std::vector<int64_t>{parent.id, -int64_t(parent.id)}));
    EXPECT_EQ(worker_events, (std::vector<int64_t>{worker_context.id, -int64_t(worker_context.id)}));
    EXPECT_FALSE(invalid_block_.load());
}

TEST_F(DiagnosticStreamingTest, LiveReceiverKeepsCaptureActiveBeyondTheCumulativeBudget)
{
    StartReader();
    std::array<char, 60000> text{};
    uint32_t random = 1;
    StreamingTest_FillText(text, random);
    const size_t initial = tracy::QueueMemory_GetStatus().used;
    for (size_t batch = 0; batch < 600; ++batch)
    {
        for (size_t index = 0; index < 8; ++index)
        {
            TracyMessage(text.data(), text.size());
        }
        FrameMark;
        ASSERT_TRUE(StreamingTest_Wait([&]() { return tracy::QueueMemory_GetStatus().used < initial + 4u * 1024u * 1024u; }));
        ASSERT_EQ(tracy::QueueMemory_GetStatus().stopped, 0u);
    }
    EXPECT_TRUE(StreamingTest_Wait([&]() { return blocks_.load() > 16; }));
    EXPECT_TRUE(ncl_perf::Profiling_IsConnected());
    EXPECT_FALSE(invalid_block_.load());
    EXPECT_LT(tracy::QueueMemory_GetStatus().peak, 16u * 1024u * 1024u);
}

TEST_F(DiagnosticStreamingTest, StalledReceiverStopsCaptureWithinBudgetAndAllowsShutdown)
{
    std::array<char, 60000> text{};
    uint32_t random = 1;
    for (size_t index = 0; index < 4096 && !tracy::QueueMemory_IsStopped(); ++index)
    {
        StreamingTest_FillText(text, random);
        TracyMessage(text.data(), text.size());
    }
    const tracy::QueueMemoryStatus status = tracy::QueueMemory_GetStatus();
    EXPECT_EQ(status.stopped, 1u);
    EXPECT_LE(status.peak, status.limit);
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    EXPECT_FALSE(ncl_perf::Profiling_IsConnected());
    ncl_perf::Profiling_Stop();
    EXPECT_EQ(tracy::QueueMemory_GetStatus().used, 0u);
}

TEST_F(DiagnosticStreamingTest, StalledReceiverAllowsNormalShutdownBeforeTheBudgetIsExhausted)
{
    std::array<char, 60000> text{};
    uint32_t random = 1;
    for (size_t index = 0; index < 512; ++index)
    {
        StreamingTest_FillText(text, random);
        TracyMessage(text.data(), text.size());
    }
    tracy::lz4sz_t first_block{};
    ASSERT_TRUE(StreamingTest_Read(receiver_, &first_block, sizeof(first_block)));
    ASSERT_GT(first_block, 0u);
    for (size_t index = 0; index < 20000; ++index)
    {
        TracyPlot("shutdown backlog", int64_t(index));
    }
    for (size_t index = 0; index < 512; ++index)
    {
        StreamingTest_FillText(text, random);
        TracyMessage(text.data(), 8192);
    }
    EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 0u);

    ncl_perf::Profiling_Stop();
    EXPECT_EQ(tracy::QueueMemory_GetStatus().used, 0u);
}
