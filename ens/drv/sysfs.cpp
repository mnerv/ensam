#include "sysfs.hpp"

#include <stdexcept>
#include <fcntl.h>

#include "fmt/format.h"

namespace ens {
sysfs::sysfs(std::string const& path, int const flag) {
    m_fd = open(path.c_str(), flag);
    if (m_fd < 0)
        throw std::runtime_error(fmt::format("{}\n", strerror(errno)));
}
sysfs::~sysfs() {
    close(m_fd);
}

auto sysfs::r() const -> std::string {
    char buffer[256]{};
    auto const ret = read(m_fd, buffer, sizeof buffer);
    if (ret < 0)
        throw std::runtime_error(fmt::format("Error reading file: {}", strerror(errno)));
    return buffer;
}

auto sysfs::w(std::string const& value) const -> void {
    auto const ret = write(m_fd, value.c_str(), value.size());
    if (ret < 0)
        throw std::runtime_error(fmt::format("Error writing to file: {}", strerror(errno)));
}
}
