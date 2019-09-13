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


#include "backend/opencl/OclConfig.h"
#include "backend/opencl/wrappers/OclLib.h"
#include "base/io/json/Json.h"
#include "base/io/log/Log.h"
#include "rapidjson/document.h"


namespace xmrig {

static const char *kAMD         = "AMD";
static const char *kCache       = "cache";
static const char *kCn          = "cn";
static const char *kCn2         = "cn/2";
static const char *kEnabled     = "enabled";
static const char *kINTEL       = "INTEL";
static const char *kLoader      = "loader";
static const char *kNVIDIA      = "NVIDIA";
static const char *kPlatform    = "platform";


#ifdef XMRIG_ALGO_CN_GPU
static const char *kCnGPU = "cn/gpu";
#endif

#ifdef XMRIG_ALGO_CN_LITE
static const char *kCnLite = "cn-lite";
#endif

#ifdef XMRIG_ALGO_CN_HEAVY
static const char *kCnHeavy = "cn-heavy";
#endif

#ifdef XMRIG_ALGO_CN_PICO
static const char *kCnPico = "cn-pico";
#endif

#ifdef XMRIG_ALGO_RANDOMX
static const char *kRx    = "rx";
static const char *kRxWOW = "rx/wow";
#endif

#ifdef XMRIG_ALGO_ARGON2
//static const char *kArgon2     = "argon2";
#endif


extern template class Threads<OclThreads>;


static size_t generate(const char *key, Threads<OclThreads> &threads, const Algorithm &algorithm, const std::vector<OclDevice> &devices)
{
    if (threads.has(key) || threads.isExist(algorithm)) {
        return 0;
    }

    OclThreads profile;
    for (const OclDevice &device : devices) {
        device.generate(algorithm, profile);
    }

    const size_t count = profile.count();
    threads.move(key, std::move(profile));

    return count;
}


}


xmrig::OclConfig::OclConfig() :
    m_platformVendor(kAMD)
{
}


xmrig::OclPlatform xmrig::OclConfig::platform() const
{
    const auto platforms = OclPlatform::get();
    if (platforms.empty()) {
        return {};
    }

    if (!m_platformVendor.isEmpty()) {
        String search;
        String vendor = m_platformVendor;
        vendor.toUpper();

        if (vendor == kAMD) {
            search = "Advanced Micro Devices";
        }
        else if (vendor == kNVIDIA) {
            search = kNVIDIA;
        }
        else if (vendor == kINTEL) {
            search = "Intel";
        }
        else {
            search = m_platformVendor;
        }

        for (const auto &platform : platforms) {
            if (platform.vendor().contains(search)) {
                return platform;
            }
        }
    }
    else if (m_platformIndex < platforms.size()) {
        return platforms[m_platformIndex];
    }

    return {};
}


rapidjson::Value xmrig::OclConfig::toJSON(rapidjson::Document &doc) const
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    Value obj(kObjectType);

    obj.AddMember(StringRef(kEnabled),  m_enabled, allocator);
    obj.AddMember(StringRef(kCache),    m_cache, allocator);
    obj.AddMember(StringRef(kLoader),   m_loader.toJSON(), allocator);
    obj.AddMember(StringRef(kPlatform), m_platformVendor.isEmpty() ? Value(m_platformIndex) : m_platformVendor.toJSON(), allocator);

    m_threads.toJSON(obj, doc);

    return obj;
}


std::vector<xmrig::OclLaunchData> xmrig::OclConfig::get(const Miner *miner, const Algorithm &algorithm, const OclPlatform &platform, const std::vector<OclDevice> &devices, const char *tag) const
{
    std::vector<OclLaunchData> out;
    const OclThreads &threads = m_threads.get(algorithm);

    if (threads.isEmpty()) {
        return out;
    }

    out.reserve(threads.count() * 2);

    for (const OclThread &thread : threads.data()) {
        if (thread.index() >= devices.size()) {
            LOG_INFO("%s" YELLOW(" skip non-existing device with index ") YELLOW_BOLD("%u"), tag, thread.index());
            continue;
        }

#       ifdef XMRIG_ALGO_RANDOMX
        auto dataset = algorithm.family() == Algorithm::RANDOM_X ? std::make_shared<OclRxDataset>() : nullptr;
#       endif

        if (thread.threads().size() > 1) {
            auto interleave = std::make_shared<OclInterleave>(thread.threads().size());

            for (int64_t affinity : thread.threads()) {
                OclLaunchData data(miner, algorithm, *this, platform, thread, devices[thread.index()], affinity);
                data.interleave = interleave;

#               ifdef XMRIG_ALGO_RANDOMX
                data.dataset = dataset;
#               endif

                out.emplace_back(std::move(data));
            }
        }
        else {
            OclLaunchData data(miner, algorithm, *this, platform, thread, devices[thread.index()], thread.threads().front());

#           ifdef XMRIG_ALGO_RANDOMX
            data.dataset = dataset;
#           endif

            out.emplace_back(std::move(data));
        }
    }

    return out;
}


void xmrig::OclConfig::read(const rapidjson::Value &value)
{
    if (value.IsObject()) {
        m_enabled   = Json::getBool(value, kEnabled, m_enabled);
        m_cache     = Json::getBool(value, kCache, m_cache);
        m_loader    = Json::getString(value, kLoader);

        setPlatform(Json::getValue(value, kPlatform));

        if (isEnabled()) {
            m_threads.read(value);

            generate();
        }
    }
    else if (value.IsBool() && value.IsFalse()) {
        m_enabled = false;
    }
    else {
        generate();
    }
}


void xmrig::OclConfig::generate()
{
    if (!OclLib::init(loader())) {
        return;
    }

    const auto devices = platform().devices();
    if (devices.empty()) {
        return;
    }

    size_t count = 0;

    count += xmrig::generate(kCn, m_threads, Algorithm::CN_0, devices);
    count += xmrig::generate(kCn2, m_threads, Algorithm::CN_2, devices);

    if (!m_threads.isExist(Algorithm::CN_0)) {
        m_threads.disable(Algorithm::CN_0);
        count++;
    }

#   ifdef XMRIG_ALGO_CN_GPU
    count += xmrig::generate(kCnGPU, m_threads, Algorithm::CN_GPU, devices);
#   endif

#   ifdef XMRIG_ALGO_CN_LITE
    count += xmrig::generate(kCnLite, m_threads, Algorithm::CN_LITE_1, devices);

    if (!m_threads.isExist(Algorithm::CN_LITE_0)) {
        m_threads.disable(Algorithm::CN_LITE_0);
        count++;
    }
#   endif

#   ifdef XMRIG_ALGO_CN_HEAVY
    count += xmrig::generate(kCnHeavy, m_threads, Algorithm::CN_HEAVY_0, devices);
#   endif

#   ifdef XMRIG_ALGO_CN_PICO
    count += xmrig::generate(kCnPico, m_threads, Algorithm::CN_PICO_0, devices);
#   endif

#   ifdef XMRIG_ALGO_RANDOMX
    count += xmrig::generate(kRx, m_threads, Algorithm::RX_0, devices);
    count += xmrig::generate(kRxWOW, m_threads, Algorithm::RX_WOW, devices);
#   endif

    m_shouldSave = count > 0;
}


void xmrig::OclConfig::setPlatform(const rapidjson::Value &platform)
{
    if (platform.IsString()) {
        m_platformVendor = platform.GetString();
    }
    else if (platform.IsUint()) {
        m_platformVendor = nullptr;
        m_platformIndex  = platform.GetUint();
    }
}
