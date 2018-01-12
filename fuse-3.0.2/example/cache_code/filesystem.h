// Joseph Oh

cacheEntry fs_getPage(pageKey desiredKey);

void fs_updateParentPage(pageKey childKey, page_addr childAddress, int dirty);

writeablePage fs_writeData(pageKey dataKey, char* data);

void fs_updateFileInLru(addressCache cache, openFile file);

void fs_flushMetadataPages(addressCache cache, openFile file);

void fs_flushDataPages(addressCache cache, openFile file);

void fs_removeFileFromLru(addressCache cache, openFile file);
