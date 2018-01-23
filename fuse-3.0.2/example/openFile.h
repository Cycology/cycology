// Joseph Oh


void openFile_addDataPage(openFile file, cacheEntry entry);

void openFile_removeDataPage(openFile file, cacheEntry entry);

void openFile_flushDataPages(openFile file, addressCache cache);

void openFile_addMetadataPage(openFile file, cacheEntry entry);

void openFile_removeMetadataPage(openFile file, cacheEntry entry);

void openFile_printData(openFile file);
