// File: fs.h
#ifndef FS_H
#define FS_H
typedef unsigned long BytesCnt;//broj bajtova
typedef unsigned long EntryNum;//broj ulaza
const unsigned long ENTRYCNT = 64;//broj ulaza
const unsigned int FNAMELEN = 8;//duzina naziva fajla
const unsigned int FEXTLEN = 3;//duzina ekstenzije
struct Entry {//ulaz
	char name[FNAMELEN];//naziv ulaza-fajla
	char ext[FEXTLEN];//ekstenzija ulaza-fajla
	char reserved;//rezervisani bajt koji se ne koristi ima vrijednost 0
	unsigned long indexCluster;//klaster koji sadrži indeks prvog nivoa za dati fajl
	unsigned long size;//veličina fajla u bajtovima
};
typedef Entry Directory[ENTRYCNT];//direktorijumi
#include "part.h"
#include "file.h"

#include "List.h"
class KernelFS;//klasa KernelFS
#include "KernelFS.h"
class Partition;//klasa Partition
class File;//klasa File
class FS {
public:
	~FS();
	static char mount(Partition* partition); //montira particiju
											 // vraca dodjeljeno slovo
	static char unmount(char part); //demontira particiju oznacenu datim
									// slovom vraca 0 u slucaju neuspeha ili 1 u slucaju uspjeha
	static char format(char part); //particija zadata slovom se formatira;
								   // vraca 0 u slucaju neusjpeha ili 1 u slucaju uspjeha
	static char readRootDir(char part, EntryNum n, Directory &d);
	//prvim argumentom se zadaje particija, drugim redni broj
	//validnog ulaza od kog se pocinje citanje
	static char doesExist(char* fname); //argument je naziv fajla sa
										//apsolutnom putanjom
	static File* open(char* fname, char mode);//otvara fajl
	static char deleteFile(char* fname);//brise fajl
protected:
	FS();//konstruktor
	static KernelFS *myImpl;
};
#endif