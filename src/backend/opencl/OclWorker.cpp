/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2018-2019 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2019 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <assert.h>
#include <thread>


#include "backend/opencl/OclWorker.h"
#include "core/Miner.h"
#include "crypto/common/Nonce.h"
#include "net/JobResults.h"


namespace xmrig {

static constexpr uint32_t kReserveCount = 4096;

} // namespace xmrig



xmrig::OclWorker::OclWorker(size_t index, const OclLaunchData &data) :
    Worker(index, data.thread.affinity(), -1),
    m_algorithm(data.algorithm),
    m_miner(data.miner)
{
}


xmrig::OclWorker::~OclWorker()
{
}


bool xmrig::OclWorker::selfTest()
{
    return true;
}


void xmrig::OclWorker::start()
{
    while (Nonce::sequence(Nonce::OPENCL) > 0) {
        if (Nonce::isPaused()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            while (Nonce::isPaused() && Nonce::sequence(Nonce::OPENCL) > 0);

            if (Nonce::sequence(Nonce::OPENCL) == 0) {
                break;
            }

            consumeJob();
        }

        while (!Nonce::isOutdated(Nonce::OPENCL, m_job.sequence())) {
            if ((m_count & 0x7) == 0) {
                storeStats();
            }

            const Job &job = m_job.currentJob();

            if (job.algorithm().l3() != m_algorithm.l3()) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // FIXME

            m_job.nextRound(kReserveCount);

            std::this_thread::yield();
        }

        consumeJob();
    }
}


void xmrig::OclWorker::consumeJob()
{
    m_job.add(m_miner->job(), Nonce::sequence(Nonce::OPENCL), kReserveCount);
}
