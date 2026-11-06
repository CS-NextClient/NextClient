#pragma once

#include <string>

#include <ncl_material/ProgramManifest.h>

namespace matgen
{
    // The manifests the client itself ships, embedded here from the same files the engine
    // module embeds. A tool checking against a copy that had drifted would be worse than no
    // tool, because it would say yes to what the client says no to.
    //
    // Returns nullptr when the program is not one of them, which is the same answer the
    // client gives; the message is the caller's to write.
    const ncl_material::ProgramManifest* FindManifest(const std::string& program, std::string& error);
} // namespace matgen
