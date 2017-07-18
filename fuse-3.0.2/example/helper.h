/* void initAddrMap(addrMap map); */

/* void initCache(pageCache cache); */

/* void initFreeLists(freeList lists); */

void initCYCstate(CYCstate state);

void initInode(struct inode ind, mode_t mode, int fileID);

void initLogHeader(struct logHeader logH, unsigned long erases,
		   page_vaddr logId, block_addr first,
		   short logType);

void initOpenFile(openFile oFile, struct activeLog log,
		  struct inode ind, page_vaddr fileID);

int getFreePtr(addrMap map);

int getEraseCount(page_addr k);

void writeCurrLogHeader(openFile oFile);
