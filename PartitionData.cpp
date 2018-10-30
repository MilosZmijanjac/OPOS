#include "PartitionData.h"
#include <algorithm>

ClusterNo PartitionData::findFreeBit()//pronalazi slobodan bit u particiji
{
	char cluster0[ClusterSize];//privremeni buffer
	ClusterNo numOfClusters = part->getNumOfClusters();//broj klastera
	part->readCluster(0, cluster0);//procitaj klaster
	for (ClusterNo byte = 0; byte < ClusterSize; byte++)//trazi slobodni klaster
		for (int bit = 0; bit < 8; bit++) 
		{
			
			if (!(cluster0[byte] & 1 << (7 - bit)))//ako ima slobodnih
				return byte * 8 + bit;

			if (byte * 8 + bit >= numOfClusters) //Ako nema slobodnih klastera
				return (ClusterNo)~0;
		}

	return (ClusterNo)~0;
}

char PartitionData::setBit(ClusterNo pos, char * buffer)
{
	if (pos >= part->getNumOfClusters()) return 0;//ako je pos veci od broja klastera, tj ako se trenutni bit ne nalazi unutar klastera
	ClusterNo byte = pos / 8;
	int bit = pos % 8;
	buffer[byte] |= 1 << (7 - bit);//setovanje
	return 1;
}

char PartitionData::resetBit(ClusterNo pos, char * buffer)
{
	if (pos >= part->getNumOfClusters()) return 0;//ako je pos veci od broja klastera
	int byte = pos / 8;
	int bit = pos % 8;
	buffer[byte] &= ~(1 << (7 - bit));//resetovanje
	return 1;
}

char PartitionData::_doesExist(char* fname, Entry & entry, ClusterNo & entryCluster, EntryNum & entryPos)
{

	Entry tmp = makeSearchEntry(fname);//ulaz pripremjenj za pretragu
	Index buffer1, buffer2;//indeksi
	char temp[2048];//2 KB
	RootData &rtref = (RootData&)temp;//root data reference

	/*Ulaz*/diskRWMutex.Acquire();//lock diskRW

	part->readCluster(1, (char*)buffer1);//procita prvi klaster 
	
	for (ClusterNo i = 0; i < sizeOfIndex; i++)
		if (buffer1[i] != 0)//ako je klaster koji moze da se koristi
			if (i < sizeOfIndex / 2) //ako je u prvoj polovini
			{
				part->readCluster(buffer1[i], temp);//procitaj klaster
				for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
					if (sameFile(rtref[k], tmp))
					{
						copyEntry(entry, rtref[k]);//kopiraj ulaz
						entryPos = k;//setuj poziciju
						entryCluster = buffer1[i];//setuj klaster ulaza
						/*Izlaz*/diskRWMutex.Release(); //unlock diskRW
						return 1;
					}
			}
			else //ako je u drugoj polovini, gdje su dva nivoa
			{
				part->readCluster(buffer1[i], (char*)buffer2);//ucitaj indeks klaster za drugi nivo
				for (ClusterNo j = 0; j < sizeOfIndex; j++) 
				{
					part->readCluster(buffer2[j], temp);//ucitaj klaster
					for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
						if (sameFile(rtref[k], tmp)) 
						{
							copyEntry(entry, rtref[k]);//kopiraj ulaz
							entryPos = k;//setuj poziciju ulaza
							entryCluster = buffer2[j];//setuj klaster ulaza
							/*Izlaz*/diskRWMutex.Release(); //unlock diskRW
							return 1;
						}
				}
			}
			
	       /*Izlaz*/diskRWMutex.Release(); //unlock diskRW
			return 0;
}

