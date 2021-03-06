#include <basicFile.h>
#include <file.h>
#include <memory.h>
#include <stdlib.h>
#include <stddef.h>
#include <kernel-lib.h>

// #pragma pack(push)
// #pragma pack (1) 				//Align structure to 1 byte (commented)
struct basicFile_s {
	char name[MAX_NAME+1];			//File name
	void *stream;					//Where data is actually read from/written to
	uint64_t size;					//Highest valid offset from <stream>
	Place place;					//Where the File resides - either a memory page, or a kernel buffer (e.g. video buffer)
	uint64_t writeIndex;			//Next position to put data into
	uint64_t readIndex;				//Next position from which to read data
};
// #pragma pack(pop)


/* Static variables */
static uint64_t maxFiles = PAGE_SIZE / sizeof(struct basicFile_s); /* Should be...idk I didn't wanna do the math */
// static void *filesTablePage;
static struct basicFile_s *filesTable = NULL; 		//"Table" (array) which keeps track of open files
static uint64_t numFiles = 0;						//Number of open files



/* Static function prototypes */

/**
* Ensures the specified parameters are valid for creating a new File.
*
* @return 1 if valid, 0 if not (e.g. name too short/long, invalid place, etc.)
*/
static int8_t validateParams(const char* name, void *stream, uint64_t size, Place place);

/**
* Checks whether the specified File actually resides within the File table.
*
* @return 1 if valid, 0 if not (e.g. outside of files table memory range)
*/
static int8_t isValidFile(BasicFile file);

/**
* Finds the index of the specified file in the Files table.
*
* @return The found index, or -1 if not found.
*/
static int64_t indexOfFile(BasicFile file);


/**
 * Initializes the "file system."
 * 
 * @return 0 on success, -1 otherwise
 */
uint64_t initializeBasicFiles() {
	void *filesTablePage;
	pageManager(POP_PAGE, &filesTablePage);
	if (filesTablePage == NULL) {
		return -1; /* Couldn't start file system */
	}
	memset(filesTablePage, 0, PAGE_SIZE);	//Clear the page
	filesTable = (struct basicFile_s *)filesTablePage;
	return 0;
}

BasicFile createBasicFileWithName(const char* name) {
	if(name == NULL || name[0] == 0 || strlen(name) > MAX_NAME) {
		return NULL;
	}
	void *stream;
	BasicFile newFile;
	pageManager(POP_PAGE, &stream);
	if (stream == NULL) {
		return NULL;
	}
	newFile = createBasicFile(name, stream, PAGE_SIZE, HEAP);
	if (newFile == NULL) {	//Failed, put the page back as available
		pageManager(PUSH_PAGE, &stream);
	}
	return newFile;
}

BasicFile createBasicFile(const char* name, void *stream, uint64_t size, Place place) {
	if (numFiles >= maxFiles) {
		return NULL;
	}
	if (!validateParams(name, stream, size, place)) {
		return NULL;
	}

	/* Find the next available entry in the Files table - guaranteed to exist at this point */
	uint64_t index = 0;
	while(filesTable[index].name[0] != 0) {
		index++;
	}
	
	memcpy((void *) filesTable[index].name, name, strlen(name)+1);
	filesTable[index].size = size;
	filesTable[index].stream = stream;
	filesTable[index].place = place;
	filesTable[index].writeIndex = 0;
	filesTable[index].readIndex = 0;
	numFiles++;
	return &filesTable[index];
}

BasicFile findBasicFile(const char* name) {
	uint64_t index = 0;
	while (index < maxFiles && strcmp((filesTable[index]).name, name)){
		index++;
	}
	if (index >= maxFiles) {
		return NULL;
	}
	return &filesTable[index];
}

int basicFileReadChar(BasicFile file) {
	if (!isValidFile(file) || basicFileIsEmpty(file)) {
		return EOF;
	}

	char *stream = (char *)file->stream;
	int c = stream[file->readIndex];
	//Advance read index, wrapping around the end
	if(file->readIndex >= file->size-1) {
		file->readIndex = 0;
	}
	else {
		file->readIndex++;
	}
	return c;
}


int basicFileWriteChar(char c, BasicFile file) {
	if (!isValidFile(file) || basicFileIsFull(file)) {
		return EOF;
	}
	char *stream = (char *)file->stream;
	stream[file->writeIndex] = c;
	//Advance write index, wrapping around the end
	if(file->writeIndex >= file->size-1) {
		file->writeIndex = 0;
	}
	else {
		file->writeIndex++;
	}
	return 1;
}

int basicFileIsFull(BasicFile file) {
	//FIXME this returns true when writeIndex == file->size && readIndex == 0-- but when that happens, the write hasn't actually been performed. So 1 byte is wasted
	return file->writeIndex == file->readIndex-1
			|| (file->writeIndex >= file->size-1 && file->readIndex == 0);
}

int basicFileIsEmpty(BasicFile file) {
	return file->writeIndex == file->readIndex;
}

int destroyBasicFile(BasicFile file) {
	if (!isValidFile(file)) { /* Must check file pointer in order to avoid buffer overflows */
		return -1;
	}
	uint64_t index = indexOfFile(file);
	if(index == -1) {
		return -1;
	}
	if (file->place == HEAP) {
		//Reclaim the heap page - this shouldn't cause an error
		pageManager(PUSH_PAGE, &(file->stream));
	}
	memset(&filesTable[index], 0, sizeof(struct basicFile_s));
	numFiles--;
	return 0;
}

char* getBasicFileName(BasicFile f) {
	return f->name;
}

uint64_t getBasicFileSize(BasicFile f) {
	return f->size;
}

uint64_t getBasicFileFreeSpace(BasicFile f) {
	return (f->writeIndex >= f->readIndex)
                ? f->size - f->writeIndex
                : f->readIndex - f->writeIndex;
}

static int8_t validateParams(const char* name, void *memory, uint64_t size, Place place) {
	if(name[0] == 0 || strlen(name) >= MAX_NAME) {
		return 0;	//Name too long or too short
	}
	if(memory == NULL || size <= 0 || (place != KERNEL && place != HEAP)) {
		return 0;	//Invalid values
	}
	if(findBasicFile(name) != NULL) {
		return 0;	//File already exists
	}
	return 1;
}

static int8_t isValidFile(BasicFile file) {
	if (file == NULL) {
		return 0;
	}
	if ( ((void *)file < (void *)filesTable) ||  ((void *)file >= ( ((void *)filesTable) + PAGE_SIZE)) ) {	//FIXME this is not reliable, if files table becomes a different size this will fail
		return 0;	//Out of table range
	}
	uint64_t entry = (uint64_t) ((void *)file - (void *)filesTable);
	if (entry % sizeof(struct basicFile_s)) {
		return 0;	//Not a valid entry (not aligned)
	}
	return 1;
}

static int64_t indexOfFile(BasicFile file) {
	for(uint64_t i = 0, checkedFiles = 0; checkedFiles < numFiles && i < maxFiles; i++) {
		if(filesTable[i].name != NULL) {
			if(streql(file->name, filesTable[i].name)) {
				return (int64_t) i;
			}
			checkedFiles++;
		}
	}
	return -1;
}
