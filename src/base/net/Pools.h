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

#ifndef XMRIG_POOLS_H
#define XMRIG_POOLS_H


#include <vector>


#include "base/net/Pool.h"


namespace xmrig {


class Pools
{
public:
    Pools();

    inline bool setUserpass(const char *userpass)       { return current().setUserpass(userpass); }
    inline const std::vector<Pool> &data() const        { return m_data; }
    inline void setFingerprint(const char *fingerprint) { current().setFingerprint(fingerprint); }
    inline void setKeepAlive(bool enable)               { setKeepAlive(enable ? Pool::kKeepAliveTimeout : 0); }
    inline void setKeepAlive(int keepAlive)             { current().setKeepAlive(keepAlive); }
    inline void setNicehash(bool enable)                { current().setNicehash(enable); }
    inline void setPassword(const char *password)       { current().setPassword(password); }
    inline void setRigId(const char *rigId)             { current().setRigId(rigId); }
    inline void setTLS(bool enable)                     { current().setTLS(enable); }
    inline void setUser(const char *user)               { current().setUser(user); }
    inline void setVariant(const char *variant)         { current().algorithm().parseVariant(variant); }
    inline void setVariant(int variant)                 { current().algorithm().parseVariant(variant); }

    bool setUrl(const char *url);
    size_t active() const;
    void adjust(const Algorithm &algorithm);

private:
    Pool &current();

    std::vector<Pool> m_data;
};


} /* namespace xmrig */


#endif /* XMRIG_POOLS_H */
