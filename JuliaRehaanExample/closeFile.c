/*void closeFile(const char *path, int fd){
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	openFile file = state->openFileTable[fd];
	if (file->inode->i_links_count = 0){
		//Just call unlink function
		res = unlink(path);
		if (res == -1)
			return -errno;
		state->addrMap->map[file->inode->i_file_no] = 0;

	}
	else{
		activeLog activeLog = state->openFileTable[i]->mainExtentLog;
		fullPage newPage = newFullPage(PTYPE_INODE);
		newPage->contents = (char*) log;
		//Then nested if statements to fill in fields
		int overflow = activeLog->nextPage%BLOCKSIZE;
		//First Page
		if (overflow == 0){
			newPage->eraseCount = 0;
		}
		//Last and next to last page
		else if (overflow == BLOCKSIZE - 1 || overflow == BLOCKSIZE - 2){
			block_addr nextLogBlock = activeLog->last;
			int nextBlockErases = 0;
		}
		//Write new page
		xmp_write(state->storePath, (char *) newPage, PAGESIZE, activeLog->nextPage*sizeof(struct fullPage), state->storePathDescriptor);
	}
	state->openFileTable[fd] = 0;
	}*/
