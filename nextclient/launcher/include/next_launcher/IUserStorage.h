#pragma once

namespace next_launcher
{
    class IUserStorage
    {
    public:
        virtual ~IUserStorage() = default;

        virtual int GetLocal(const char* key, int defaultValue) = 0;
        virtual void GetLocal(const char* key, char* output, int max_len, const char* defaultValue) = 0;
        virtual void SetLocal(const char* key, int value) = 0;
        virtual void SetLocal(const char* key, const char* value) = 0;
        virtual void DeleteLocalKey(const char* key) = 0;
        virtual void DeleteAllLocalKeys(const char* key) = 0;

        virtual int GetGlobal(const char* key, int defaultValue) = 0;
        virtual void GetGlobal(const char* key, char* output, int max_len, const char* defaultValue) = 0;
        virtual void SetGlobal(const char* key, int value) = 0;
        virtual void SetGlobal(const char* key, const char* value) = 0;
        virtual void DeleteGlobalKey(const char* key) = 0;
        virtual void DeleteAllGlobalKeys(const char* key) = 0;
    };
}