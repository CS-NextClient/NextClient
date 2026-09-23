#pragma once

namespace nitroapi
{
    class IEngine
    {
    public:
        enum
        {
            QUIT_NOTQUITTING = 0,
            QUIT_TODESKTOP,
            QUIT_RESTART
        };

    public:
        virtual ~IEngine() = default;

        virtual bool Load( bool dedicated, char* basedir, char* cmdline ) = 0;
        virtual void Unload() = 0;

        virtual void SetState( int iState ) = 0;
        virtual int GetState() = 0;

        virtual void SetSubState( int iSubState ) = 0;
        virtual int GetSubState() = 0;

        virtual int Frame() = 0;
        virtual double GetFrameTime() = 0;
        virtual double GetCurTime() = 0;

        virtual void TrapKey_Event( int key, bool down ) = 0;
        virtual void TrapMouse_Event( int buttons, bool down ) = 0;
        virtual void StartTrapMode() = 0;
        virtual bool IsTrapping() = 0;
        virtual bool CheckDoneTrapping( int& buttons, int& key ) = 0;

        virtual int GetQuitting() = 0;
        virtual void SetQuitting( int quittype ) = 0;
    };
}