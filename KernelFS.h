#ifndef KERNELFS_H
#define KERNELFS_H
#include <iostream>
#include "MyMutex.h"
#include <Windows.h>
#include "part.h"
#include "fs.h"
#include "file.h"
#include <cstring>
#include "List.h"
#include "PartitionData.h"

const Entry emptyEntry = { "","",0,0,0 };// prazni ulaz
class List;//klasa List
class PartitionData;//klasa PartitionData

const ClusterNo sizeOfIndex = ClusterSize / sizeof(ClusterNo);//velicina indeksa
typedef ClusterNo Index[sizeOfIndex];//indeksi
typedef Entry RootData[ClusterSize / sizeof(Entry)];//korijeni

class KernelFS {
    friend class FS;//prijateljska klasa FS
private:
    PartitionData* partitions[26];//niz od 26 particija
    MyMutex partitionsMutex;//kriticna sekcija particije
	char mount(Partition* partition); //montira particiju
	char unmount(char part);//demontira particiju
	char format(char part);//formatira particiju
	char readRootDir(char part, EntryNum n, Directory &d);//cita sadrzaj korijenskog direktorijuma
	char doesExist(char*fname); //ispituje postojanje fajla
	class File *open(char*fname, char mode);//otvara fajl
	char deleteFile(char* fname);//brise fajl
public:
    KernelFS();//konstruktor
    ~KernelFS();//destruktor
	int freePartition();//vraca slobodnu particiju
    PartitionData* getPartDataPointer(char c) { return partitions[c - 'A']; }//vraca particiju iz niza unosenjem slova particije
    char setBit(char part, ClusterNo bit, char* buffer); //buffer - niz bajtova koji se menja
    char resetBit(char part, ClusterNo bit, char* buffer); //static ili r/w u funkciju?
    ClusterNo findFreeBit(char part);//pronalazi slobodni bit
};
//pomocne funkcije za rad sa strukturom Entry
char copyEntry(Entry & to, Entry & from);//kopira ulaze
int operator==(const Entry & e1, const Entry & e2);//operator jednakosti dva ulaza
int sameFile(const Entry & e1, const Entry & e2);//provjera jednakosti dva fajla
Entry makeSearchEntry(char* fname);//formira ulaz na osnovu imena
#endif