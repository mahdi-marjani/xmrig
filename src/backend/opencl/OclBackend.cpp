/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
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


#include <mutex>


#include "backend/common/Hashrate.h"
#include "backend/common/interfaces/IWorker.h"
#include "backend/common/Workers.h"
#include "backend/opencl/OclBackend.h"
#include "backend/opencl/OclConfig.h"
#include "backend/opencl/OclLaunchData.h"
#include "backend/opencl/wrappers/OclLib.h"
#include "base/io/log/Log.h"
#include "base/net/stratum/Job.h"
#include "base/tools/Chrono.h"
#include "base/tools/String.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "rapidjson/document.h"


#ifdef XMRIG_FEATURE_API
#   include "base/api/interfaces/IApiRequest.h"
#endif


namespace xmrig {


extern template class Threads<OclThreads>;


static const char *tag      = MAGENTA_BG_BOLD(WHITE_BOLD_S " ocl ");
static const String kType   = "opencl";


struct LaunchStatus
{
public:
    inline void reset()
    {
        hugePages = 0;
        memory    = 0;
        pages     = 0;
        started   = 0;
        threads   = 0;
        ways      = 0;
        ts        = Chrono::steadyMSecs();
    }

    size_t hugePages    = 0;
    size_t memory       = 0;
    size_t pages        = 0;
    size_t started      = 0;
    size_t threads      = 0;
    size_t ways         = 0;
    uint64_t ts         = 0;
};


class OclBackendPrivate
{
public:
    inline OclBackendPrivate(Controller *controller) :
        controller(controller)
    {
    }


    bool init(const OclConfig &cl)
    {
        if (!OclLib::init(cl.loader())) {
            LOG_ERR("%s" RED_S " failed to load OpenCL runtime (%s): " RED_BOLD_S "\"%s\"", tag, OclLib::loader().data(), OclLib::lastError());

            return false;
        }

        if (!platform.isValid()) {
            platform = cl.platform();

            if (!platform.isValid()) {
                LOG_ERR("%s" RED_S " selected OpenCL platform NOT found.", tag);

                return false;
            }

            LOG_INFO("%s use platform " WHITE_BOLD("%s") "/" WHITE_BOLD("%s"), tag, platform.name().data(), platform.version().data());
        }

        return true;
    }


    inline void start()
    {
        LOG_INFO("%s use profile " BLUE_BG(WHITE_BOLD_S " %s ") WHITE_BOLD_S " (" CYAN_BOLD("%zu") WHITE_BOLD(" threads)") " scratchpad " CYAN_BOLD("%zu KB"),
                 tag,
                 profileName.data(),
                 threads.size(),
                 algo.l3() / 1024
                 );

        workers.stop();

        status.reset();
        status.memory   = algo.l3();
        status.threads  = threads.size();

        for (const OclLaunchData &data : threads) {
            status.ways += static_cast<size_t>(data.intensity);
        }

        workers.start(threads);
    }


    size_t ways()
    {
        std::lock_guard<std::mutex> lock(mutex);

        return status.ways;
    }


    Algorithm algo;
    Controller *controller;
    LaunchStatus status;
    OclPlatform platform;
    std::mutex mutex;
    std::vector<OclLaunchData> threads;
    String profileName;
    Workers<OclLaunchData> workers;
};


} // namespace xmrig


xmrig::OclBackend::OclBackend(Controller *controller) :
    d_ptr(new OclBackendPrivate(controller))
{
    d_ptr->workers.setBackend(this);
}


xmrig::OclBackend::~OclBackend()
{
    delete d_ptr;

    OclLib::close();
}


bool xmrig::OclBackend::isEnabled() const
{
    return d_ptr->controller->config()->cl().isEnabled();
}


bool xmrig::OclBackend::isEnabled(const Algorithm &algorithm) const
{
    return !d_ptr->controller->config()->cl().threads().get(algorithm).isEmpty();
}


const xmrig::Hashrate *xmrig::OclBackend::hashrate() const
{
    return d_ptr->workers.hashrate();
}


const xmrig::String &xmrig::OclBackend::profileName() const
{
    return d_ptr->profileName;
}


const xmrig::String &xmrig::OclBackend::type() const
{
    return kType;
}


void xmrig::OclBackend::prepare(const Job &)
{
}


void xmrig::OclBackend::printHashrate(bool details)
{
    if (!details || !hashrate()) {
        return;
    }

    char num[8 * 3] = { 0 };

    Log::print(WHITE_BOLD_S "| OPENCL THREAD | AFFINITY | 10s H/s | 60s H/s | 15m H/s |");

    size_t i = 0;
    for (const OclLaunchData &data : d_ptr->threads) {
         Log::print("| %13zu | %8" PRId64 " | %7s | %7s | %7s |",
                    i,
                    data.affinity,
                    Hashrate::format(hashrate()->calc(i, Hashrate::ShortInterval),  num,         sizeof num / 3),
                    Hashrate::format(hashrate()->calc(i, Hashrate::MediumInterval), num + 8,     sizeof num / 3),
                    Hashrate::format(hashrate()->calc(i, Hashrate::LargeInterval),  num + 8 * 2, sizeof num / 3)
                    );

         i++;
    }
}


void xmrig::OclBackend::setJob(const Job &job)
{
    if (!isEnabled()) {
        return stop();
    }

    const OclConfig &cl = d_ptr->controller->config()->cl();
    if (!d_ptr->init(cl)) {
        return;
    }

    std::vector<OclLaunchData> threads = cl.get(d_ptr->controller->miner(), job.algorithm());
//    if (d_ptr->threads.size() == threads.size() && std::equal(d_ptr->threads.begin(), d_ptr->threads.end(), threads.begin())) {
//        return;
//    }

    d_ptr->algo         = job.algorithm();
    d_ptr->profileName  = cl.threads().profileName(job.algorithm());

//    if (d_ptr->profileName.isNull() || threads.empty()) {
//        d_ptr->workers.stop();

//        LOG_WARN(YELLOW_BOLD_S "CPU disabled, no suitable configuration for algo %s", job.algorithm().shortName());

//        return;
//    }

    d_ptr->threads = std::move(threads);
    d_ptr->start();
}


void xmrig::OclBackend::start(IWorker *worker)
{
    d_ptr->mutex.lock();

    d_ptr->status.started++;

    if (d_ptr->status.started == d_ptr->status.threads) {
    }

    d_ptr->mutex.unlock();

    worker->start();
}


void xmrig::OclBackend::stop()
{
    const uint64_t ts = Chrono::steadyMSecs();

    d_ptr->workers.stop();
    d_ptr->threads.clear();

    LOG_INFO("%s" YELLOW(" stopped") BLACK_BOLD(" (%" PRIu64 " ms)"), tag, Chrono::steadyMSecs() - ts);
}


void xmrig::OclBackend::tick(uint64_t ticks)
{
    d_ptr->workers.tick(ticks);
}


#ifdef XMRIG_FEATURE_API
rapidjson::Value xmrig::OclBackend::toJSON(rapidjson::Document &doc) const
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    Value out(kObjectType);
    out.AddMember("type",       type().toJSON(), allocator);
    out.AddMember("enabled",    isEnabled(), allocator);
    out.AddMember("algo",       d_ptr->algo.toJSON(), allocator);
    out.AddMember("profile",    profileName().toJSON(), allocator);
    out.AddMember("platform",   d_ptr->platform.toJSON(doc), allocator);

    if (d_ptr->threads.empty() || !hashrate()) {
        return out;
    }

    Value threads(kArrayType);
    const Hashrate *hr = hashrate();

    size_t i = 0;
    for (const OclLaunchData &data : d_ptr->threads) {
        Value thread(kObjectType);
        thread.AddMember("intensity",   data.intensity, allocator);
        thread.AddMember("affinity",    data.affinity, allocator);

        Value hashrate(kArrayType);
        hashrate.PushBack(Hashrate::normalize(hr->calc(i, Hashrate::ShortInterval)),  allocator);
        hashrate.PushBack(Hashrate::normalize(hr->calc(i, Hashrate::MediumInterval)), allocator);
        hashrate.PushBack(Hashrate::normalize(hr->calc(i, Hashrate::LargeInterval)),  allocator);

        i++;

        thread.AddMember("hashrate", hashrate, allocator);
        threads.PushBack(thread, allocator);
    }

    out.AddMember("threads", threads, allocator);

    return out;
}


void xmrig::OclBackend::handleRequest(IApiRequest &)
{
}
#endif
