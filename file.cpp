#include "file.h"


File::File() 
{
    myImpl = new KernelFile();
}

File::~File() //Zatvara fajl
{
    delete myImpl;
}

char File::write(BytesCnt n, char * buffer)
{
	return myImpl->write(n, buffer);
}

BytesCnt File::read(BytesCnt max, char * buffer)
{
	return myImpl->read(max, buffer);
}

char File::seek(BytesCnt value)
{
	return myImpl->seek(value);
}

BytesCnt File::filePos()
{
    return myImpl->filePosition;
}

char File::eof()
{
	return myImpl->eof();
}

BytesCnt File::getFileSize()
{
    return myImpl->entry.size;
}

char File::truncate()
{
	return myImpl->truncate();
}
