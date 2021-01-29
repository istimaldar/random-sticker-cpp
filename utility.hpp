#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace utility {
    auto get_home() -> std::unique_ptr<std::string> {
#ifdef _WIN32
        auto homePath = getenv("USERPROFILE");
    if (homePath == nullptr || homePath[0] == '\0') {
        homePath = strcat(getenv("HOMEDRIVE"), getenv("HOMEPATH"));
    }
#else
        auto homePath = getenv("HOME");
        if (homePath == nullptr || homePath[0] == '\0') {
            auto pwd = getpwuid(getuid());
            homePath = pwd->pw_dir;
        }
#endif
        auto result = std::make_unique<std::string>(homePath);
        return result;
    }

    auto join_path(std::unique_ptr<std::string> const first,
                   std::unique_ptr<std::string> const second) -> std::unique_ptr<std::string> {
#ifdef _WIN32
        return std::make_unique<std::string>(*first + "\\" + *second);
#else
        return std::make_unique<std::string>(*first + "/" + *second);
#endif
    }
}
