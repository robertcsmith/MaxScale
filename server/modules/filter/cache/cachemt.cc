/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl.
 *
 * Change Date: 2019-07-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "cachemt.h"
#include "storage.h"
#include "storagefactory.h"

CacheMT::CacheMT(const char* zName,
                 CACHE_CONFIG& config,
                 CACHE_RULES* pRules,
                 StorageFactory* pFactory,
                 Storage* pStorage,
                 HASHTABLE* pPending)
    : CacheSimple(zName, config, pRules, pFactory, pStorage, pPending)
{
    spinlock_init(&m_lockPending);
}

CacheMT::~CacheMT()
{
}

CacheMT* CacheMT::Create(const char* zName, CACHE_CONFIG& config)
{
    CacheMT* pCache = NULL;

    CACHE_RULES* pRules = NULL;
    HASHTABLE* pPending = NULL;
    StorageFactory* pFactory = NULL;

    if (CacheSimple::Create(config, &pRules, &pFactory, &pPending))
    {
        uint32_t ttl = config.ttl;
        int argc = config.storage_argc;
        char** argv = config.storage_argv;

        Storage* pStorage = pFactory->createStorage(CACHE_THREAD_MODEL_MT, zName, ttl, argc, argv);

        if (pStorage)
        {
            CPP_GUARD(pCache = new CacheMT(zName,
                                           config,
                                           pRules,
                                           pFactory,
                                           pStorage,
                                           pPending));

            if (!pCache)
            {
                cache_rules_free(pRules);
                hashtable_free(pPending);
                delete pStorage;
                delete pFactory;
            }
        }
    }

    return pCache;
}

bool CacheMT::mustRefresh(const CACHE_KEY& key, const SessionCache* pSessionCache)
{
    long k = hashOfKey(key);

    spinlock_acquire(&m_lockPending);
    bool rv = CacheSimple::mustRefresh(k, pSessionCache);
    spinlock_release(&m_lockPending);

    return rv;
}

void CacheMT::refreshed(const CACHE_KEY& key,  const SessionCache* pSessionCache)
{
    long k = hashOfKey(key);

    spinlock_acquire(&m_lockPending);
    CacheSimple::refreshed(k, pSessionCache);
    spinlock_release(&m_lockPending);
}
