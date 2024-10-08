/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "random.h"

#include <random>

#include <QtLogging>
#include <QtSystemDetection>

#if defined(Q_OS_WIN)
#include <windows.h>
#include "base/global.h"
#include "base/utils/os.h"
#elif defined(Q_OS_LINUX)
#include <cerrno>
#include <cstring>
#include <sys/random.h>
#else
#include <cerrno>
#include <cstdio>
#include <cstring>
#endif

namespace
{
#if defined(Q_OS_WIN)
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
            : m_processPrng {Utils::OS::loadWinAPI<PPROCESSPRNG>(u"BCryptPrimitives.dll"_s, "ProcessPrng")}
        {
            if (!m_processPrng)
                qFatal("Failed to load ProcessPrng().");
        }

        static constexpr result_type min()
        {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max()
        {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()()
        {
            result_type buf = 0;
            const bool result = m_processPrng(reinterpret_cast<PBYTE>(&buf), sizeof(buf));
            if (!result)
                qFatal("ProcessPrng() failed.");

            return buf;
        }

    private:
        using PPROCESSPRNG = BOOL (WINAPI *)(PBYTE, SIZE_T);
        const PPROCESSPRNG m_processPrng;
    };
#elif defined(Q_OS_LINUX)
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
        {
        }

        static constexpr result_type min()
        {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max()
        {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()()
        {
            const int RETRY_MAX = 3;

            for (int i = 0; i < RETRY_MAX; ++i)
            {
                result_type buf = 0;
                const ssize_t result = ::getrandom(&buf, sizeof(buf), 0);
                if (result == sizeof(buf))  // success
                    return buf;

                if (result < 0)
                    qFatal("getrandom() error. Reason: %s. Error code: %d.", std::strerror(errno), errno);
            }

            qFatal("getrandom() failed. Reason: too many retries.");
        }
    };
#else
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
            : m_randDev {fopen("/dev/urandom", "rb")}
        {
            if (!m_randDev)
                qFatal("Failed to open /dev/urandom. Reason: %s. Error code: %d.", std::strerror(errno), errno);
        }

        ~RandomLayer()
        {
            fclose(m_randDev);
        }

        static constexpr result_type min()
        {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max()
        {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()() const
        {
            result_type buf = 0;
            if (fread(&buf, sizeof(buf), 1, m_randDev) != 1)
                qFatal("Read /dev/urandom error. Reason: %s. Error code: %d.", std::strerror(errno), errno);

            return buf;
        }

    private:
        FILE *m_randDev = nullptr;
    };
#endif
}

uint32_t Utils::Random::rand(const uint32_t min, const uint32_t max)
{
    static RandomLayer layer;

    // new distribution is cheap: https://stackoverflow.com/a/19036349
    std::uniform_int_distribution<uint32_t> uniform(min, max);

    return uniform(layer);
}
