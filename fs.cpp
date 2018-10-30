#include "fs.h"
#include <iostream>
#include <algorithm>
#include <experimental\filesystem>

KernelFS* FS::myImpl = new KernelFS();

FS::~FS()
{
    delete myImpl;
}

char FS::mount(Partition * partition)
{
	return myImpl->mount(partition);
}

char FS::unmount(char part)
{
	return myImpl->unmount(part);
}

char FS::format(char part)
{
	return myImpl->format(part);
}

char FS::readRootDir(char part, EntryNum n, Directory & d)
{
	return myImpl->readRootDir(part, n, d);
}

char FS::doesExist(char * fname)
{
	return myImpl->doesExist(fname);
}

File * FS::open(char * fname, char mode)
{
	return myImpl->open(fname, mode);
}

char FS::deleteFile(char * fname)
{
	return myImpl->deleteFile(fname);
}

FS::FS(){}
