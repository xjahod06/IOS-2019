/*
synchronizace paralernich processu
	"River Crossing Problem"
autor: Vojtech jahoda (xjahod06)
fakulta: VUT FIT
datum: 22.04.2019
*/

#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>

//funkce na "namapovani" sdilene pameti (upravene ale prevzate z democvika)
#define MMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
//funkce pro "odmapovani" sdilene pameti (upravene ale prevzate z democvika)
#define UNMAP(pointer) {munmap((pointer),sizeof((pointer)));}
//funkce pro uspani processu na nahodou dobu
#define mysleep(time) {if (time != 0) alarm(rand() % time);}
//definovani hodnot pro vypocty
#define LOCK 0
#define SHIP_CAPACITY 4
#define MAX_SLEEP 2000
#define VYPIS "proj2.out"

enum vycet {ALL_OK,ERR}; //definuje navratove hodnoty

//kontrola parametru programu
int kontrola (char *arg, int *promena, int n, bool *chyba);
//vytisk chybove hlasky pro zadani spatneho púoptku parametru
void printerr();
//inicializace sdilene pameti, semaforu a otevreni programu
int inicializace(int C);
//vycisteni sdilene pameti, semaforu a uzaveni programu
void cleanup();
//samotne rizeni jizdy lode
void ship_ride(int P);	
//generovani processu s nazvem "serif"
void gen_serif(int persons, int serifs_break,int back_time,int R);
//samotne processy serif
void serf_self(int serf_id,int R,int back_time);
//cekani processu na volne misto ve fronte "molo"
void serf_waiting(int back_time,int serf_id);
//samotne cekani na "mole" a nasledne nalodeni
void serf_pier(int serf_id,int R,int ID);
//generovani processu ze tridy hacker
void gen_hacker(int persons, int hackers_break,int back_time,int R);
//samotne processy typu hacker
void hack_self(int hack_id,int R,int back_time);
//cekani processu hacker na volne misto na "molu"
void hack_waiting(int back_time,int hack_id);
//cekani processu hacker na molu + nalodeni
void hack_pier(int hack_id,int R,int ID);
//"nalodeni processu" pridani ID processu do sdilene pameti "ship"
void boarding(int ID);
//vypis do souboru v danem formatu
void write_out(char *who,char *what,int id);

//vycet semaforu a sdilenych pameti
sem_t *hacker = NULL;
sem_t *serif = NULL;
sem_t *stop_hacker = NULL;
sem_t *stop_serif = NULL;
sem_t *generate = NULL;
sem_t *board = NULL;
sem_t *board_end = NULL;
sem_t *captain = NULL;
sem_t *vypis = NULL;
sem_t *ride = NULL;
sem_t *serf_wait = NULL;
sem_t *captain_promote = NULL;
sem_t *captain_wake_up = NULL;
sem_t *hack_wait = NULL;
sem_t *serf_wait_end = NULL;
sem_t *hack_wait_end = NULL;
int *process_counter = NULL;
int *process_id = NULL;
int *cap_molo = NULL;
int *serf_molo = NULL;
int *hack_molo = NULL;
int (*ship)[SHIP_CAPACITY] = NULL;
int *ship_counter = NULL;
int *captain_id = NULL;
FILE* output;

int main (int argc, char* argv[])
{
	//pocet argumentu programu musi byt presne 7
	if (argc != 7){
		//pokud ne tak nastava volani vypisu chyby a ukonci program
		printerr();
		return ERR;
	}
	//sefinovani zakladnich promennych
	int P,H,S,R,W,C;
	bool chyba = false;
	//pocet osob v kazde kategorii
	kontrola(argv[1],&P,1,&chyba);
	//max doba generovani noveho hackera
	kontrola(argv[2],&H,2,&chyba);
	//max doba generovani neveho serifa
	kontrola(argv[3],&S,3,&chyba);
	//max doba plavby
	kontrola(argv[4],&R,4,&chyba);
	//max doba po ktere se vraci z5 na molo
	kontrola(argv[5],&W,5,&chyba);
	//kapacita mola
	kontrola(argv[6],&C,6,&chyba);

	//kontrola jestli nenastala chyba v prubehu kontroli argumentu
	if (chyba == true)
		return ERR;
	//kontrola rozsahu parametru
	if (P < 2 || (P % 2) != 0){
		fprintf(stderr,"parametr \"P\" musi: P>= 2 && delitele 2mi \n");
		return ERR;
	}if (H < 0 || H > MAX_SLEEP){
		fprintf(stderr,"parametr \"H\" musi: H >= 0 && H <= %d\n", MAX_SLEEP);
		return ERR;
	}if (S < 0 || S > MAX_SLEEP){
		fprintf(stderr,"parametr \"S\" musi: S >= 0 && S <= %d\n", MAX_SLEEP);
		return ERR;
	}if (R < 0 || R > MAX_SLEEP){
		fprintf(stderr,"parametr \"R\" musi: R >= 0 && R <= %d\n", MAX_SLEEP);
		return ERR;
	}if (W < 20 || W > MAX_SLEEP){
		fprintf(stderr,"parametr \"W\" musi: W >= 20 && W <= %d\n", MAX_SLEEP);
		return ERR;
	}if (C < 5){
		fprintf(stderr,"parametr \"C\" musi: C >= 5\n");
		return ERR;
	}

	//kontrola prubehu inicializace
	if (inicializace(C) == ERR){
		cleanup();
		exit(ERR);
	}

	//rozdvojeni processu
	pid_t gen_s = fork();
	if (gen_s == 0){
		//dite
		//spusteni generace proressu typu sherif
		gen_serif(P,S,W,R);
	//kontrola uspechu forku
	}else if(gen_s < 0){
		perror("Fork error "); 
		cleanup();
		exit(ERR);
	}else{
		//rodic
		//rozdvoje processu
		pid_t gen_h = fork();
		if (gen_h == 0)
		{	//dite
			//spusteni generace processu typu hacker
			gen_hacker(P,H,W,R);
		//kontrola forku
		}else if(gen_h < 0){
		perror("Fork error "); 
		cleanup();
		exit(ERR);
		}else{
			//rodic
			//spusteni jizdy lode
			ship_ride(P);
		}
	}
	//ceka na ukonce generace procressu serif a hacker
	sem_wait(stop_serif);
	sem_wait(stop_hacker);
	//uklid
	cleanup();
	//ukonceni
	exit(ALL_OK);
	return ALL_OK;
}
/*
@ P -> pocet processu pro oba typy
*/
void ship_ride(int P){
	//pocitani pluti lode
	int borading_counter = 0;
	//provadi se dokud counter nedosahne hodnoty P/(SHIP_CAPACITY/2) tj. dokud nepreveze vsechny processy
	while(borading_counter < (P/(SHIP_CAPACITY/2)) ){
		//kontroluje kombinace processu na molu 4*serif nebo 4*hacker nebo 2*serfi a 2* hacker
		if (*serf_molo >= SHIP_CAPACITY || *hack_molo >= SHIP_CAPACITY || (*hack_molo >= (SHIP_CAPACITY/2) && *serf_molo>=(SHIP_CAPACITY/2)) )
		{
			//ceka dokud neskonci vsechny processy co "prevazel"
			for (int i = 0; i < SHIP_CAPACITY; ++i)
				sem_wait(ride);

			//podle kombinace na vstupu necha projit pres molo urcity pocet a typ processu
			if (*serf_molo >=SHIP_CAPACITY){
				for (int i = 0; i < SHIP_CAPACITY; ++i)
					sem_post(serf_wait);
			}
			else if (*hack_molo >=SHIP_CAPACITY){
				for (int i = 0; i < SHIP_CAPACITY; ++i)
					sem_post(hack_wait);
			}
			else if (*hack_molo >=(SHIP_CAPACITY/2) && *serf_molo>=(SHIP_CAPACITY/2)){
				sem_post(serf_wait);
				sem_post(hack_wait);
				sem_post(serf_wait);
				sem_post(hack_wait);
			}else{
				continue;
			}
			//ceka dokud vsechny processy nedokonci fazi nalodeni
			for (int i = 0; i < SHIP_CAPACITY; ++i)
				sem_wait(board_end);

			//urci captain id tj, ktery z nalodenych procesu se stane kapitanem
			*captain_id = *ship[rand() % SHIP_CAPACITY];

			//pusti processy dal uz s urcenym kapitanem
			for (int i = 0; i < SHIP_CAPACITY; ++i)
				sem_post(captain_promote);
			//vrati hodnotu "ship counter" na 0, tj muze probehnout dalsi nalodeni
			*ship_counter = 0;
			//zmensi hodnotu mola
			if (*serf_molo >=SHIP_CAPACITY)
				*serf_molo -= SHIP_CAPACITY;
			else if (*hack_molo >=SHIP_CAPACITY)
				*hack_molo -= SHIP_CAPACITY;
			else if (*hack_molo >=(SHIP_CAPACITY/2) && *serf_molo>=(SHIP_CAPACITY/2)){
				*serf_molo -= (SHIP_CAPACITY/2);
				*hack_molo -= (SHIP_CAPACITY/2);
			}else{
				continue;
			}
			//pocet prevezeni svysi o 1
			borading_counter++;
		}
	}
}
/*kontroluej zadane paramtery programu
@ arg -> preda argument
@ promena -> promena do ktere se uklada zadana hodnota
@ n -> kolikaty parametr byl predan
@ chyba -> hodnoty typu bool na vraceni chyby do hlavniho pocessu
*/
int kontrola (char *arg, int *promena, int n, bool *chyba)
{
	char *ctrl;
	*promena = strtol(arg,&ctrl,10);
	if (*ctrl != '\0'){
		fprintf(stderr,"%i.parametr (\"%s\") neni kladne cele cislo\n",n,ctrl );
		*chyba = true;
		return ERR;			
	}
	return ALL_OK;
}

/*vypise se pri chybne zadanem vstupu jako napoveda
*/
void printerr(){
	fprintf(stderr,"spatny format vstupu\n");
	fprintf(stderr,"spravny: \"./proj2 P H S R W C\"\n");
	fprintf(stderr,"	-P pocet generovanych osob\n");			
	fprintf(stderr,"	-H max doba generovani noveho hackera\n");
	fprintf(stderr,"	-S max doba generovani neveho serifa\n");
	fprintf(stderr,"	-R max doba plavby\n");
	fprintf(stderr,"	-W max doba po ktere se vraci z5 na molo\n");
	fprintf(stderr,"	-C kapacita mola\n");
	return;
}
/*generuje procesy typu serif
@ persons -> pocet processu co ma generovat
@ serifs_break -> max doba generovani dalsiho processu(uspani)
@ back_time -> navratova hodnota pro odchazeni z mola
@ R -> max doba uspani kapitana
*/
void gen_serif(int persons, int serifs_break,int back_time,int R){
	for (int i = 1; i <= persons; ++i){
		//uspani processu z intervalu <0,serifs_break(S)>
		mysleep(serifs_break);
		//ceka na dogenerovani predchoziho processu (aby se nezacali generovat 2 zaroven tj. meli by stejne ID)
		sem_wait(generate);

		pid_t serf = fork();
		if (serf == 0){
			//samotna generace processu typu serif
			serf_self(i,R,back_time);
		//osetreni mozne chyby forku
		}else if(serf < 0){
			perror("Fork error "); 
			cleanup();
			exit(ERR);
		}else{
			//ceka dokud se nevygeneruje momentalni process (aby se neulozili 2 pod stejnym ID)
			sem_wait(serif);
		}
		//pusti moznost generace dalsiho processu	
		sem_post(generate);
		if (i == persons)
			sem_post(serf_wait_end);
	}
	//ceka nez se vygeneruji vsechny processy typu serif, aby mohl ukoncit svoji cinnost a ohlasit ze vechyn jeho processy skoncili
	for (int i = 0; i < persons; ++i)
		sem_wait(serf_wait_end);
	//povoli ukonceni programu ze sve strany 
	sem_post(stop_serif);
	exit(ALL_OK);
}
/*samotne celo processu serf
@ serf_id -> id pro processy typu serf
@ R -> max doby uspani kapitana
@ back_time -> max doba po ktere se process vraci zpatky na "molo"
*/
void serf_self(int serf_id,int R,int back_time){
	//zapamtuje si svoje ID 
	int ID=*process_id;
	*process_id += 1;
	//pusti generaci dalsiho serfu
	sem_post(serif);
	write_out("SERF","starts",serf_id);

	//samotne telo processu kdy kontroluje jestli je na molu misto, pokud ne tak jde cekat 
	while (true){
		if ((*serf_molo+*hack_molo) < *cap_molo)
		{
			serf_pier(serf_id,R,ID);
			break;
		}else{
			serf_waiting(back_time,serf_id);
		}
	}
	//oznami ze dokoncil plavbu
	sem_post(ride);
	//oznami ze se ukoncil
	sem_post(serf_wait_end);
	exit(ALL_OK);
}
/*akce na molu
@ serf_id -> id processu typu serf
@ R -> max doba uspani kapitana plavby
@ ID -> id processu v celem programu
*/
void serf_pier(int serf_id,int R,int ID){
	*serf_molo += 1;
	write_out("SERF","waits",serf_id);

	//ceka dokud nebude moct "jit na palubu"
	sem_wait(serf_wait);
	//zacne nalodeni
	boarding(ID);
	//ceka dokud se neurci kapitan
	sem_wait(captain_promote);		
	if (ID == *captain_id){
		//pokud je kapitan tak se pochlubi
		write_out("SERF","boards",serf_id);
		//uspi se kapitansky process
		mysleep(R);
		//postupne vzbudi vsechny nekapitanske processy co jsou na lodi
		for (int i = 0; i < SHIP_CAPACITY-1 ; ++i)
			sem_post(captain_wake_up);	
		//kapitan ceka dokud se vsichni nevylodi
		for (int i = 0; i < 3; ++i)
			sem_wait(captain);
		write_out("SERF","captain exits",serf_id);
	}else{
		//ceka dokud se neprobudi kapitan
		sem_wait(captain_wake_up);
		write_out("SERF","member exits",serf_id);
		//oznami kapitanovi ze se vylodil
		sem_post(captain);
	}
}
/*"nalodeni" processu na lod
@ ID -> id processu v celem programu
*/
void boarding(int ID){
	//ceka jestli se muze nalodit
	sem_wait(board);
	//prida do pole ship sve globalni id a tim se "nalodi"
	*ship[*ship_counter]=ID;
	*ship_counter += 1;
	//oznami ze se nalodil lodi
	sem_post(board_end);
	//posle dalsiho na nalodeni
	sem_post(board);
}
/*vypis jestlivych akci processu
@ who -> typ processu
@ what -> jaka akce
@ id -> jeho momentalni id typu processu
*/
void write_out(char *who,char *what,int id){
	//ceka jestli se nevypisuje neco jineho
	sem_wait(vypis);
	if (strcmp(what,"is back") == 0 || strcmp(what,"starts") == 0  ){
		fprintf(output,"%d	: %s %d 	:%s\n",*process_counter,who,id,what);
	}
	else
		fprintf(output,"%d	: %s %d 	:%-13s		: %d 	: %d\n",*process_counter,who,id,what,*hack_molo,*serf_molo);
	//cisti buffer pri vypisu ??
	fflush(output);
	*process_counter += 1;
	//povoli dalsi vypis
	sem_post(vypis);
}
/*ceka na volne misto na molu
@ back_time -> max doba za jakou se muze vratit
@ serf_id -> id typu processu
*/
void serf_waiting(int back_time,int serf_id){
	write_out("SERF","leaves queue",serf_id);
	if (back_time != 20){
		back_time = (rand() % (back_time-20))+20;
		mysleep(back_time);
	}
	write_out("SERF","is back",serf_id);
}
/*generuje procesy typu serif
@ persons -> pocet processu co ma generovat
@ hackers_break -> max doba generovani dalsiho processu(uspani)
@ back_time -> navratova hodnota pro odchazeni z mola
@ R -> max doba uspani kapitana
*/
void gen_hacker(int persons, int hackers_break,int back_time,int R){
	for (int i = 1; i <= persons; ++i){
		//uspani processu v nahodnem intervalu <0,hackers_break(H)>
		mysleep(hackers_break);
		//ceka na dogenerovani predchoziho processu (aby se nezacali generovat 2 zaroven tj. meli by stejne ID)
		sem_wait(generate);

		pid_t hack = fork();
		if (hack == 0)
		{
			//generovani processu typi hack
			hack_self(i,R,back_time);
		//osetreni mozne chyby fork()
		}else if(hack < 0){
			perror("Fork error "); 
			cleanup();
			exit(ERR);
		}else{
			//ceka dokud se nevygeneruje momentalni process (aby se neulozili 2 pod stejnym ID)
			sem_wait(hacker);
		}
		//pusti moznost generace dalsiho processu	
		sem_post(generate);
	}
	//ceka nez se vygeneruji vsechny processy typu serif, aby mohl ukoncit svoji cinnost a ohlasit ze vechyn jeho processy skoncili
	for (int i = 0; i < persons; ++i)
		sem_wait(hack_wait_end);
	//povoli ukonceni programu ze sve strany
	sem_post(stop_hacker);
	exit(ALL_OK);
}
/*samotne celo processu serf
@ hack_id -> id pro processy typu hack
@ R -> max doby uspani kapitana
@ back_time -> max doba po ktere se process vraci zpatky na "molo"
*/
void hack_self(int hack_id,int R,int back_time){
	//zapamatuje se svoje ID
	int ID=*process_id;
	*process_id += 1;
	//povoli generaci dalsiho processu typu hacker
	sem_post(hacker);
	//pocka jestli se neco nevypisuje a pripadne vypise ze zacal
	write_out("HACK","starts",hack_id);

	//samotne telo processu kdy kontroluje jestli je na molu misto, pokud ne tak jde cekat 
	while (true){
		if ((*serf_molo+*hack_molo) < *cap_molo)
		{
			hack_pier(hack_id,R,ID);
			break;
		}else{
			hack_waiting(back_time,hack_id);
		}
	}
	//oznami ze dokoncil plavbu
	sem_post(ride);
	//oznami ze se ukoncil
	sem_post(hack_wait_end);
	exit(ALL_OK);
}
/*ceka na volne misto na molu
@ back_time -> max doba za jakou se muze vratit
@ hack_id -> id typu processu
*/
void hack_waiting(int back_time,int hack_id){
	write_out("HACK","leaves queue",hack_id);
	if (back_time != 20){
		back_time = (rand() % (back_time-20))+20;
		mysleep(back_time);
	}
	mysleep(back_time);
	write_out("HACK","is back",hack_id);
}
/*akce na molu
@ hack_id -> id processu typu hack
@ R -> max doba uspani kapitana plavby
@ ID -> id processu v celem programu
*/
void hack_pier(int hack_id,int R,int ID){
	*hack_molo += 1;
	write_out("HACK","waits",hack_id);

	//ceka dokud nebude moct "jit na palubu"
	sem_wait(hack_wait);
	//zacne nalodeni
	boarding(ID);
	//ceka dokud se neurci kapitan
	sem_wait(captain_promote);		
	if (ID == *captain_id){
		//pokud je kapitan tak se pochlubi		
		write_out("HACK","boards",hack_id);
		//uspi se kapitansky process
		mysleep(R);
		//postupne vzbudi vsechny nekapitanske processy co jsou na lodi
		for (int i = 0; i < SHIP_CAPACITY-1 ; ++i)
			sem_post(captain_wake_up);
		//kapitan ceka dokud se vsichni nevylodi
		for (int i = 0; i < 3; ++i)
			sem_wait(captain);
		write_out("HACK","captain exits",hack_id);
	}else{
		//ceka dokud se neprobudi kapitan
		sem_wait(captain_wake_up);
		write_out("HACK","member exits",hack_id);
		//oznami kapitanovi ze se vylodil		
		sem_post(captain);
	}
}
/*inicializace semaforu, sdilenych pameti a otevreni souboru a kontrola spravne inicializace 
@ C -> kapacita lodi
*/
int inicializace(int C){
	//semafor pro generovani processu typu hacker
	if ((hacker = sem_open("/xjahod06.ios.proj2.hacker1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro generovani processu typu serif
	if ((serif = sem_open("/xjahod06.ios.proj2.serif1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro oznameni ukonceni generace processu typu serif
	if ((stop_serif = sem_open("/xjahod06.ios.proj2.stop_serif1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro oznameni ukonceni generace processu typu hacker
	if ((stop_hacker = sem_open("/xjahod06.ios.proj2.stop_hacker1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro synchronizaci generace processu
	if ((generate = sem_open("/xjahod06.ios.proj2.generate1", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED)
		return ERR;
	//semafor pro postupne nalodeni
	if ((board = sem_open("/xjahod06.ios.proj2.board1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro oznameni ukonceni nalodeni
	if ((board_end = sem_open("/xjahod06.ios.proj2.board_end1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro oznameni kapitanovi ze se vsichni vylodili
	if ((captain = sem_open("/xjahod06.ios.proj2.captain1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro postupny vypis
	if ((vypis = sem_open("/xjahod06.ios.proj2.vypis1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro oznameni o dokonceni jizdy (na zacatku necha projit 4)
	if ((ride = sem_open("/xjahod06.ios.proj2.ride1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro povoleni nalodeni processu typu serf
	if ((serf_wait = sem_open("/xjahod06.ios.proj2.serf_wait1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro cekani na urceni kapitana
	if ((captain_promote = sem_open("/xjahod06.ios.proj2.captain_promote1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro cekani na probuzeni kapitana
	if ((captain_wake_up = sem_open("/xjahod06.ios.proj2.captain_wake_up1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro povoleni nalodeni processu typu hack
	if ((hack_wait = sem_open("/xjahod06.ios.proj2.hack_wait1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro urceni ukonceni vsech podprocessu typu hack
	if ((hack_wait_end = sem_open("/xjahod06.ios.proj2.hack_wait_end1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//semafor pro urceni ukonceni vsech podprocessu typu serf
	if ((serf_wait_end = sem_open("/xjahod06.ios.proj2.serf_wait_end1", O_CREAT | O_EXCL, 0666, LOCK)) == SEM_FAILED)
		return ERR;
	//urceni ID processu v globalnim meritku
	MMAP(process_id);
	//pocet processu typu serf cekajicim na molu
	MMAP(serf_molo);
	//pocet processu typu hack cekajicim na molu
	MMAP(hack_molo);
	//kapacita mola
	MMAP(cap_molo);
	//pole lodi dop ktereho se pridavaji id ktere se "prevazi"
	MMAP(ship);
	//index mista na lodi
	MMAP(ship_counter);
	//urceni kapitanskeho id
	MMAP(captain_id);
	//pocitani processu "A"
	MMAP(process_counter);
	//soubor na vypis
	MMAP(output);
	output=fopen(VYPIS,"w");
	if(output == NULL){
		return ERR;
	}
	sem_post(board);
	sem_post(vypis);
	for (int i = 0; i < SHIP_CAPACITY; ++i)
		sem_post(ride);
	sem_post(generate);
	//inicializace zakladnich hodnot
	*process_id = 1;
	*process_counter = 1;
	*cap_molo = C;
	*ship_counter = 0;
	return ALL_OK;
}
/*vycisteni semaforu, sdilenych promennych a uzavreni souboru
*/
void cleanup(){
	fclose(output);
	UNMAP(output);
	UNMAP(process_id);
	UNMAP(serf_molo);
	UNMAP(hack_molo);
	UNMAP(cap_molo);
	UNMAP(ship);
	UNMAP(ship_counter);
	UNMAP(captain_id);
	UNMAP(process_counter);
	sem_close(hacker);
	sem_close(serif);
	sem_close(stop_hacker);
	sem_close(stop_serif);
	sem_close(generate);
	sem_close(board);
	sem_close(board_end);
	sem_close(captain);
	sem_close(vypis);
	sem_close(ride);
	sem_close(serf_wait);
	sem_close(serf_wait_end);
	sem_close(captain_promote);
	sem_close(captain_wake_up);
	sem_close(hack_wait);
	sem_close(hack_wait_end);
	sem_unlink("/xjahod06.ios.proj2.hacker1");
	sem_unlink("/xjahod06.ios.proj2.serif1");
	sem_unlink("/xjahod06.ios.proj2.stop_hacker1");
	sem_unlink("/xjahod06.ios.proj2.stop_serif1");
	sem_unlink("/xjahod06.ios.proj2.plavba1");
	sem_unlink("/xjahod06.ios.proj2.generate1");
	sem_unlink("/xjahod06.ios.proj2.board1");
	sem_unlink("/xjahod06.ios.proj2.board_end1");
	sem_unlink("/xjahod06.ios.proj2.captain1");
	sem_unlink("/xjahod06.ios.proj2.vypis1");
	sem_unlink("/xjahod06.ios.proj2.ride1");
	sem_unlink("/xjahod06.ios.proj2.serf_wait1");
	sem_unlink("/xjahod06.ios.proj2.serf_wait_end1");
	sem_unlink("/xjahod06.ios.proj2.captain_promote1");
	sem_unlink("/xjahod06.ios.proj2.captain_wake_up1");
	sem_unlink("/xjahod06.ios.proj2.hack_wait1");
	sem_unlink("/xjahod06.ios.proj2.hack_wait_end1");
}