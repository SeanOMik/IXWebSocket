/*
 *  IXSelectInterruptPipe.cpp
 *  Author: Benjamin Sergeant
 *  Copyright (c) 2018-2019 Machine Zone, Inc. All rights reserved.
 */

//
// On macOS we use UNIX pipes to wake up select.
//

#include "IXSelectInterruptPipe.h"

#include <unistd.h> // for write
#include <string.h> // for strerror
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sstream>

namespace ix
{
    // File descriptor at index 0 in _fildes is the read end of the pipe
    // File descriptor at index 1 in _fildes is the write end of the pipe
    const int SelectInterruptPipe::kPipeReadIndex = 0;
    const int SelectInterruptPipe::kPipeWriteIndex = 1;

    SelectInterruptPipe::SelectInterruptPipe()
    {
        _fildes[kPipeReadIndex] = -1;
        _fildes[kPipeWriteIndex] = -1;
    }

    SelectInterruptPipe::~SelectInterruptPipe()
    {
        ::close(_fildes[kPipeReadIndex]);
        ::close(_fildes[kPipeWriteIndex]);
        _fildes[kPipeReadIndex] = -1;
        _fildes[kPipeWriteIndex] = -1;
    }

    bool SelectInterruptPipe::init(std::string& errorMsg)
    {
        // calling init twice is a programming error
        assert(_fildes[kPipeReadIndex] == -1);
        assert(_fildes[kPipeWriteIndex] == -1);

        if (pipe(_fildes) < 0)
        {
            std::stringstream ss;
            ss << "SelectInterruptPipe::init() failed in pipe() call"
               << " : " << strerror(errno);
            errorMsg = ss.str();
            return false;
        }

        if (fcntl(_fildes[kPipeReadIndex], F_SETFL, O_NONBLOCK) == -1)
        {
            std::stringstream ss;
            ss << "SelectInterruptPipe::init() failed in fcntl() call"
               << " : " << strerror(errno);
            errorMsg = ss.str();

            _fildes[kPipeReadIndex] = -1;
            _fildes[kPipeWriteIndex] = -1;
            return false;
        }

        if (fcntl(_fildes[kPipeWriteIndex], F_SETFL, O_NONBLOCK) == -1)
        {
            std::stringstream ss;
            ss << "SelectInterruptPipe::init() failed in fcntl() call"
               << " : " << strerror(errno);
            errorMsg = ss.str();

            _fildes[kPipeReadIndex] = -1;
            _fildes[kPipeWriteIndex] = -1;
            return false;
        }

        //
        // FIXME: on macOS we should configure the pipe to not trigger SIGPIPE
        // on reads/writes to a closed fd
        //
        // The generation of the SIGPIPE signal can be suppressed using the
        // F_SETNOSIGPIPE fcntl command.
        //

        return true;
    }

    bool SelectInterruptPipe::notify(uint64_t value)
    {
        int fd = _fildes[kPipeWriteIndex];
        if (fd == -1) return false;

        // we should write 8 bytes for an uint64_t
        return write(fd, &value, sizeof(value)) == 8;
    }

    // TODO: return max uint64_t for errors ?
    uint64_t SelectInterruptPipe::read()
    {
        int fd = _fildes[kPipeReadIndex];

        uint64_t value = 0;
        ::read(fd, &value, sizeof(value));
        return value;
    }

    bool SelectInterruptPipe::clear()
    {
        return true;
    }

    int SelectInterruptPipe::getFd() const
    {
        return _fildes[kPipeReadIndex];
    }
}