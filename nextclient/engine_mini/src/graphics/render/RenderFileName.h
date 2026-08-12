#pragma once

namespace render
{
    // Whether a command argument may be used as the name of a file written under the game
    // directory. A console command is something a server can send through stuffcmd, so a
    // name that could leave that directory is refused rather than resolved.
    //
    // Shared by every command that takes one: the check and the refusal are the same
    // question, and two copies of it are two answers waiting to drift apart.
    bool FileName_IsPlain(const char* command, const char* name);
}
