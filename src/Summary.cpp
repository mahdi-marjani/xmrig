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


#include <uv.h>


#include "Console.h"
#include "Cpu.h"
#include "Mem.h"
#include "net/Url.h"
#include "Options.h"
#include "Summary.h"
#include "version.h"


static void print_versions()
{
    char buf[16];

#   if defined(__clang__)
    snprintf(buf, 16, " clang/%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#   elif defined(__GNUC__)
    snprintf(buf, 16, " gcc/%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#   elif defined(_MSC_VER)
    snprintf(buf, 16, " MSVC/%d", MSVC_VERSION);
#   else
    buf[0] = '\0';
#   endif


    if (Options::i()->colors()) {
        Console::i()->text("\x1B[01;32m * \x1B[01;37mVERSIONS:     \x1B[01;36mXMRig/%s\x1B[01;37m libuv/%s%s", APP_VERSION, uv_version_string(), buf);
    } else {
        Console::i()->text(" * VERSIONS:     XMRig/%s libuv/%s%s", APP_VERSION, uv_version_string(), buf);
    }
}


static void print_memory() {
    if (Options::i()->colors()) {
        Console::i()->text("\x1B[01;32m * \x1B[01;37mHUGE PAGES:   %s, %s",
                           Mem::isHugepagesAvailable() ? "\x1B[01;32mavailable" : "\x1B[01;31munavailable",
                           Mem::isHugepagesEnabled() ? "\x1B[01;32menabled" : "\x1B[01;31mdisabled");
    }
    else {
        Console::i()->text(" * HUGE PAGES:   %s, %s", Mem::isHugepagesAvailable() ? "available" : "unavailable", Mem::isHugepagesEnabled() ? "enabled" : "disabled");
    }
}


static void print_cpu()
{
    if (Options::i()->colors()) {
        Console::i()->text("\x1B[01;32m * \x1B[01;37mCPU:          %s (%d) %sx64 %sAES-NI",
                           Cpu::brand(),
                           Cpu::sockets(),
                           Cpu::isX64() ? "\x1B[01;32m" : "\x1B[01;31m-",
                           Cpu::hasAES() ? "\x1B[01;32m" : "\x1B[01;31m-");
#       ifndef XMRIG_NO_LIBCPUID
        Console::i()->text("\x1B[01;32m * \x1B[01;37mCPU L2/L3:    %.1f MB/%.1f MB", Cpu::l2() / 1024.0, Cpu::l3() / 1024.0);
#       endif
    }
    else {
        Console::i()->text(" * CPU:          %s (%d) %sx64 %sAES-NI", Cpu::brand(), Cpu::sockets(), Cpu::isX64() ? "" : "-", Cpu::hasAES() ? "" : "-");
#       ifndef XMRIG_NO_LIBCPUID
        Console::i()->text(" * CPU L2/L3:    %.1f MB/%.1f MB", Cpu::l2() / 1024.0, Cpu::l3() / 1024.0);
#       endif
    }
}


static void print_threads()
{
    char buf[32];
    if (Options::i()->affinity() != -1L) {
        snprintf(buf, 32, ", affinity=0x%llX", Options::i()->affinity());
    }
    else {
        buf[0] = '\0';
    }

    Console::i()->text(Options::i()->colors() ? "\x1B[01;32m * \x1B[01;37mTHREADS:      \x1B[01;36m%d\x1B[01;37m, %s, av=%d, donate=%d%%%s%s" : " * THREADS:      %d, %s, av=%d, donate=%d%%%s%s",
                       Options::i()->threads(),
                       Options::i()->algoName(),
                       Options::i()->algoVariant(),
                       Options::i()->donateLevel(),
                       Options::i()->nicehash() ? ", nicehash" : "", buf);
}


static void print_pools()
{
    Console::i()->text(Options::i()->colors() ? "\x1B[01;32m * \x1B[01;37mPOOL #1:      \x1B[01;36m%s:%d" : " * POOL #1:      %s:%d",
                       Options::i()->url()->host(),
                       Options::i()->url()->port());

    if (!Options::i()->backupUrl()) {
        return;
    }

    Console::i()->text(Options::i()->colors() ? "\x1B[01;32m * \x1B[01;37mPOOL #2:      \x1B[01;36m%s:%d" : " * POOL #2:      %s:%d",
                       Options::i()->backupUrl()->host(),
                       Options::i()->backupUrl()->port());
}


void Summary::print()
{
    print_versions();
    print_memory();
    print_cpu();
    print_threads();
    print_pools();
}



