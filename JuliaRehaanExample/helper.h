//Helper functions:

//From fetchInode.c

page_vaddr getVirtualAddress(const char *path);

struct inode* getInode(page_vaddr vAddr);

logHeader getLog(page_vaddr vaddr);

fullPage getFullPage(page_vaddr vaddr);

//From inode.c

struct inode* newInode(page_vaddr i_file_no, page_vaddr i_log_no, mode_t mode);

logHeader newLog(short logType);

fullPage newFullPage(short pageType);

//From activeLog.c

activeLog newActiveLog(fullPage logPage);

//From addressMap.c

addrMap initAddrMap();

page_addr getAvailableAddress();

//From openFile.c

*(openFile[]) initOpenTable(int size);

int addOpenFile(page_vaddr vaddr);

int isOpen(page_vaddr vaddr);

void closeFile(const char *path, int fd);
