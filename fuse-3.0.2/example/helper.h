// TODO: Comment these

void initCYCstate(CYCstate state);

void initInode(struct inode *ind, mode_t mode,
	       page_vaddr fileID, page_vaddr logID);

void initLogHeader(struct logHeader *logH,
		   page_vaddr logId, short logType);

void initActiveLog(struct activeLog *theLog, page_addr page);

void initOpenFile(openFile oFile, struct activeLog *log,
		  struct inode *ind, page_vaddr fileID);
writeablePage getFreePage(freeList lists, struct activeLog *log);

void getPageFields(page_addr pageAddr, int *eraseCount,
		   page_addr *nextLogBlock, int *nextBlockErases);

int getFreePtr(addrMap map);

int getFreeBlock(freeList lists, page_addr *freePage, int newFile);

activeLog getLogForFile(CYCstate state, page_vaddr fileID,page_addr *logHeaderPage, mode_t mode);

page_vaddr readStubFile(char *fullPath);

void writeCurrLogHeader(openFile oFile, freeList lists);

