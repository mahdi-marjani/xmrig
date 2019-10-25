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


#include <stdexcept>
#include <uv.h>


#include "backend/cuda/wrappers/CudaLib.h"
#include "base/io/log/Log.h"


namespace xmrig {


enum Version : uint32_t
{
    ApiVersion,
    DriverVersion,
    RuntimeVersion
};


static uv_lib_t cudaLib;


static const char *kAlloc                               = "alloc";
static const char *kDeviceCount                         = "deviceCount";
static const char *kPluginVersion                       = "pluginVersion";
static const char *kRelease                             = "release";
static const char *kSymbolNotFound                      = "symbol not found";
static const char *kVersion                             = "version";


using alloc_t                                           = nvid_ctx * (*)(size_t, int, int, int, int, int);
using deviceCount_t                                     = size_t (*)();
using pluginVersion_t                                   = const char * (*)();
using release_t                                         = void (*)(nvid_ctx *);
using version_t                                         = uint32_t (*)(Version);


static alloc_t pAlloc                                   = nullptr;
static deviceCount_t pDeviceCount                       = nullptr;
static pluginVersion_t pPluginVersion                   = nullptr;
static release_t pRelease                               = nullptr;
static version_t pVersion                               = nullptr;


#define DLSYM(x) if (uv_dlsym(&cudaLib, k##x, reinterpret_cast<void**>(&p##x)) == -1) { throw std::runtime_error(kSymbolNotFound); }


bool CudaLib::m_initialized = false;
bool CudaLib::m_ready       = false;
String CudaLib::m_loader;


} // namespace xmrig


bool xmrig::CudaLib::init(const char *fileName)
{
    if (!m_initialized) {
        m_loader      = fileName == nullptr ? defaultLoader() : fileName;
        m_ready       = uv_dlopen(m_loader, &cudaLib) == 0 && load();
        m_initialized = true;
    }

    return m_ready;
}


const char *xmrig::CudaLib::lastError()
{
    return uv_dlerror(&cudaLib);
}


void xmrig::CudaLib::close()
{
    uv_dlclose(&cudaLib);
}


const char *xmrig::CudaLib::pluginVersion() noexcept
{
    return pPluginVersion();
}


nvid_ctx *xmrig::CudaLib::alloc(size_t id, int blocks, int threads, int bfactor, int bsleep, const Algorithm &algorithm) noexcept
{
    return pAlloc(id, blocks, threads, bfactor, bsleep, algorithm);
}


size_t xmrig::CudaLib::deviceCount() noexcept
{
    return pDeviceCount();
}


uint32_t xmrig::CudaLib::driverVersion() noexcept
{
    return pVersion(DriverVersion);
}


uint32_t xmrig::CudaLib::runtimeVersion() noexcept
{
    return pVersion(RuntimeVersion);
}


void xmrig::CudaLib::release(nvid_ctx *ctx) noexcept
{
    pRelease(ctx);
}


bool xmrig::CudaLib::load()
{
    if (uv_dlsym(&cudaLib, kVersion, reinterpret_cast<void**>(&pVersion)) == -1) {
        return false;
    }

    if (pVersion(ApiVersion) != 1u) {
        return false;
    }

    try {
        DLSYM(Alloc);
        DLSYM(DeviceCount);
        DLSYM(PluginVersion);
        DLSYM(Release);
        DLSYM(Version);
    } catch (std::exception &ex) {
        return false;
    }

    return true;
}


const char *xmrig::CudaLib::defaultLoader()
{
#   if defined(__APPLE__)
    return "/System/Library/Frameworks/OpenCL.framework/OpenCL"; // FIXME
#   elif defined(_WIN32)
    return "xmrig-cuda.dll";
#   else
    return "xmrig-cuda.so";
#   endif
}
