/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2019 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018-2019 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2019 XMRig       <support@xmrig.com>
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


#ifdef XMRIG_HWLOC_DEBUG
#   include <uv.h>
#endif


#include <algorithm>
#include <hwloc.h>


#include "backend/cpu/platform/HwlocCpuInfo.h"


namespace xmrig {


static inline bool isCacheObject(hwloc_obj_t obj)
{
#   if HWLOC_API_VERSION >= 0x20000
    return hwloc_obj_type_is_cache(obj->type);
#   else
    return obj->type == HWLOC_OBJ_CACHE;
#   endif
}


template <typename func>
static inline void findCache(hwloc_obj_t obj, unsigned min, unsigned max, func lambda)
{
    for (size_t i = 0; i < obj->arity; i++) {
        if (isCacheObject(obj->children[i])) {
            const unsigned depth = obj->children[i]->attr->cache.depth;
            if (depth < min || depth > max) {
                continue;
            }

            lambda(obj->children[i]);
        }

        findCache(obj->children[i], min, max, lambda);
    }
}


template <typename func>
static inline void findByType(hwloc_obj_t obj, hwloc_obj_type_t type, func lambda)
{
    for (size_t i = 0; i < obj->arity; i++) {
        if (obj->children[i]->type == type) {
            lambda(obj->children[i]);
        }
        else {
            findByType(obj->children[i], type, lambda);
        }
    }
}


static inline size_t countByType(hwloc_topology_t topology, hwloc_obj_type_t type)
{
    const int count = hwloc_get_nbobjs_by_type(topology, type);

    return count > 0 ? static_cast<size_t>(count) : 0;
}


static inline size_t countByType(hwloc_obj_t obj, hwloc_obj_type_t type)
{
    size_t count = 0;
    findByType(obj, type, [&count](hwloc_obj_t) { count++; });

    return count;
}


static inline bool isCacheExclusive(hwloc_obj_t obj)
{
    const char *value = hwloc_obj_get_info_by_name(obj, "Inclusive");
    return value == nullptr || value[0] != '1';
}


} // namespace xmrig


xmrig::HwlocCpuInfo::HwlocCpuInfo() : BasicCpuInfo(),
    m_backend(),
    m_cache()
{
    m_threads = 0;

    hwloc_topology_init(&m_topology);
    hwloc_topology_load(m_topology);

#   ifdef XMRIG_HWLOC_DEBUG
#   if defined(UV_VERSION_HEX) && UV_VERSION_HEX >= 0x010c00
    {
        char env[520] = { 0 };
        size_t size   = sizeof(env);

        if (uv_os_getenv("HWLOC_XMLFILE", env, &size) == 0) {
            printf("use HWLOC XML file: \"%s\"\n", env);
        }
    }
#   endif

    std::vector<hwloc_obj_t> packages;
    findByType(hwloc_get_root_obj(m_topology), HWLOC_OBJ_PACKAGE, [&packages](hwloc_obj_t found) { packages.emplace_back(found); });
    if (packages.size()) {
        const char *value = hwloc_obj_get_info_by_name(packages[0], "CPUModel");
        if (value) {
            strncpy(m_brand, value, 64);
        }
    }
#   endif

    hwloc_obj_t root = hwloc_get_root_obj(m_topology);
    snprintf(m_backend, sizeof m_backend, "hwloc/%s", hwloc_obj_get_info_by_name(root, "hwlocVersion"));

    findCache(root, 2, 3, [this](hwloc_obj_t found) { this->m_cache[found->attr->cache.depth] += found->attr->cache.size; });

    m_threads   = countByType(m_topology, HWLOC_OBJ_PU);
    m_cores     = countByType(m_topology, HWLOC_OBJ_CORE);
    m_nodes     = std::max<size_t>(countByType(m_topology, HWLOC_OBJ_NUMANODE), 1);
    m_packages  = countByType(m_topology, HWLOC_OBJ_PACKAGE);
}


xmrig::HwlocCpuInfo::~HwlocCpuInfo()
{
    hwloc_topology_destroy(m_topology);
}


xmrig::CpuThreads xmrig::HwlocCpuInfo::threads(const Algorithm &algorithm) const
{
    if (L2() == 0 && L3() == 0) {
        return BasicCpuInfo::threads(algorithm);
    }

    const unsigned depth = L3() > 0 ? 3 : 2;

    CpuThreads threads;
    threads.reserve(m_threads);

    std::vector<hwloc_obj_t> caches;
    caches.reserve(16);

    findCache(hwloc_get_root_obj(m_topology), depth, depth, [&caches](hwloc_obj_t found) { caches.emplace_back(found); });

    for (hwloc_obj_t cache : caches) {
        processTopLevelCache(cache, algorithm, threads);
    }

    return threads;
}


void xmrig::HwlocCpuInfo::processTopLevelCache(hwloc_obj_t cache, const Algorithm &algorithm, CpuThreads &threads) const
{
    size_t PUs = countByType(cache, HWLOC_OBJ_PU);
    if (PUs == 0) {
        return;
    }

    size_t size             = cache->attr->cache.size;
    const size_t scratchpad = algorithm.memory();

    if (cache->attr->cache.depth == 3 && isCacheExclusive(cache)) {
        for (size_t i = 0; i < cache->arity; ++i) {
            hwloc_obj_t l2 = cache->children[i];
            if (isCacheObject(l2) && l2->attr != nullptr && l2->attr->cache.size >= scratchpad) {
                size += scratchpad;
            }
        }
    }

    std::vector<hwloc_obj_t> cores;
    cores.reserve(m_cores);
    findByType(cache, HWLOC_OBJ_CORE, [&cores](hwloc_obj_t found) { cores.emplace_back(found); });

    size_t cacheHashes = (size + (scratchpad / 2)) / scratchpad;

#   ifdef XMRIG_ALGO_CN_GPU
    if (algorithm == Algorithm::CN_GPU) {
        cacheHashes = PUs;
    }
#   endif

    if (cacheHashes >= PUs) {
        for (hwloc_obj_t core : cores) {
            if (core->arity == 0) {
                continue;
            }

            for (unsigned i = 0; i < core->arity; ++i) {
                if (core->children[i]->type == HWLOC_OBJ_PU) {
                    threads.push_back(CpuThread(1, core->children[i]->os_index));
                }
            }
        }

        return;
    }

    size_t pu_id = 0;
    while (cacheHashes > 0 && PUs > 0) {
        bool allocated_pu = false;

        for (hwloc_obj_t core : cores) {
            if (core->arity <= pu_id || core->children[pu_id]->type != HWLOC_OBJ_PU) {
                continue;
            }

            cacheHashes--;
            PUs--;

            allocated_pu = true;
            threads.push_back(CpuThread(1, core->children[pu_id]->os_index));

            if (cacheHashes == 0) {
                break;
            }
        }

        if (!allocated_pu) {
            break;
        }

        pu_id++;
    }
}
