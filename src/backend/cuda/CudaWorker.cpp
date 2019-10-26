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


#include "backend/cuda/CudaWorker.h"
#include "backend/common/Tags.h"
#include "base/io/log/Log.h"
#include "base/tools/Chrono.h"
#include "core/Miner.h"
#include "crypto/common/Nonce.h"
#include "net/JobResults.h"


#include <cassert>
#include <thread>


namespace xmrig {


static constexpr uint32_t kReserveCount = 32768;
std::atomic<bool> CudaWorker::ready;


static inline bool isReady()                         { return !Nonce::isPaused() && CudaWorker::ready; }
static inline uint32_t roundSize(uint32_t intensity) { return kReserveCount / intensity + 1; }


static inline void printError(size_t id, const char *error)
{
    LOG_ERR("%s" RED_S " thread " RED_BOLD("#%zu") RED_S " failed with error " RED_BOLD("%s"), cuda_tag(), id, error);
}


} // namespace xmrig



xmrig::CudaWorker::CudaWorker(size_t id, const CudaLaunchData &data) :
    Worker(id, data.thread.affinity(), -1),
    m_algorithm(data.algorithm),
    m_miner(data.miner),
    m_intensity(data.thread.threads() * data.thread.blocks())
{
}


xmrig::CudaWorker::~CudaWorker()
{
//    delete m_runner;
}


bool xmrig::CudaWorker::selfTest()
{
    return false; // FIXME
}


size_t xmrig::CudaWorker::intensity() const
{
    return 0; // FIXME;
//    return m_runner ? m_runner->intensity() : 0;
}


void xmrig::CudaWorker::start()
{
    while (Nonce::sequence(Nonce::CUDA) > 0) {
        if (!isReady()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            while (!isReady() && Nonce::sequence(Nonce::CUDA) > 0);

            if (Nonce::sequence(Nonce::CUDA) == 0) {
                break;
            }

            if (!consumeJob()) {
                return;
            }
        }

        while (!Nonce::isOutdated(Nonce::CUDA, m_job.sequence())) {
//            try {
//                m_runner->run(*m_job.nonce(), results);
//            }
//            catch (std::exception &ex) {
//                printError(id(), ex.what());

//                return;
//            }

//            if (results[0xFF] > 0) {
//                JobResults::submit(m_job.currentJob(), results, results[0xFF]);
//            }

            m_job.nextRound(roundSize(m_intensity), m_intensity);

            storeStats();
            std::this_thread::yield();
        }

        if (!consumeJob()) {
            return;
        }
    }
}


bool xmrig::CudaWorker::consumeJob()
{
    if (Nonce::sequence(Nonce::CUDA) == 0) {
        return false;
    }

    m_job.add(m_miner->job(), Nonce::sequence(Nonce::CUDA), roundSize(m_intensity) * m_intensity);

//    try {
//        m_runner->set(m_job.currentJob(), m_job.blob());
//    }
//    catch (std::exception &ex) {
//        printError(id(), ex.what());

//        return false;
//    }

    return true;
}


void xmrig::CudaWorker::storeStats()
{
    if (!isReady()) {
        return;
    }

    m_count += m_intensity;

    Worker::storeStats();
}
