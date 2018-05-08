#include "merit/miner.hpp"
#include "stratum/stratum.hpp"

#include <iostream>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace merit
{
    void test()
    {
        std::cerr << "Test!!!" << std::endl;
    }

    stratum::Client client;

    bool connect_stratum(
            const std::string& url,
            const std::string& user,
            const std::string& pass)
    {
        stratum::CurlOptions opts;
        if(!client.connect(url, user, pass, opts)) {
            BOOST_LOG_TRIVIAL(error) << "error connecting to stratum server: " << url; 
            return false;
        }
        if(!client.subscribe()) {
            BOOST_LOG_TRIVIAL(error) << "error subscribing to stratum server: " << url; 
            return false;
        }

        if(!client.authorize()) {
            BOOST_LOG_TRIVIAL(error) << "error authorize to stratum server: " << url; 
            return false;
        }

        return true;
    }
    
    void init_logging()
    {
    }
}


