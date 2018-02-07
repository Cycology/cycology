// Tom Murtagh
// Joseph Oh


bool isInode(desiredKey) {
  return (desiredKey->levelsAbove == desiredKey->file->inode->treeHeight);
}

/* Get the physical address of the given data page from the parent */
int getAddressFromParent(cachedPage parent, pageKey desiredKey) {
  int parentStartOffset = getStartOfRange(desiredKey);
  int pages_per_pointer = getNumPagesPerPointerAtLevel(desiredKey->file,
						       desiredKey->levelsAbove + 1);
  int pointer_index = (desiredKey->dataOffset - parentStartOffset) / pages_per_pointer;

  if (key->levelsAbove == file.tree_height) {
    // Parent is the inode
    return parent->directPage[pointer_index];
  } else {
    // Parent is an indirect page
    return parent->contents[pointer_index];
  }
}

/* Creates and adds a blank indirect page to the cache */
cachedPage createPageInCache(page_addr address, pageKey key) {
  cachedPage page = initCachedPage(page);
  cache_set(key, page);
  return page;
}

cachedPage readIntoCache(page_addr address, pageKey key) {
  cachedPage desired;
  // Evict page if necessary
  if (cache-size >= CACHE_SIZE) {
    cache_evict(cache);
  }

  // Put the page into the cache
  if (address == 0) {
    desired = createPageInCache(address, key);
  } else {
    // Read in from NAND and add to cache
    if (readNAND(&desired->dataPage, address) != 1) {
      return NULL;
    }
    cache_set(cache, key, desired);
  }

  return desired;
}

cachedPage getPage(pageKey desiredKey) {
  if (isInode(desiredKey)) {
    return desiredKey->file->inodeCached;
  } else {
    cachedPage desired = checkCache(desiredKey);

    if (desired == null) {
      pageKey parentKey(desiredKey->file, desiredKey->dataOffset, desiredKey->levelsAbove + 1);
      cachedPage parent = getPage(parentKey);

      page_addr desiredAddr = getAddressFromParent(parent, desiredKey);
      desired = readIntoCache(desiredAddr, desiredKey);
    }

    return desired;
  }
}

/* Returns the address of the first free page in NAND memory */
cachedPage allocateFreePage() {
  memcpy( dest->page.contents, data, sizeof( dest->page.contents ) );  
}

cachedPage writeData( openFile file, int dataOffset, char * data ) {
  // Write the data into a free page
  // TODO: Shouldn't we write the data to NAND first before updating metadata? 
  writeablePage dest = allocateFreePage( &state->freelists, file->mainExtentLog );
  writeNAND( & dest->page, dest->address, 0 );
  addToCache( file, dataOffset, 0, dest );

  // Update indirect pointer 1 level above data
  cachedPage indirectPage = getPage( file, dataOffset, 1 );
  updateIndirectPointer( indirectPage, file, dataOffset, dest->address );
}


/* TRADITIONAL UNBALANCED INDIRECT TREE APPROACH */
/* Given a page address, a open file, and a specific number of levels
   above the data page in the tree of indirect pages, this function's
   purpose is to calculate the address of the first data page associated
   with the indirect block that is the requested number of levels above
   the dataOffset value. This, together with the file number and a levelsAbove
   value of 0, should serve as a unique id for the indirect page within
   the file system cache.
*/
int getStartOfRange( openFile file, int dataOffset, int levelsAbove ) {
  if ( levelsAbove = 0 ) {
    return dataOffset;
  } else {
    int levelsToGo;
    if ( dataOffset < DIRECT_POINTERS*PAGESIZE ) {
      levelsToGo = 1 - levelsAbove;
      return 0;
    } else {
      int adjustedOffset = dataOffset - DIRECT_POINTERS*PAGESIZE;
      int sizeOfLevel = PAGESIZE/PAGEADRSIZE*PAGESIZE;
      levelsToGo = 1 - levelsAbove;
      while ( adjustedOffset >= sizeOfLevel ) {
	adjustedOffset = adjustedOffset - sizeOfLevel;
	sizeOfLevel = sizeOfLevel * PAGESIZE/PAGEADRSIZE;
	levelsToGo++;
      }

      int unitSize = sizeOfLevel;
      for ( int i = levelsToGo; i > 0; i-- ) {
	unitSize = unitSize /( PAGESIZE/PAGEADRSIZE);
      }
      return dataOffset/unitSize * unitSize;
    }
  }
}


/* Deprecated */
cachedPage getPage( openFile file, int dataOffset, int levelsAbove ) {
  if ( levelsAbove == maxLevels( file, dataOffset )) {
    return file->inodeCached;
  } else {
    cachedPage desired = checkHash( file, dataOffset, levelsAbove );

    if ( desired == null ) {

      cachedPage parent = getPage( file, dataOffset, levelAbove + 1);
      pageAddr add = extractAdr( parent, file, dataOffset, levelsAbove );

      if ( add != 0 ) {
	desired = readIntoCache( adr, file, dataOffset, levelsAbove );
      } else {
	desired = allocateIndirectPage( adr, file, dataOffset, levelsAbove );
	memset( &desired->content, 0, sizeof( desired->content) );
      }
    }

    return desired;
  }
}



	   


