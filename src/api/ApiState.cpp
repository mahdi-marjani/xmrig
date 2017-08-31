/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 *
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

#include <string.h>
#include <uv.h>

#if _WIN32
#   include "winsock2.h"
#else
#   include "unistd.h"
#endif


#include "api/ApiState.h"
#include "Cpu.h"
#include "Mem.h"
#include "net/Job.h"
#include "Options.h"
#include "Platform.h"
#include "version.h"


extern "C"
{
#include "crypto/c_keccak.h"
}


ApiState::ApiState()
{
    memset(m_workerId, 0, sizeof(m_workerId));
    if (Options::i()->apiWorkerId()) {
        strncpy(m_workerId, Options::i()->apiWorkerId(), sizeof(m_workerId) - 1);
    }
    else {
        gethostname(m_workerId, sizeof(m_workerId) - 1);
    }

    genId();
}


ApiState::~ApiState()
{
}


const char *ApiState::get(const char *url, size_t *size) const
{
    json_t *reply = json_object();

    getIdentify(reply);
    getMiner(reply);

    return finalize(reply, size);
}


const char *ApiState::finalize(json_t *reply, size_t *size) const
{
    *size = json_dumpb(reply, m_buf, sizeof(m_buf) - 1, JSON_INDENT(4));

    json_decref(reply);
    return m_buf;
}


void ApiState::genId()
{
    memset(m_id, 0, sizeof(m_id));

    uv_interface_address_t *interfaces;
    int count = 0;

    if (uv_interface_addresses(&interfaces, &count) < 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (!interfaces[i].is_internal && interfaces[i].address.address4.sin_family == AF_INET) {
            uint8_t hash[200];

            keccak(reinterpret_cast<const uint8_t *>(interfaces[i].phys_addr), static_cast<int>(sizeof(interfaces[i].phys_addr)), hash, sizeof(hash));
            Job::toHex(hash, 8, m_id);
            break;
        }
    }

    uv_free_interface_addresses(interfaces, count);
}


void ApiState::getIdentify(json_t *reply) const
{
    json_object_set(reply, "id",        json_string(m_id));
    json_object_set(reply, "worker_id", json_string(m_workerId));
}


void ApiState::getMiner(json_t *reply) const
{
    json_t *cpu = json_object();
    json_object_set(reply, "version",   json_string(APP_VERSION));
    json_object_set(reply, "kind",      json_string(APP_KIND));
    json_object_set(reply, "ua",        json_string(Platform::userAgent()));
    json_object_set(reply, "cpu",       cpu);
    json_object_set(reply, "algo",      json_string(Options::i()->algoName()));
    json_object_set(reply, "hugepages", json_boolean(Mem::isHugepagesEnabled()));
    json_object_set(reply, "donate",    json_integer(Options::i()->donateLevel()));

    json_object_set(cpu, "brand",       json_string(Cpu::brand()));
    json_object_set(cpu, "aes",         json_boolean(Cpu::hasAES()));
    json_object_set(cpu, "x64",         json_boolean(Cpu::isX64()));
    json_object_set(cpu, "sockets",     json_integer(Cpu::sockets()));
}
