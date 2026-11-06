//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once

// Helper class for file sink
// When failing to open a file, retry several times(5) with small delay between the tries(10 ms)
// Can be set to auto flush on every line
// Throw quantlog_ex exception on errors

#include <lquantlog/details/os.h>
#include <lquantlog/details/log_msg.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <cerrno>

namespace lquantlog
{
namespace details
{

class file_helper
{

public:
    const int open_tries = 5;
    const int open_interval = 10;

    explicit file_helper() :
        _fd(nullptr)
    {}

    file_helper(const file_helper&) = delete;
    file_helper& operator=(const file_helper&) = delete;

    ~file_helper()
    {
        close();
    }


    void open(const filename_t& fname, bool truncate = false)
    {

        close();
        auto *mode = truncate ? SPDLOG_FILENAME_T("wb") : SPDLOG_FILENAME_T("ab");
        _filename = fname;
        for (int tries = 0; tries < open_tries; ++tries)
        {
            if (!os::fopen_s(&_fd, fname, mode))
                return;

            std::this_thread::sleep_for(std::chrono::milliseconds(open_interval));
        }

       // throw quantlog_ex("Failed opening file " + os::filename_to_str(_filename) + " for writing", errno);
    }

    void reopen(bool truncate)
    {
        if (_filename.empty())
            return;
         //   throw quantlog_ex("Failed re opening file - was not opened before");
        open(_filename, truncate);

    }

    void flush()
    {
        std::fflush(_fd);
    }

    void close()
    {
        if (_fd)
        {
            std::fclose(_fd);
            _fd = nullptr;
        }
    }

    void write(const log_msg& msg)
    {

        size_t msg_size = msg.formatted.size();
        auto data = msg.formatted.data();
        if (std::fwrite(data, 1, msg_size, _fd) != msg_size)
            return;
           // throw quantlog_ex("Failed writing to file " + os::filename_to_str(_filename), errno);
    }

    size_t size()
    {
        if (!_fd)
            return 0;
           // throw quantlog_ex("Cannot use size() on closed file " + os::filename_to_str(_filename));
        return os::filesize(_fd);
    }

    const filename_t& filename() const
    {
        return _filename;
    }

    static bool file_exists(const filename_t& name)
    {

        return os::file_exists(name);
    }

private:
    FILE* _fd;
    filename_t _filename;
};
}
}
