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
//#include <stdlib.h>


#include "net/Job.h"
#include "Console.h"


static inline unsigned char hf_hex2bin(char c, bool &err)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    }
    else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xA;
    }

    err = true;
    return 0;
}


static inline char hf_bin2hex(unsigned char c)
{
    if (c <= 0x9) {
        return '0' + c;
    }

    return 'a' - 0xA + c;
}


Job::Job() :
    m_size(0),
    m_diff(0),
    m_target(0)
{
}


bool Job::setBlob(const char *blob)
{
    if (!blob) {
        return false;
    }

    m_size = strlen(blob);
    if (m_size % 2 != 0) {
        return false;
    }

    m_size /= 2;
    if (m_size < 76 || m_size >= sizeof(m_blob)) {
        return false;
    }

    return fromHex(blob, m_size * 2, m_blob);
}


bool Job::setId(const char *id)
{
    if (!id || strlen(id) >= sizeof(m_id)) {
        return false;
    }

    memset(m_id, 0, sizeof(m_id));
    memcpy(m_id, id, strlen(id));
    return true;
}


bool Job::setTarget(const char *target)
{
    if (!target) {
        return false;
    }

    const size_t len = strlen(target);

    if (len <= 8) {
        uint32_t tmp = 0;
        char str[8];
        memcpy(str, target, len);

        if (!fromHex(str, 8, reinterpret_cast<unsigned char*>(&tmp)) || tmp == 0) {
            return false;
        }

        m_target = 0xFFFFFFFFFFFFFFFFULL / (0xFFFFFFFFULL / static_cast<uint64_t>(tmp));
    }
    else if (len <= 16) {
        m_target = 0;
        char str[16];
        memcpy(str, target, len);

        if (!fromHex(str, 16, reinterpret_cast<unsigned char*>(&m_target)) || m_target == 0) {
            return false;
        }
    }
    else {
        return false;
    }


    m_diff = toDiff(m_target);
    return true;
}


bool Job::fromHex(const char* in, unsigned int len, unsigned char* out)
{
    bool error = false;
    for (unsigned int i = 0; i < len; i += 2) {
        out[i / 2] = (hf_hex2bin(in[i], error) << 4) | hf_hex2bin(in[i + 1], error);

        if (error) {
            return false;
        }
    }
    return true;
}


void Job::toHex(const unsigned char* in, unsigned int len, char* out)
{
    for (unsigned int i = 0; i < len; i++) {
        out[i * 2] = hf_bin2hex((in[i] & 0xF0) >> 4);
        out[i * 2 + 1] = hf_bin2hex(in[i] & 0x0F);
    }
}
