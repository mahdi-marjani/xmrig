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

#ifndef XMRIG_API_H
#define XMRIG_API_H


#include "base/kernel/interfaces/IControllerListener.h"


namespace xmrig {


class Controller;
class Httpd;
class HttpRequest;
class String;


class Api : public IControllerListener
{
public:
    Api(Controller *controller);
    ~Api() override;

    inline const char *id() const       { return m_id; }
    inline const char *workerId() const { return m_workerId; }

    void request(const HttpRequest &req);
    void start();
    void stop();

protected:
    void onConfigChanged(Config *config, Config *previousConfig) override;

private:
    void genId(const String &id);
    void genWorkerId(const String &id);

    char m_id[32];
    char m_workerId[128];
    Controller *m_controller;
    Httpd *m_httpd;
};


} // namespace xmrig


#endif /* XMRIG_API_H */
