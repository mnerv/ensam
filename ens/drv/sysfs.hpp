/**
 * sysfs file read/write wrapper
 */
#ifndef ENS_DRV_SYSFS_HPP
#define ENS_DRV_SYSFS_HPP

#include <string>

namespace ens {
class sysfs {
public:
    sysfs(std::string const& path, int const flag);
    ~sysfs();

    auto r() const -> std::string;
    auto w(std::string const& value) const -> void;

    auto native() const -> int { return m_fd; }

private:
    int m_fd;
};
}

#endif  // !ENS_SYSFS_HPP
