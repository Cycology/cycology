// Joseph Oh

addressCache cache_create(int size);

int cache_hash(addressCache cache, pageKey page_key);

cacheEntry cache_set(addressCache cache, pageKey key, writeablePage wp);

cacheEntry cache_get(addressCache cache, pageKey key);

void cache_updateDataLruHead(addressCache cache, cacheEntry entry);

void cache_addDataPageToLru(addressCache cache, cacheEntry entry);

cacheEntry cache_removeDataPageFromLru(addressCache cache, cacheEntry current);

void cache_remove(addressCache cache, cacheEntry entry);

void cache_printCacheRecord();
