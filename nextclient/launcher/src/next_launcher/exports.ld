{
    /* i386 glibc looks this up to tell a new-stdio binary from a pre-2.1 one */
    global: _IO_stdin_used;
    local: *;
};
