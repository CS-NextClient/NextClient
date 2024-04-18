/* Copyright (c) 2009, Fredrik Orderud
   License: BSD licence (http://www.opensource.org/licenses/bsd-license.php) */

#pragma once
#include <vector>
#include <cstring>

namespace stacktrace {

/** Call-stack entry datastructure. */
struct entry {
    /** Default constructor that clears all fields. */
    entry () : line(0) {
    }

    char file[513];     ///< filename
    size_t      line;     ///< line number
    char function[513]; ///< name of function or method

    /** Serialize entry into a text string. */
    void to_string (char* out) const
    {
        char s[1100] = "";
        snprintf(s, 1100, "%s (%zu): %s", file, line, function);
        strcat(out, s);
    }
};

/** Stack-trace base class, for retrieving the current call-stack. */
class call_stack {
public:
    /** Stack-trace consructor.
     \param num_discard - number of stack entries to discard at the top. */
    call_stack (const size_t num_discard = 0);

    virtual ~call_stack () throw();

    /** Serializes the entire call-stack into a text string. */
    void to_string (char* out) const
    {
        char* s = new char[1100 * stack.size()];
        s[0] = 0;
        for (size_t i = 0; i < stack.size(); i++)
        {
            stack[i].to_string(s);
            strcat(s, "\n");
        }
        strcat(out, s);
        delete[] s;
    }

    size_t to_string_size() const
    {
        char* s = new char[1100 * stack.size()];
        s[0] = 0;
        for (size_t i = 0; i < stack.size(); i++)
        {
            stack[i].to_string(s);
            strcat(s, "\n");
        }
        size_t result = strlen(s);
        delete[] s;
        return result;
    }

    /** Call stack. */
    std::vector<entry> stack;
};

} // namespace stacktrace
