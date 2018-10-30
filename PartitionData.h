#ifndef PARTITIONDATA_H
#define PARTITIONDATA_H
#include "fs.h"
#include "List.h"
#include "MyMutex.h"
class PartitionData {
	friend class FS;
	friend class KernelFS;
	friend class File;
	friend class KernelFile;
private:
	Partition *part = nullptr;

	int filesOpen = 0;
	bool formattingRequest = false;
	bool formatting = false;

	List openForReading, openForWriting;

	MyMutex diskRWMutex;
	MyMutex listsMutex;
	MyMutex filesOpenMutex;


	unsigned long threadsReadingRootDir = 0;

public:
	PartitionData(Partition* p) :part(p) { }
	~PartitionData() { }

	ClusterNo findFreeBit();
	
	char setBit(ClusterNo bit, char* buffer); 
	char resetBit(ClusterNo bit, char* buffer);
	char _doesExist(char * fname, Entry & entry, ClusterNo & entryCluster, EntryNum & entryPos);
};
#endif PARTITIONDATA_H