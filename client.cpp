#include <cstdio>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstdlib>
#include <netdb.h>
#include <sstream>
#include <unistd.h>

using namespace std;

#define MAX_RECORDS 16  //maximum najednou vyhledavanych zaznamu
#define CHAR_ARRAY_LENGTH 1024
#define NUMBER_OF_PARAMETERS 6

typedef struct parameters
{
	int G, N, L, U, H, S;
	int port;
	char hostname[CHAR_ARRAY_LENGTH];
	string login[MAX_RECORDS];
	string uid[MAX_RECORDS];
	int count_records;
	int search_type; //pri vyhledevani podle loginu ma hodnotu 1, podle uid 2
} Tparam;

//napoveda
void print_help(){
	cout << "Synopsis: client –h hostname –p port –l login ... –u uid ... –L –U –G –N –H –S" << endl;
	exit(0);
}

void init_struct(Tparam *p)
{
	p->port = 0;	
	memset(p->hostname, 0, CHAR_ARRAY_LENGTH);
	for(int j=0;j<MAX_RECORDS;j++) 
		p->login[j] = "";
	for(int j=0;j<MAX_RECORDS;j++) 
		p->uid[j] = "";
	p->G = 0; p->N = 0; p->L = 0; p->U = 0; p->H = 0; p->S = 0;
	p->search_type = 0;
	p->count_records = 0;

	return;
	
}
int getparam(int argc,char **argv, Tparam *p){
	
	int c;
	int check_arg_h = 0;
	int check_arg_p = 0;
	int check_arg_search = 0;
	int order = 1; //pro zachovani poradi parametru L,U,G,N,H,S

	int k = 0;

	if((argc == 2) and (strcmp(argv[1], "--help") == 0))
		print_help();

	if(argc < 6){
		cerr << "Jeden z nasledujicich povinnych parametru chybi: -p -h (-u, -l). Zkuste --help." << endl;
		return 10;
	}
	while ((c = getopt(argc, argv, "h:p:l:u:LUGNHS")) != -1) {	
		switch (c) {
			case 'h':
 				strcpy(p->hostname, optarg);
				check_arg_h = 1;
				break;
			case 'p':
				p->port = atoi(optarg);
				check_arg_p = 1;
				break;
			case 'l':
				k = 0;	
 				p->login[0] = optarg;
				if(p->login[0].substr(0,1) == "-"){
					cerr << "Zadejte loginy, ktere chcete vyhledat." << endl;
					exit(10);
				}

				while((optind < argc) and (argv[optind][0] != '-')){
					k++;
					p->login[k] = argv[optind];
					optind++;
				}
							
				check_arg_search = 1;
				p->search_type = 1;
				p->count_records = k+1;
				break;
			case 'u':
				k = 0;
				p->uid[0] = optarg;
				if(p->uid[0].substr(0,1) == "-"){
					cerr << "Zadejte uid, ktere chcete vyhledat." << endl;
					exit(10);
				}
				
				while((optind < argc) and (argv[optind][0] != '-')){
					k++;
					p->uid[k] = argv[optind];
					optind++;
				}
				
				check_arg_search = 1;
				p->search_type = 2;
				p->count_records = k+1;
				break;
			case 'L':
				p->L = order;
				order++;
				break;
			case 'U':
				p->U = order;
				order++;
				break;
			case 'G':
				p->G = order;
				order++;
				break;
			case 'N':
				p->N = order;
				order++;
				break;
			case 'H':
				p->H = order;
				order++;
				break;
			case 'S':
				p->S = order;
				order++;
				break;
			case '?':
				cerr << "Nevalidni argumenty. Zkuste --help." << endl;
				exit(10);
				break;
			default: 
				break;
		}
	}
	if((check_arg_h == 0) or (check_arg_p == 0) or (check_arg_search == 0)){
		cerr << "Nejsou zadany vsechny povinne parametry" << endl;
		return 10;
	}		

	return 0;	
	

}

//vytvori request pro server na zaklade parametru se kterymi byl program spusten
//pro "./client -p 12 -h localhost -l rab rysav jan kos -GNU" bude req vypadat takto "l,4,rab,rysav,jan,kos,GNU0"
//l - hledam podle loginu, 4 loginy ocekavam, 0 - ukonceni casti s loginy, prepinace, 0 - ukonceni retezce
const char *build_request(Tparam *p){

	string req = "";
	char buffer[32];
	memset(buffer, 0, 32);


	if(p->search_type == 1){ // hledame podle loginu
		req = "l,";
		sprintf(buffer, "%d", p->count_records);
		req = req + buffer + ",";
		for(int k=0; k<p->count_records; k++) //naplni string loginy oddelenymi carkou
			req = req + p->login[k] + ",";
	}
	else if(p->search_type == 2){ // hledame podle uid
		req = "u,";
		sprintf(buffer, "%d", p->count_records);
		req = req + buffer + ",";
		for(int k=0; k<p->count_records; k++){
			req += p->uid[k];
			req += + ",";
		}
	}


	for(int j=1; j<=NUMBER_OF_PARAMETERS; j++){  //prida parametry zaznamu do requestu v poradi, v jakem je uziv. zadal	
		if(p->L == j) req += "L";
		if(p->U == j) req += "U";
		if(p->G == j) req += "G";
		if(p->N == j) req += "N";
		if(p->H == j) req += "H";
		if(p->S == j) req += "S";
	}
	req += "0";


	return req.c_str();
	
}


int main(int argc, char **argv){

	int mysocket, rc;
	struct hostent *host = NULL;
	sockaddr_in sin;

	const char *request;
	char msg[MAX_RECORDS*CHAR_ARRAY_LENGTH];
	memset(msg, 0, CHAR_ARRAY_LENGTH);

	Tparam p;
	int retval = 0;

	init_struct(&p);
	if((retval = getparam(argc,argv,&p)) != 0)
		return retval;

	
	//preklad domenoveho jmena
	if((host = gethostbyname(p.hostname)) == NULL){
		cerr << "Operace gethostbyname selhala." << endl;
		return -1;
	}
	
	//tvorba socketu
	if((mysocket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Chyba pri vytvareni socketu." << endl;
		return -1;
	}

	//nastaveni struktury pro connect
	sin.sin_family = AF_INET;
	sin.sin_port = htons(p.port);

	memcpy(&(sin.sin_addr), host->h_addr, host->h_length);

	//pripojeni k serveru
	if((rc = connect(mysocket,(sockaddr *)&sin, sizeof(sin))) < 0){
		cerr << "Operace connect selhala." << endl;
		return -1;
	}

	request = build_request(&p); //sestavi request pro server

	if(write(mysocket, request, strlen(request)) < 0){
		cerr << "Operace write selhala." << endl;
		return -2;
	}


	if(read(mysocket, msg, sizeof(msg)) < 0){
		cerr << "Operace read selhala." << endl;
		return -3;
	} 

	cout << msg << endl; //tiskne odpoved
	
	//uzavreni socketu
	if(close(mysocket) < 0){
		cerr << "Nepodarilo se uzavrit socket." << endl;
		return -1;
	}

	return 0;

}

