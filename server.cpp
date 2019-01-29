#include <cstdio>
#include <fstream>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstdlib>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <signal.h>
#include <sys/wait.h>

using namespace std;


#define CHAR_ARRAY_LENGTH 1024
#define MAX_RECORDS 16
#define NUMBER_OF_PARAMETERS 6
#define SEARCH_BY_LOGIN 1
#define SEARCH_BY_UID 2

//struktura pozadavku
typedef struct parameters
{
	int L, U, G, N, H, S;	// login, uid, gid, gecos, domovsky adresar, logovaci shell
	string login[MAX_RECORDS];
	string uid[MAX_RECORDS];
	int count_records;
	string record[2 * MAX_RECORDS];
} Tparam;

void print_help(){
	cout << "Synopsis: server –p port" << endl;
	exit(0);
}


//inicializace struktury pozadavku klienta
void init_struct(Tparam *p)
{	
	for(int j=0;j<MAX_RECORDS;j++){ 
		p->login[j] = "";
		p->uid[j] = "";
	}
	for(int j=0;j<2*MAX_RECORDS;j++){ 
		p->record[j] = "";
	}
	p->G = 0; p->N = 0; p->L = 0; p->U = 0; p->H = 0; p->S = 0;
	p->count_records = 0;

	return;
	
}

//
void fill_records(Tparam *p, string msg, int type){

	string temp;
	unsigned pos;

	
	pos = msg.find(","); //najdi prvni carku
	for(int j=0;j<p->count_records;j++){ 
		temp = msg.substr(0,pos); //do temp uloz text mezi carkami
		if(type == SEARCH_BY_LOGIN)
			p->login[j] = temp;
		else if(type == SEARCH_BY_UID)
			p->uid[j] = temp;
		msg = msg.substr(pos+1); //nacti pouze nezpracovanou cast stringu
		pos = msg.find(","); //najdi dalsi carku
	}

	return;

}


//naplneni struktury pozadavku klienta
int fill_req_struct(Tparam *p, char m[CHAR_ARRAY_LENGTH]){

	string msg(m);
	string temp;
	int type = 0;
	int order = 1;
	unsigned pos = 0;

	msg = msg.substr(2);  //msg: l,3,xkysil00,rabj,rysavy,LUG0 -> 3,xkysil00,... ->
	pos = msg.find(","); 
	temp = msg.substr(0,pos); //temp: -> 3
	stringstream convert(temp);		// string to
	if(!(convert >> p->count_records)) 	// int
   		p->count_records = 0;
	
	msg = msg.substr(pos+1); //msg: xkysil00,rabj,rysavy,LUG0

	if(m[0] == 'l'){ //login
		type = SEARCH_BY_LOGIN;
		fill_records(p, msg, type);
	}
	else if(m[0] == 'u'){ //uid
		type = SEARCH_BY_UID;
		fill_records(p, msg, type);
	}

	pos = msg.find_last_of(",");  //najde posledni carku v request msg
	msg = msg.substr(pos+1);  //odstrani vse co bylo pred posledni carkou, vcetne carky
				  //zbydou pouze parametry zaznamu

	while((temp = msg.substr(0,1)) != "0"){	//nacitej prepinace po jednom a nastav podle toho promenne ve strukture
		msg = msg.substr(1);
		if(temp == "L"){	
			p->L = order;
			order++;
		}
		else if(temp == "U"){	
			p->U = order;
			order++;
		}
		else if(temp == "G"){	
			p->G = order;
			order++;
		}
		else if(temp == "N"){	
			p->N = order;
			order++;
		}
		else if(temp == "H"){	
			p->H = order;
			order++;
		}
		else if(temp == "S"){	
			p->S = order;
			order++;
		}
	}


	return type;
}


//vyhledavani v souboru /etc/passwd
void get_user_info(Tparam *p, int type){

	string response;
	ifstream passwd ("pass");
	string line;
	unsigned pos;
	string temp;
	int n_of_records_to_send = 0;
 	while(getline(passwd, line)) {	//nacita radek po radku ze souboru /etc/passwd
		for(int k=0; k<p->count_records; k++){	//na kazdem radku zjistuje shodu se zadanymi loginy/uid
			if(type == SEARCH_BY_LOGIN){	//resi hledani podle loginu
				temp = p->login[k];
				temp+= ":";
				if(line.find(temp) != string::npos){
					pos = line.find(temp);
					if(pos == 0){
						p->record[2*k] = line;
						n_of_records_to_send++;
					}
				}
			}
			else if(type == SEARCH_BY_UID){ //uid
				temp = p->uid[k];
				temp = "*:" + temp + ":";
				if(line.find(temp) != string::npos){
					n_of_records_to_send++;
					if(p->record[2*k] == "")
						p->record[2*k] = line; //kvuli moznosti, ze 1 uid maji 2 zaznamy je zde pole zaznamu pouzivano rozsirene
					else
						p->record[2*k+1] = line; 	
				}
			}
		}
	}
	
 
	return;
	
}

//zpracovani parametru serveru
int get_param(int argc, char **argv){

	int port;

	if((argc == 2) and (strcmp(argv[1], "--help") == 0))
		print_help();
	else if((argc == 3) and (strcmp(argv[1], "-p") == 0))
		port = atoi(argv[2]);
	else{
		cerr << "Chybne parametry. Zkuste --help." << endl;
		exit(10);
	}
	return port;

}

//podle klientovych pozadavku na informace v zaznamu zpracuje nacteny radek ze souboru /etc/passwd
void parse_records(Tparam *p, int type){



	string temp;
	string result = "";
	unsigned pos = 0;
	string l,u,g,n,h,s;
	for(int k=0; k<2*p->count_records; k++){
		if((p->record[k] == "") and (k%2 == 0)){
			if(type == SEARCH_BY_LOGIN){
				if(k != 0)
					p->record[k] = "Chyba: neznamy login " + p->login[k/2];
				else
					p->record[k] = "Chyba: neznamy login " + p->login[0];
			}
			else if(type == SEARCH_BY_UID){
				if(k != 0)
					p->record[k] = "Chyba: nezname uid " + p->uid[k/2];
				else
					p->record[k] = "Chyba: nezname uid " + p->uid[0];
			}

			continue;
		}

		if(p->record[k] == "")
			continue;	

		
		// roztridi zaznam do promennych
		// napr. login:*:uid:gid:gecos:domovsky_adresar:shell => l=login, u=uid, g=gid, ...
		temp = p->record[k];
		pos = temp.find(":");
		l = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		u = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		g = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		n = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		h = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		pos = temp.find(":");
		s = temp.substr(0,pos);
		temp = temp.substr(pos+1);
		// /roztrideni

		for(int j=1; j<=NUMBER_OF_PARAMETERS; j++){  //vytvori vysledek v danem poradi
			if(p->L == j) result += l + " ";
			if(p->U == j) result += u + " ";
			if(p->G == j) result += g + " ";
			if(p->N == j) result += n + " ";
			if(p->H == j) result += h + " ";
			if(p->S == j) result += s + " ";
		}
		p->record[k] = result;
		result = "";
	}

	return;
	
}

//tvorba odpovedi
string build_response(Tparam *p){

	string response = "";
	for(int i=0; i<2*p->count_records; i++){
		if(p->record[i] == "")
			continue;
		response += p->record[i];
		response += "\n";
	}
	response = response.substr(0, response.size()-1);

	return response;

}
void reaper(int s){
	
	wait(&s);

}	

int main(int argc, char **argv){

	int mysocket, sinlen, acc;	
	char msg[CHAR_ARRAY_LENGTH];
	string response;
	const char *r;
	int port = 0;
	int type = 0;
	Tparam param;
	signal(SIGCHLD,reaper);
	port = get_param(argc, argv);

	
	if((mysocket = socket(AF_INET, SOCK_STREAM,0)) < 0){
		cerr << "Selhala operace socket." << endl;
		return -10;
	}

	//vyplneni struktury pro bind
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;	
	
	if(bind(mysocket, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		cerr << "Operace bind selhala." << endl;
		return -10;
	}

	//fronta posluchacu, maximalne pro 5
	if(listen(mysocket, 5)){
		cerr << "Chyba pri operaci listen." << endl;
		return -10;
	}
	
	sockaddr_in client_sin;
	sinlen = sizeof(client_sin);
	int pid;


//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	while(1){

		memset(msg, 0, 1024);
		//accept ceka, dokud klient nezasle pozadavek, take vytvori zvlastni socket pro klienta
		if((acc = accept(mysocket, (struct sockaddr *)&client_sin, (socklen_t *)&sinlen)) < 0){
			cerr << "Accept selhal." << endl;
			return -2;
		}
		
		//pozadavky obsluhuji child procesy
		if((pid = fork()) < 0){
			cerr << "Fork() se nezdaril." << endl;
			exit(EXIT_FAILURE);
		}
		else if(pid == 0){

			if(close(mysocket) < 0){
				cerr << "Nepodarilo se uzavrit rodicovsky socket." << endl;
				return -1;	
			}	

			
			if(read(acc, msg, sizeof(msg)) < 0){
				cerr << "S: Nepodarilo se ziskat zpravu od klienta." << endl;
			} 
			else{
				init_struct(&param); //inicializuje ("vynuluje") strukturu pozadavku
				type = fill_req_struct(&param,msg); //naplni strukturu pozadavku, podle prijateho requestu
				get_user_info(&param, type); //vyhleda informace v /etc/passwd
				parse_records(&param, type); //zpracuje vyhledane zaznamy
				response = build_response(&param); //vytvori odpoved
				r = response.c_str();
				if(write(acc, r, strlen(r)) < 0){
					cerr << "Operace write selhala." << endl;
				}
			}

			exit(0);				

		}
		else{
			if(close(acc) < 0){
				cerr << "Nepodarilo se uzavrit socket." << endl;
				return -1;
			}	

		}
	}	

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

	return 0;
}


