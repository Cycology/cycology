// Joseph Oh

cacheEntry fs_getPage(addressCache cache, pageKey desiredKey);

void fs_updateParentPage(addressCache cache, pageKey childKey, page_addr childAddress, int dirty);

writeablePage fs_writeData(addressCache cache, pageKey dataKey, char* data);


void fs_updateFileInLru(openFile file);

void fs_removeFileFromLru(CYCstate state, openFile file);


void fs_flushMetadataPages(addressCache cache, openFile file);

void fs_flushDataPages(addressCache cache, openFile file);

void fs_closeFile(CYCstate state, openFile file);

void fs_evictLruFile(CYCstate state);
