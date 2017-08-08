
void initCYCstate(CYCstate state);

void initInode(struct inode *ind, mode_t mode,
	       page_vaddr fileID, page_vaddr logID);

void initLogHeader(struct logHeader *logH, unsigned long erases,
		   page_vaddr logId, short logType);

void initActiveLog(struct activeLog *theLog, page_addr page,
		   block_addr block, struct logHeader logH);

void initOpenFile(openFile oFile, struct activeLog *log,
		  struct inode *ind, page_vaddr fileID);

page_addr getNextPageAddr(page_addr curPage, struct activeLog *log);

void getPageFields(page_addr pageAddr, int *eraseCount,
		   page_addr *nextLogBlock, int *nextBlockErases);

int getFreePtr(addrMap map);

writeablePage getFreeBlock(freeList lists, page_addr *freePage, int newFile,
			   logHeader logH, activeLog log);

activeLog getLogForFile(CYCstate state, page_vaddr fileID, struct inode ind,
			mode_t mode,page_addr *logHeaderPage);

page_vaddr readStubFile(char *fullPath);

void writeCurrLogHeader(openFile oFile);

