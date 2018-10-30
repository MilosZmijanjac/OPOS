#include "KFile.h"


char KernelFile::write(BytesCnt n, char * buffer)
{
	if (n == 0) //ako upisuje nula bajtova
		return 1;

	/*Ulaz*/pd->listsMutex.Acquire();//lock list

	if (pd->openForReading.doesExist(entry)) //ako je fajl otvoren u READ modu
	{
		/*Izlaz*/pd->listsMutex.Release();//unlock list
		return 0;
	}

	/*Izlaz*/pd->listsMutex.Release();//unlock list

	BytesCnt bytesWritten = 0;//broj upisanih bajtova
	Index index;//indeks
	char cluster0[ClusterSize];//pomocni nulti klaster-2KB

	if (entry.size == 0)//ako je velicina ulaza nula
	{
		/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

		ClusterNo cluster = pd->findFreeBit();//pronadji slobodan bit
		if (cluster == ~0) //Ako nema slobodnih klastera
		{
			pd->diskRWMutex.Release();//unlock diskRW
			return 0;
		}

		pd->part->readCluster(0, cluster0);//procitaj nulti klaster
		pd->setBit(cluster, cluster0);//setuj bite klastera
		pd->part->writeCluster(0, cluster0);//upisi klaster

		/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

		entry.indexCluster = cluster;//setuj index klastera ulaza
	}
	pd->part->readCluster(entry.indexCluster, (char*)index);//procitaj indeks klaster

	char data[ClusterSize];//pomocni klaster za podatke-2KB
	ClusterNo cluster = ~0;

	while (bytesWritten < n)//dok se n bajtova ne upisu
	{
		if (filePosition == entry.size) //ako je pozicija fajla jednaka velicini ulaza
		{ 
			if (entry.size%ClusterSize == 0) //ako velicina ulaza umnozak velicine klastera
			{
				/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

				ClusterNo x = entry.size / ClusterSize;//setuje se broj-indeks klastera
				if (x < sizeOfIndex / 2) //ako je u prvoj polovini
				{
					ClusterNo y = pd->findFreeBit();//pronadji slobodan bit
					if (y == ~0) //ako nema slobonih bitova
					{
						/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW
						return 0;
					}
					pd->part->readCluster(0, cluster0);//procitaj nulti klaster
					pd->setBit(y, cluster0);//setuj bite nultog klastera
					pd->part->writeCluster(0, cluster0);//upisi nulti klaster
					index[x] = y;//setuj vrijednost indeksa
					pd->part->writeCluster(entry.indexCluster, (char*)index);//upisi klaster
				}
				else if (x < sizeOfIndex / 2 + sizeOfIndex / 2 * sizeOfIndex)//ako je u drugoj polovini
				{
					ClusterNo z = pd->findFreeBit();//pronadji slobodni bit
					if (z == ~0)//ako nema slobodnog bita
					{
						/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW
						return 0;
					}
					pd->part->readCluster(0, cluster0);//procitaj nulti klaster
					pd->setBit(z, cluster0);//setuj bite nultog klastera
					pd->part->writeCluster(0, cluster0);//upisi nulti klaster
					ClusterNo level1pos = sizeOfIndex / 2 + (x - sizeOfIndex / 2) / sizeOfIndex;//pozicija prvog nivoa
					ClusterNo level2pos = (x - sizeOfIndex / 2) % sizeOfIndex;//pozicija u drugom nivou
					if (level2pos == 0) //ako je pozicija 0
					{
						ClusterNo y = pd->findFreeBit();//pronadji slobodni bit
						if (y == ~0) //ako nema slobodnog bita
						{
							pd->part->readCluster(0, cluster0);//procitaj nulti klaster
							pd->resetBit(z, cluster0);//resetuj bit nultog klastera
							pd->part->writeCluster(0, cluster0);//upisi nulti klaster

							/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW
							return 0;
						}
						pd->part->readCluster(0, cluster0);//procitaj nulti klaster
						pd->setBit(y, cluster0);//setuj bite nultog klastera
						pd->part->writeCluster(0, cluster0);//upisi nulti klaster
						index[level1pos] = y;//setuj indeks
					}
					Index level2index;//indeksi drugog nivoa
					pd->part->readCluster(index[level1pos], (char*)level2index);//procitaj indeks klaster za drugi nivo
					level2index[level2pos] = z;//setuj indeks
					pd->part->writeCluster(index[level1pos], (char*)level2index);//upisi klaster drugog nivoa
					pd->part->writeCluster(entry.indexCluster, (char*)index);//upisi indeks klaster ulaza
				}
				else //Ako je dostignuta maksimalna velicina fajla
				{
					/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW
					return 0;
				}
				/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

			}
			entry.size++;
		}
		ClusterNo posCluster = filePosition / ClusterSize;//pozicija klastera
		if (posCluster != cluster) 
		{
			if (posCluster < sizeOfIndex / 2)//ako je u prvoj polovini
			{
				pd->part->readCluster(index[posCluster], data);//procitaj podatke
			}
			else {
				ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;//pozicija u prvom nivou
				Index level2;//drugi nivo
				pd->part->readCluster(index[level1pos], (char*)level2);//procitaj indekse za drugi nivo
				ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;//pozicija u drugom nivou
				pd->part->readCluster(level2[level2pos], data);//procitaj podatke iz drugog nivoa
			}
			cluster = posCluster;
		}
		unsigned int posByte = filePosition % ClusterSize;//pozicija bajta
		data[posByte] = buffer[bytesWritten];//pozicija bajta u klasteru podataka setuje se na vrijednost upisanih bajtova
		if (posByte == ClusterSize - 1)//ako je popunjen klaster
		{
			if (posCluster < sizeOfIndex / 2) //ako se on nalazi na prvoj polovini
			{
				pd->part->writeCluster(index[posCluster], data);//upisi podatke
			}
			else 
			{
				ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;//pozicija u prvom nivou
				Index level2;
				pd->part->readCluster(index[level1pos], (char*)level2);//procitaj indeks klaster za drugi nivo
				ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;//pozicija u drugom nivou
				pd->part->writeCluster(level2[level2pos], data);//upisi podatk u drugi nivo
			}
		}
		filePosition++;//uvecaj poziciju fajla
		bytesWritten++;//uvecaj broj upisanih bajtova
	}
	if (cluster < sizeOfIndex / 2) //klaster na prvoj polovini
	{
		pd->part->writeCluster(index[cluster], data);//upisi podatke
	}
	else 
	{
		ClusterNo level1pos = sizeOfIndex / 2 + (cluster - sizeOfIndex / 2) / sizeOfIndex;//pozicija u prvom nivou
		Index level2;
		pd->part->readCluster(index[level1pos], (char*)level2);//procitaj indeks klaster za drugi nivo
		ClusterNo level2pos = (cluster - sizeOfIndex / 2) % sizeOfIndex;//pozicija u drugom nivou
		pd->part->writeCluster(level2[level2pos], data);//upisi podatke na drugi nivo
	}
	return 1;
}

BytesCnt KernelFile::read(BytesCnt max, char * buffer)
{

	BytesCnt bytesRead = 0;//broj procitanih bajtova
	Index index;//indeks
	pd->part->readCluster(entry.indexCluster, (char*)index);//procitaj indeks klaster ulaza
	char data[ClusterSize];//pomocni klaster za podatke - 2KB
	ClusterNo cluster = 0;//pomocni klaster
	pd->part->readCluster(index[0], data);//procitaj index klaster podataka
	while (bytesRead < max && !eof()) //dok ne procita maksimalan broj bajtova ili ne dok ne dodje do kraja fajla
	{ 
		ClusterNo posCluster = filePosition / ClusterSize;//pozicija klastera - indeks
		if (posCluster != cluster)
		{
			if (posCluster < sizeOfIndex / 2)//ako je u prvoj polovini
			{
				pd->part->readCluster(index[posCluster], data);//procitaj podatke
				cluster = posCluster;
			}
			else {
				ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;//poziciju u prvom nivou
				Index level2;
				pd->part->readCluster(index[level1pos], (char*)level2);//procitaj indeks klaster drugog nivoa
				ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;//pozicija u drugom nivou
				pd->part->readCluster(level2[level2pos], data);//upisi podatke na drugi nivo
			}
		}
		unsigned int posByte = filePosition % ClusterSize;//pozicija procitanih bajtova
		buffer[bytesRead] = data[posByte];//setuj procitane bajtove
		filePosition++;//uvecaj poziciju u fajlu
		bytesRead++;//uvecaj broj procitanih bajtova
	}
	return bytesRead;
}

char KernelFile::seek(BytesCnt value)
{
	filePosition = (value < entry.size) ? value : entry.size;//pozicija fajla
	return 1;
}

char KernelFile::eof()
{
	if (filePosition == entry.size) return 2;
	else if (filePosition < entry.size) return 0;
	else return 1; //greska
}


char KernelFile::truncate()
{
	/*Ulaz*/pd->listsMutex.Acquire();//lock lists

	if (pd->openForReading.doesExist(entry))//Ako je otvoren u r modu
	{
		/*Izlaz*/pd->listsMutex.Release();//unlock lists
		return 0;//greska
	}
	/*Izlaz*/pd->listsMutex.Release();//unlock lists

	entry.size = filePosition;//setuj poziciju fajla
	return 1;
}


KernelFile::KernelFile()
{
entry = emptyEntry; //setuj ulaz na vrijednost praznog ulaza
entryDataCluster = entryDataClusterPos = filePosition = 0; //sve setuj na 0
pd = nullptr;//pokazivac na podatke particije 
}

KernelFile::~KernelFile()
{   
	
	/*Ulaz*/pd->listsMutex.Acquire();// lock list
	
    if (pd->openForReading.remove(entry))//ako je uspjesno uklonjen iz liste fajlova koji se citaju
	{
       /*Ulaz*/ pd->filesOpenMutex.Acquire();	//lock filesOpen

        pd->filesOpen--; //Smanji broj otvorenih fajlova

	   /*Izlaz*/pd->filesOpenMutex.Release();   //unlock filesOpen
		
		pd->listsMutex.WakeAllConditionVariable();//odblokiraj niti
		
    }
    else if (pd->openForWriting.remove(entry))//ako je uspjesno uklonjen iz liste fajlova koji se citaju
	{
		/*Ulaz*/pd->filesOpenMutex.Acquire(); //lock filesOpen

        pd->filesOpen--;//Smanji broj otvorenih fajlova

		/*Izlaz*/pd->filesOpenMutex.Release(); //unlock filesOpen
		
        char temp[2048];//2 KB
        RootData *rt;//root data pointer

		/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW
		
        pd->part->readCluster(entryDataCluster, temp);//procitaj klaster
        rt = (RootData*)temp;//formiraj root data pointer
		RootData &rtref = (RootData&)temp;//formiraj referencu na root data pointer
        copyEntry(rtref[entryDataClusterPos], entry);//kopiraj ulaz
		
        pd->part->writeCluster(entryDataCluster, temp);//upisi klaster
		
		/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

		pd->listsMutex.WakeAllConditionVariable();//odblokiraj niti
				
    }
	/*Izlaz*/pd->listsMutex.Release();  //unlock lists
	
	if (pd->filesOpen == 0)//ako nema otvorenih fajlova
		pd->filesOpenMutex.WakeAllConditionVariable();//odblokiraj niti
		
		
}
