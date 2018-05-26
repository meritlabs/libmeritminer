#include "miner/miner.hpp"

namespace merit
{
    namespace miner
    {
        Miner::Miner(
                int wokers,
                int threads_per_worker,
                util::SubmitWorkFunc submit_work) :
            _submit_work{submit_work}
        {

        }

        void Miner::submit_job(const stratum::Job&)
        {

        }

        void Miner::run()
        {

        }

        void Miner::stop()
        {

        }

    }
}
