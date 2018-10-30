#include "KernelFS.h"
#include <algorithm>
#include <experimental\filesystem>
#include <iostream>

char KernelFS::mount(Partition * partition)
{
	/*Ulaz*/partitionsMutex.Acquire();//lock partitions

	int freePartitionIndex = freePartition();//nalazimo slobodan index za particiju

	if (freePartitionIndex == 26)//ako nema slobodnih particija
	{
		/*Izlaz*/partitionsMutex.Release();//unlock partitions
		return 0;
	}
	partitions[freePartitionIndex] = new PartitionData(partition);//kreiramo novu particiju (mount-ujemo je)

	/*Izlaz*/partitionsMutex.Release();//unlock partitions

	return 'A' + freePartitionIndex;//vracamo slovo particije
}

char KernelFS::unmount(char part)//char part predstavlja slovo particije
{
	if (!IsCharUpper(part)) //ako nije veliko slovo
		return 0;

	PartitionData *pd = getPartDataPointer(part);//pokazivac na podatke particije
	if (pd == nullptr) //ako nema podataka-fajlova
		return 0;

	/*Ulaz*/pd->filesOpenMutex.Acquire();//lock filesOpen

	while (pd->filesOpen > 0)//dok ima otvorenih fajlova
		pd->filesOpenMutex.SleepConditionVariableCS(INFINITE);//cekaj dok se ne zatvore svi fajlovi

	/*Izlaz*/pd->filesOpenMutex.Release();//unlock filesOpen

	/*Ulaz*/partitionsMutex.Acquire();//lock partitions

	delete pd;//brisemo pokazivac na podatke
	partitions[part - 'A'] = nullptr;//brisemo particiju

	/*Izlaz*/partitionsMutex.Release();//unlock partitions

	return 1;
}

char KernelFS::format(char part)//char part predstavlja slovo particije
{
	if (!IsCharUpper(part)) //ako nije veliko slovo
		return 0;

	PartitionData *pd = getPartDataPointer(part);//pokazivac na podatke particije
	if (pd == nullptr) //ako nema podataka-fajlova
		return 0;

	pd->formattingRequest = true;//zahtjev za formatiranje
	/*Ulaz*/pd->filesOpenMutex.Acquire();//lock filesOpen

	while (pd->filesOpen > 0 || pd->threadsReadingRootDir > 0)//dok ima fajlova koji su otvoreni ili niti koji citaju korijenski direktorijum
		pd->filesOpenMutex.SleepConditionVariableCS(INFINITE);//cekaj

	/*Izlaz*/pd->filesOpenMutex.Release();//unlock filesOpen

	pd->formatting = true;//formatiranje u toku
	pd->formattingRequest = false;//zahtjv za formatiranje nije vise potreban
	char buffer[ClusterSize] = "";//prazni buffer velicine velicine klastera - 2KB
	for (ClusterNo i = 1; i < pd->part->getNumOfClusters(); i++)//formatiraj klastere-particiju
		pd->part->writeCluster(i, buffer);//ne polazi se od nultog klastera jer u njemu ne skladistimo indekse niti podatke

	buffer[0] = '\xC0';//1100 0000
	pd->part->writeCluster(0, buffer);//upisi nulti klaster-bit vektor
	pd->formatting = false;//formatiranje zavrseno
	pd->diskRWMutex.WakeAllConditionVariable();//odblokiraj

	return 1;
}

char KernelFS::readRootDir(char part, EntryNum n, Directory & d)
{
	if (!IsCharUpper(part))//ako nije veliko slovo
		return 0;

	PartitionData *pd = getPartDataPointer(part);//pokazivac na podatke particije
	if (pd == nullptr)//ako nema podataka
		return 0;

	if (pd->formatting)//ako je u toku formatiranje particije
		return 0;

	pd->threadsReadingRootDir++;//broj niti koje citaju korjenski direktorijum
	char entriesRead = 0;//broj procitanih ulaza-fajlova
	EntryNum entriesBeforeReading = n;//broj ulaza-fajlova prije citanja
	Index buffer1, buffer2;//buferi-indeksiranje u dva nivoa
	char temp[2048];//pomocni niz 2 KB
	RootData &rtref = (RootData&)temp;//pomocna referenca
	pd->part->readCluster(1, (char*)buffer1);//citanje prvog klastera u buffer1
	for (ClusterNo i = 0; i < sizeOfIndex; i++)
		if (buffer1[i] != 0)//ako je razlicit od nule-ulaz koji se ne koristi ima vrijednost 0
			if (i < sizeOfIndex / 2)// ako je u prvoj polovini gdje je indeksiranja samo u jednom nivou
			{
				pd->part->readCluster(buffer1[i], temp);//procitaj klaster
				for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)//prolazi kroz ulaze
					if (!sameFile(rtref[k], emptyEntry))//ako nije prazan ulaz
					{
						if (entriesRead == ENTRYCNT) //ako je niz pun vrati 65
							return ENTRYCNT + 1;
						if (entriesBeforeReading > 0) //ako ima jos ulaza koji nisu procitani
							entriesBeforeReading--;//smanji broj
						else 
						{
							copyEntry(d[entriesRead], rtref[k]);//kopiraj ulaz
							entriesRead++;//uvecaj broj procitanih
						}
					}
			}
			else//ako je u drugoj polovini, druga polovina ulaza je rasporedjena u dva nivoa
			{
				pd->part->readCluster(buffer1[i], (char*)buffer2);//procitaj klaster
				for (ClusterNo j = 0; j < sizeOfIndex; j++)
				{
					pd->part->readCluster(buffer2[j], temp);//procitaj klaster
					for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
						if (!sameFile(rtref[k], emptyEntry))//ako nije prazan ulaz
						{
							if (entriesRead == ENTRYCNT) //ako je niz pun vrati 65
								return ENTRYCNT + 1;
							if (entriesBeforeReading > 0) //ako ima jos ulaza koji nisu procitani
								entriesBeforeReading--;//smanji broj
							else {
								copyEntry(d[entriesRead], rtref[k]);//kopiraj ulaz
								entriesRead++;//uvecaj broj procitanih
							}
						}
				}
			}
			pd->threadsReadingRootDir--;//smanji broj niti koji citaju
			return entriesRead;//vrati broj procitanih ulaza 
}

char KernelFS::doesExist(char * fname)
{
	
	if (!IsCharUpper(fname[0]))//ako nije veliko slovo
		return 0;

	PartitionData *pd = getPartDataPointer(fname[0]);//pokazivac na podatke particije

	if (pd == nullptr)//ako nema podataka
		return 0;

	if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0'))//ako nije pravilno navedena zeljeni format "X:\"
		return 0;

	Entry e;//ulaz
	ClusterNo c;//broj klastera
	EntryNum n;//broj ulaza
	return pd->_doesExist(fname, e, c, n);
}

File * KernelFS::open(char * fname, char mode)
{
	
	if (!IsCharUpper(fname[0]))//ako nije velikog slova
		return nullptr;
	PartitionData *pd = getPartDataPointer(fname[0]);//pokazivac na podatke particije
	if (pd == nullptr)//ako nema podataka
		return nullptr;

	if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0'))//ako nije pravilno naveden naziv
		return nullptr;

	if (pd->formattingRequest)//ako je u particija  procesu formatiranja
		return nullptr;

	/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

	if (pd->formatting)//ako je particija u procesu formatiranja sacekaj da se zavrsi,tj blokira se nit
		pd->diskRWMutex.SleepConditionVariableCS(INFINITE);

	/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

	File *f = nullptr;//prazan fajl
	int found;
	char temp[2048];
	RootData &rtref = (RootData&)temp;
	Entry returnEntry;//ulaz koji ce biti setovan ako u particiji postoji fajl pod tim imenom
	ClusterNo returnCluster;//klaster koji ce biti setovan ako u particiji postoji fajl pod tim imenom
	EntryNum returnClusterPos;//pozicija klastera koja ce biti setovana ako u particiji postoji fajl pod tim imenom
	int getEntryAgain = 0;//flag koji ce biti setovan na 1 ako je trazeni ulaz-fajl trenutno u upotrebi, ali ce se za izvjesno vrijeme moci otvoriti
	switch (mode) {//modaliteti
	case 'r'://otvara se fajl samo za citanje
		if (pd->_doesExist(fname, returnEntry, returnCluster, returnClusterPos)) //ako postoji fajl
		{
			/*Ulaz*/pd->listsMutex.Acquire();//lock lists

			while (pd->openForWriting.doesExist(returnEntry)) 
			{//dok se upisuje u fajl blokiraj nit
				getEntryAgain = 1;//setuj flag za kasnije otvaranje
				pd->listsMutex.SleepConditionVariableCS(INFINITE);//blokira nit
			}
			if (getEntryAgain) //ako moze da se fajl bezbjedno otvori
			{
				/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

				pd->part->readCluster(returnCluster, temp);//procitaj klaster
				copyEntry(returnEntry, rtref[returnClusterPos]);//kopiraj ulaz

				/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

			}
			pd->openForReading.add(returnEntry);//dodavanje u listu ulaza-fajlova otvorenih radi citanja
			/*Izlaz*/pd->listsMutex.Release();//unlock lists

			f = new File();//instanciraj novi fajl
			/*Ulaz*/pd->filesOpenMutex.Acquire();//lock filesOpen

			pd->filesOpen++;//uvecaj broj otvorenih fajlova
			/*Izlaz*/pd->filesOpenMutex.Release();//unlock filesOpen

			copyEntry(f->myImpl->entry, returnEntry);//kopiraj ulaz
			f->seek(0);//pozicioniraj se na pocetak
			f->myImpl->entryDataCluster = returnCluster;//setuj podatke
			f->myImpl->entryDataClusterPos = returnClusterPos;//setuj poziciju podataka
			f->myImpl->pd = pd;//setuj pokazivac na podatke

			return f;//vrati otvoreni fajl
		}
		else //ako fajl ne postoji
			return nullptr;
	case 'w'://fajl otvoren u modu za upisivanje
		Entry e = makeSearchEntry(fname);//pripremi ulaz za pretragu
		if (pd->_doesExist(fname, returnEntry, returnCluster, returnClusterPos)) //ako postoji
		{
			/*Ulaz*/pd->listsMutex.Acquire();//lock lists

			while (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry))
				//blokiraj nit dok je fajl otvoren
				pd->listsMutex.SleepConditionVariableCS(INFINITE);//blokira nit

			FS::deleteFile(fname);//obrise fajl
			/*Izlaz*/pd->listsMutex.Release();//unlock lists

		}
		//potrebno je pronaci slobodni ulaz
		Index cluster1, buffer2;
		found = 0;//flag za pronadjeni slobodni ulaz
		pd->part->readCluster(1, (char*)cluster1);//procitaj prvi klaster
		
		for (ClusterNo i = 0; i < sizeOfIndex; i++) //trazenje slobodno ulaza
		{
			if (cluster1[i] != 0)//ako se koristi
				if (i < sizeOfIndex / 2)//ako je u prvoj polovini
				{
					pd->part->readCluster(cluster1[i], temp);//procitaj klaster
					for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
						if (rtref[k] == emptyEntry) //ako je prazan
						{
							copyEntry(returnEntry, e);//kopiraj ulaz u povratni ulaz
							returnClusterPos = k;//setuj poziciju
							returnCluster = cluster1[i];//setuj povratni klaster
							copyEntry(rtref[k], e);//kopiraj ulaz u referencu
							pd->part->writeCluster(cluster1[i], temp);//upisi klaster
							found = 1;//setuj flag na 1
							break;
						}
				}
				else //ako nije u prvoj polovini
				{
					pd->part->readCluster(cluster1[i], (char*)buffer2);//procitaj klaster prvog nivoa
					for (ClusterNo j = 0; j < sizeOfIndex; j++) 
					{
						pd->part->readCluster(buffer2[j], temp);//procitaj klastere drugog nivoa
						for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
							if (rtref[k] == emptyEntry) //ako je ulaz prazan
							{
								copyEntry(returnEntry, e);//kopiraj ulaz u povratni ulaz
								returnClusterPos = k;//setuj poziciju
								returnCluster = buffer2[j];//setuj povratni klaster
								copyEntry(rtref[k], e);//kopiraj ulaz u referencu
								pd->part->writeCluster(buffer2[j], temp);//upisi klaster
								found = 1;//setuj flag na 1
								break;
							}
					}
				}
		}
		if (!found) //ako slobodni ulaz nij pronadjen
		{
			ClusterNo level1 = findFreeBit(fname[0]);//pronadji slobodni bit particije
			if (level1 == ~0)//ako ga nema
				return nullptr;

			ClusterNo x = (ClusterNo)~0;

			for (ClusterNo i = 0; i < sizeOfIndex; i++)//pronadji index ulaza koji se ne koristi
				if (cluster1[i] == 0) //ulaz koji se ne koristi ima vrijednost 0
				{
					x = i;
					break;
				}
			if (x == ~0)//ako su svi ulazi zauzeti
				return nullptr;

			char cluster0[ClusterSize];//pomocni nulti klaster-2KB
			pd->part->readCluster(0, cluster0);//procitaj nulti klaster
			pd->setBit(level1, cluster0);//setuj bite prvog nivoa
			pd->part->writeCluster(0, cluster0);//upisi nulti klaster
			if (x < sizeOfIndex / 2) //ako je ulaz na prvoj polovini
			{
				cluster1[x] = level1;//dodijeli prvi nivo
				pd->part->writeCluster(1, (char*)cluster1);//upisi prvi nivo
				RootData *newRD;//pomocni pokazivac na korijen podataka
				pd->part->readCluster(level1, temp);//procitaj klaster
				newRD = (RootData*)temp;//setuj pokazivac na temp
				copyEntry((*newRD)[0], e);// kopiraj ulaz
				copyEntry(returnEntry, e);//kopiraj ulaz
				returnCluster = level1;//povratni klaster
				returnClusterPos = 0;//pozicija povratnog klastera
				pd->part->writeCluster(level1, temp);//ipisi klaster
			}
			else {//ako nije u prvoj polovini
				ClusterNo level2 = findFreeBit(fname[0]);//pronadji slobodni bit u drugom nivou
				if (level2 == ~0) //ako nema slobodno bita
				{
					pd->part->readCluster(0, cluster0);//procitaj nulti klaster
					pd->resetBit(level1, cluster0);//resetuj bite prvog nivoa
					pd->part->writeCluster(0, cluster0);//upisi nulti klaster
					return nullptr;
				}

				ClusterNo y = (ClusterNo)~0;
				for (ClusterNo i = 0; i < sizeOfIndex; i++)//pronadji index ulaza koji se ne koristi 
					if (cluster1[i] == 0)//ako se ne koristi
					{
						y = i;//sacuvaj index
						break;
					}
				if (y == ~0)//ako se svi ulazi koriste
				{
					pd->part->readCluster(0, cluster0);//procitaj nulti klaster
					pd->resetBit(level1, cluster0);//resetuj bite prvog nivoa
					pd->part->writeCluster(0, cluster0);//upisi nulti klaster
					return nullptr;
				}
				pd->part->readCluster(0, cluster0);//procitaj nulti klaster
				pd->setBit(level2, cluster0);//setuj bite drugog nivoa
				pd->part->writeCluster(0, cluster0);//upisi nulti klaster
				cluster1[x] = level1;//setuj indekse prvog nivoa
				pd->part->writeCluster(1, (char*)cluster1);//upisi prvi klaster
				pd->part->readCluster(level1, (char*)buffer2);//procitaj klaster prvog nivoa
				buffer2[y] = level2;//setuj indekse drugog nivoa
				RootData *newRD;//pomocni pokazivac na korijen podataka
				pd->part->readCluster(level2, temp);//procitaj klaster
				newRD = (RootData*)temp;//setuj pokazivac na temp
				copyEntry((*newRD)[0], e);//kopiraj ulaz
				copyEntry(returnEntry, e);//kopiraj ulaz
				returnCluster = level2;//setuj povratni klaster
				returnClusterPos = 0;//setuj povratnu poziciju
				pd->part->writeCluster(level2, temp);//upici klaster
			}
		}
		/*Ulaz*/pd->listsMutex.Acquire();//lock lists

		pd->openForWriting.add(returnEntry);//dodati ulaz-fajl u listu fajlova otvorenih u w modu

		/*Izlaz*/pd->listsMutex.Release();//unlock lists

		f = new File();//instanciraj novi fajl

		/*Ulaz*/pd->filesOpenMutex.Acquire();//lock filesOpen

		pd->filesOpen++;//uvecaj broj otvorenih fajlova

		/*Izlaz*/pd->filesOpenMutex.Release();//unlock filesOpen

		copyEntry(f->myImpl->entry, e);//kopiraj ulaz-fajl
		f->seek(0);//pozicioniraj se na pocetak
		f->myImpl->entryDataCluster = returnCluster;//setuj klaster sa podacima
		f->myImpl->entryDataClusterPos = returnClusterPos;//setuj poziciju
		f->myImpl->pd = pd;//setuj pokazivac na podatke

		return f;//vrati fajl
	case 'a'://modalitet za upis ili citanje
		if (pd->_doesExist(fname, returnEntry, returnCluster, returnClusterPos)) //ako postoji ulaz-fajl
		{
			/*Ulaz*/pd->listsMutex.Acquire();//lock lists

			while (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry)) 
			{//dok su otvoreni za citanje ili pisanje blokiraj nit
				getEntryAgain = 1;//setuj flag za ponovno otvaranje
				pd->listsMutex.SleepConditionVariableCS(INFINITE);
			}

			if (getEntryAgain)//ako je flag setovan
			{
				/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

				pd->part->readCluster(returnCluster, temp);//procitaj klaster
				copyEntry(returnEntry, rtref[returnClusterPos]);//kopiraj ulaz

				/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

			}
			pd->openForWriting.add(returnEntry);//dodati u listu otvorenih fajlova za citanje

			/*Izlaz*/pd->listsMutex.Release();//unlock lists

			f = new File();//instanciraj novi fajl
			/*Ulaz*/pd->filesOpenMutex.Acquire();//unlock filesOpen

			pd->filesOpen++;//uvec broj otvorenih fajlova
			/*Izlaz*/pd->filesOpenMutex.Release();//unlock filesOpen


			copyEntry(f->myImpl->entry, returnEntry);//kopiraj ulaz-fajl
			f->seek(f->myImpl->entry.size);//pozicioniraj se
			f->myImpl->entryDataCluster = returnCluster;//setuj klaster za podatke
			f->myImpl->entryDataClusterPos = returnClusterPos;//setuj poziciju klastera za podatke
			f->myImpl->pd = pd;//setuj pokazivac na podatke
			return f;//vrati fajl
		}
		else return nullptr;
	default:
		return nullptr;
	}
}

char KernelFS::deleteFile(char * fname)
{
	
	if (!IsCharUpper(fname[0]))//ako nije veliko slovo
		return 0;

	PartitionData *pd = getPartDataPointer(fname[0]);//pokazivac na podatke particije
	if (pd == nullptr)//ako nema podataka
		return 0;

	if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0'))//ako naziv nije dobrog formata
		return 0;

	Entry returnEntry;//ulaz koji ce biti setovan ako u particiji postoji fajl pod tim imenom
	ClusterNo returnCluster;//klaster koji ce biti setovan ako u particiji postoji fajl pod tim imenom
	EntryNum returnClusterPos;//pozicija klastera koja ce biti setovana ako u particiji postoji fajl pod tim imenom
	if (!pd->_doesExist(fname, returnEntry, returnCluster, returnClusterPos))//ako ne postoji
		//metoda _doesExist(...) u slucaju da fajl postoji setuje vrijednosti returnEntry-u, returnCluster-u, returnClusterPos-u
		return 0;
	/*Ulaz*/pd->listsMutex.Acquire();//lock lists

	if (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry)) 
	{//Ako je otvoren za citanje ili za pisanje
		
	/*Izlaz*/pd->listsMutex.Release();//unlock lists
	             return 0;
	}
	/*Izlaz*/pd->listsMutex.Release();//unlock lists

	Index indexCluster;//indeks klaster
	char cluster0[ClusterSize];//klaster-2KB
	/*Ulaz*/pd->diskRWMutex.Acquire();//lock diskRW

	pd->part->readCluster(returnEntry.indexCluster, (char*)indexCluster);//procitaj indeks klaster
	pd->part->readCluster(0, cluster0);//procitaj nulti klaster
	int i = 0;//sluzi kao brojac index-a
	//koristenje brojaca se moglo izbjeci ako u for_each petlji zamjenimo i sa (a-indexCluster)
	std::for_each(indexCluster, indexCluster + sizeOfIndex, [&](auto a) {//resetovanje bita kroz nivoe
		if (a) {
			if (!(i < sizeOfIndex / 2))//ako se nalazi u drugoj polovini gdje imaju dva nivoa
			{
				Index level2cluster;//indek klaster drugog nivoa
				pd->part->readCluster(indexCluster[i], (char*)level2cluster);//procitaj index klaster drugog nivoa
				std::for_each(level2cluster, level2cluster + sizeOfIndex, [&](auto b) {if (b)pd->resetBit(b, cluster0); });//resetuj bite drugog nivoa
			}
			pd->resetBit(a, cluster0);//resetuj bite prvog nivoa
		}
		i++;
	});
	pd->resetBit(returnEntry.indexCluster, cluster0);//resetuj bitove
	char temp[2048];//pomocni klaster-2KB
	RootData *rt;//pokazivac na korijen
	pd->part->readCluster(returnCluster, temp);//procitaj klaster
	rt = (RootData*)temp;//setuj na pomocni klaster 
	(*rt)[returnClusterPos] = emptyEntry;//dodijeli prazni ulaz
	pd->part->writeCluster(returnCluster, temp);//upisi klaster
	pd->part->writeCluster(0, cluster0);//upisi nulti klaster

	/*Izlaz*/pd->diskRWMutex.Release();//unlock diskRW

	return 1;
}

KernelFS::KernelFS()
{
    std::fill_n(partitions, 26, nullptr);//popunjava 26 particija sa nullptr, 26 je maksimalan broj particija koje mogu biti mountovane istovremeno
}

KernelFS::~KernelFS()
{
	std::for_each(partitions, partitions + 26, [](auto x) {delete x; });//brise particije
}

int KernelFS::freePartition()//pronalazi slobodnu particiju
{
   return  std::find(partitions, partitions + 26, nullptr) - partitions;
}

char KernelFS::setBit(char part, ClusterNo pos, char* buffer)//setuje bit particije
{
    if (!IsCharUpper(part)) return 0;//ako nije veliko slovo vrati 0
    PartitionData *pd = getPartDataPointer(part);//pokazivac na podatke particije
    if (pd == nullptr) return 0;//ako je nullptr vrati 0
    if (pos >= pd->part->getNumOfClusters()) return 0;//ako je pos veci ili jednak od broja klastera particije
    ClusterNo byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] |= 1 << (7 - bit);//setovanje
    return 1;
}

char KernelFS::resetBit(char part, ClusterNo pos, char* buffer)
{
	if (!IsCharUpper(part)) return 0;//ako nije veliko slovo vrati 0
    PartitionData *pd = getPartDataPointer(part);//pokazivac na podatke particije
    if (pd == nullptr) return 0;//ako je nullptr vrati 0
    if (pos >= pd->part->getNumOfClusters()) return 0;//ako je pos veci ili jednak od broja klastera particije
    int byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] &= ~(1 << (7 - bit));//resetovanje
    return 1;
}

ClusterNo KernelFS::findFreeBit(char part)//pronalazi slobodni bit pomocu naziva particije
{ 
	if (!IsCharUpper(part))  return 0;//ako nije veliko slovo vrati 0
    PartitionData *pd = getPartDataPointer(part);//pronadji pokazivac podataka particije
    if (pd == nullptr) return 0;//ako je nullptr vrati 0
    char cluster0[ClusterSize];//pomocni buffer
    pd->part->readCluster(0, cluster0);//procitaj klaster

    for (ClusterNo byte = 0; byte < ClusterSize; byte++)//pronadji slobodni bit
        for (int bit = 0; bit < 8; bit++) 
            if (!(cluster0[byte] & 1 << (7 - bit))) //ako je bit slobodan
				return byte * 8 + bit;
        
    return (ClusterNo)~0;
}

char copyEntry(Entry & to, Entry & from)
{
	std::copy(from.name, from.name + FNAMELEN, to.name);//kopiraj ime
	std::copy(from.ext, from.ext + FEXTLEN, to.ext);//kopiraj ekstenziju
    to.indexCluster = from.indexCluster;//kopiraj index klastera
    to.size = from.size;//kopiraj velicinu
    return 1;
}

int operator==(const Entry & e1, const Entry & e2)//ako su jednaki vraca 1 u suprotnom 0
{
    return sameFile(e1, e2) && e1.indexCluster == e2.indexCluster && e1.size == e2.size;
}

int sameFile(const Entry & e1, const Entry & e2)//ako su jednaki vraca 1 u suprotnom 0
{
	        
	return (std::equal(e1.name, e1.name + FNAMELEN, e2.name) && std::equal(e1.ext, e1.ext + FEXTLEN, e2.ext));
    
}

Entry makeSearchEntry(char * fname)//formira Entry na osnovu imena fajla
{
	Entry tmp = emptyEntry;
	std::experimental::filesystem::path p(fname);//instanciramo path objekat
	strcpy(tmp.name,p.stem().string().c_str() );//kopiramo  fileName
	strcpy(tmp.ext, p.extension().string().c_str());//kopiramo ekstenziju
	return tmp;

}

