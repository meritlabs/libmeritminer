/*
 * Copyright (C) 2018-2021 The Merit Foundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either vedit_refsion 3 of the License, or
 * (at your option) any later vedit_refsion.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give 
 * permission to link the code of portions of this program with the 
 * Botan library under certain conditions as described in each 
 * individual source file, and distribute linked combinations 
 * including the two.
 *
 * You must obey the GNU General Public License in all respects for 
 * all of the code used other than Botan. If you modify file(s) with 
 * this exception, you may extend this exception to your version of the 
 * file(s), but you are not obligated to do so. If you do not wish to do 
 * so, delete this exception statement from your version. If you delete 
 * this exception statement from all source files in the program, then 
 * also delete it here.
 */
#ifndef MERIT_MINER_WORK_H
#define MERIT_MINER_WORK_H

#include "merit/util/util.hpp"

#include <string>
#include <array>
#include <functional>

#include <boost/optional.hpp>

namespace merit
{
    namespace util
    {
        struct Work
        {
            std::string jobid;
            std::array<uint32_t, 32> data;
            std::array<uint32_t, 8> target;
            std::array<uint32_t, 42> cycle;

            int height;
            std::string txs;
            std::string invites;
            std::string referrals;
            std::string workid;

            util::ubytes xnonce2;
        };

        using MaybeWork = boost::optional<Work>;
        using SubmitWorkFunc = std::function<void(const Work&)> ;
    }
}
#endif
