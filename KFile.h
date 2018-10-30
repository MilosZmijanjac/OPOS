#ifndef KFILE_H
#define KFILE_H
#include "KernelFS.h"
#include "fs.h"
#include "PartitionData.h"
class PartitionData;
class KernelFile {
    friend class File;
    friend class FS;
private:
	char write(BytesCnt, char* buffer);//upis na tekuću poziciju u fajl 
	BytesCnt read(BytesCnt, char* buffer);//čitanje sa tekuće pozicije iz fajla
	char seek(BytesCnt);//pomjeranje tekuće pozicije u fajlu
	char eof();//provjera da li je tekuća pozicija u fajlu kraj tog fajla.
	char truncate();//brisanje dijela fajla od tekuće pozicije do kraja
public:
	unsigned long filePosition;//trenutna pozicija
	PartitionData *pd;//pokazivac na podatke particije
	ClusterNo entryDataCluster;//klaster za podatke ulaza
	EntryNum entryDataClusterPos;//pozicija klastera za podatk ulaza
	Entry entry;//ulaz
    KernelFile();//konstruktor
    ~KernelFile();//destruktor
};
#endif