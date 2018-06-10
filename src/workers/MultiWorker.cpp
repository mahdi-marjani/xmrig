/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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


#include <thread>


#include "crypto/CryptoNight_test.h"
#include "workers/CpuThread.h"
#include "workers/MultiWorker.h"
#include "workers/Workers.h"


template<size_t N>
MultiWorker<N>::MultiWorker(Handle *handle)
    : Worker(handle)
{
    m_memory = Mem::create(m_ctx, m_thread->algorithm(), N);
}


template<size_t N>
MultiWorker<N>::~MultiWorker()
{
    Mem::release(m_ctx, N, m_memory);
}


template<size_t N>
bool MultiWorker<N>::selfTest()
{
    if (m_thread->algorithm() == xmrig::CRYPTONIGHT) {
        return verify(xmrig::VARIANT_0,   test_output_v0) &&
               verify(xmrig::VARIANT_1,   test_output_v1) &&
               verify(xmrig::VARIANT_XTL, test_output_xtl) &&
               verify(xmrig::VARIANT_MSR, test_output_msr);
    }

#   ifndef XMRIG_NO_AEON
    if (m_thread->algorithm() == xmrig::CRYPTONIGHT_LITE) {
        return verify(xmrig::VARIANT_0,    test_output_v0_lite) &&
               verify(xmrig::VARIANT_1,    test_output_v1_lite) &&
               verify(xmrig::VARIANT_IPBC, test_output_ipbc_lite);
    }
#   endif

#   ifndef XMRIG_NO_SUMO
    return m_thread->algorithm() == xmrig::CRYPTONIGHT_HEAVY && verify(xmrig::VARIANT_0, test_output_heavy);
#   else
    return false;
#   endif
}


template<size_t N>
void MultiWorker<N>::start()
{
    while (Workers::sequence() > 0) {
        if (Workers::isPaused()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            while (Workers::isPaused());

            if (Workers::sequence() == 0) {
                break;
            }

            consumeJob();
        }

        while (!Workers::isOutdated(m_sequence)) {
            if ((m_count & 0x7) == 0) {
                storeStats();
            }

            m_thread->fn(m_state.job.variant())(m_state.blob, m_state.job.size(), m_hash, m_ctx);

            for (size_t i = 0; i < N; ++i) {
                if (*reinterpret_cast<uint64_t*>(m_hash + (i * 32) + 24) < m_state.job.target()) {
                    Workers::submit(JobResult(m_state.job.poolId(), m_state.job.id(), *nonce(i), m_hash + (i * 32), m_state.job.diff(), m_state.job.algorithm()));
                }

                *nonce(i) += 1;
            }

            m_count += N;

            std::this_thread::yield();
        }

        consumeJob();
    }
}


template<size_t N>
bool MultiWorker<N>::resume(const Job &job)
{
    if (m_state.job.poolId() == -1 && job.poolId() >= 0 && job.id() == m_pausedState.job.id()) {
        m_state = m_pausedState;
        return true;
    }

    return false;
}


template<size_t N>
bool MultiWorker<N>::verify(xmrig::Variant variant, const uint8_t *referenceValue)
{

    xmrig::CpuThread::cn_hash_fun func = m_thread->fn(variant);
    if (!func) {
        return false;
    }

    func(test_input, 76, m_hash, m_ctx);
    return memcmp(m_hash, referenceValue, sizeof m_hash) == 0;
}


template<size_t N>
void MultiWorker<N>::consumeJob()
{
    Job job = Workers::job();
    m_sequence = Workers::sequence();
    if (m_state.job == job) {
        return;
    }

    save(job);

    if (resume(job)) {
        return;
    }

    m_state.job = job;

    const size_t size = m_state.job.size();
    memcpy(m_state.blob, m_state.job.blob(), m_state.job.size());

    if (N > 1) {
        for (size_t i = 1; i < N; ++i) {
            memcpy(m_state.blob + (i * size), m_state.blob, size);
        }
    }

    for (size_t i = 0; i < N; ++i) {
        if (m_state.job.isNicehash()) {
            *nonce(i) = (*nonce(i) & 0xff000000U) + (0xffffffU / m_totalWays * (m_offset + i));
        }
        else {
           *nonce(i) = 0xffffffffU / m_totalWays * (m_offset + i);
        }
    }
}


template<size_t N>
void MultiWorker<N>::save(const Job &job)
{
    if (job.poolId() == -1 && m_state.job.poolId() >= 0) {
        m_pausedState = m_state;
    }
}


template class MultiWorker<1>;
template class MultiWorker<2>;
template class MultiWorker<3>;
template class MultiWorker<4>;
template class MultiWorker<5>;
