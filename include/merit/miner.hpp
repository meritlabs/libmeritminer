#ifndef MERITMINER_H
#define MERITMINER_H

#include <string>

namespace merit
{
    void test();
    bool connect_stratum(
            const std::string& url,
            const std::string& user,
            const std::string& pass);
    void init_logging();
}
#endif //MERITMINER_H
