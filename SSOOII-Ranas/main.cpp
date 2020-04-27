#include <stdlib.h>
#include "stdio.h"
#include "windows.h"
#include "stdlib.h"
#include "time.h"
#include "ranas.h"

#define MIN_VELOCIDAD (const int)0
#define MAX_VELOCIDAD (const int)1000
#define MIN_TMEDIO (const int)1
#define NUM_PROCESOS (const int)25
#define MOVIMIENTO_TRONCOS (const int)0
#define FILA_ULTIMO_TRONCO (const int)10

BOOL(*AVANCERANA)(int*, int*, int);
BOOL(*AVANCERANAFIN)(int, int);
BOOL(*AVANCERANAINI) (int, int);
BOOL(*AVANCETRONCOS)(int);
BOOL(*COMPROBARESTADISTICAS)(LONG, LONG, LONG);
BOOL(*INICIORANAS)(int, int*, int*, int*, int, TIPO_CRIAR);
BOOL(*PARTORANAS)(int);
BOOL(*PUEDOSALTAR)(int, int, int);
void(*PRINTMSG)(char*);

void func_criar(int i);
DWORD WINAPI bucleRanasHija(LPVOID i);

HANDLE semProcesos;
HANDLE semNacidas;
HANDLE semSalvadas;
HANDLE semPerdidas;
CRITICAL_SECTION SC_PRIMERMOVIMIENTO[4];
CRITICAL_SECTION SC_SALTORANAS;
HANDLE semPosiciones;
FARPROC PAUSA;
FARPROC FINRANAS;

int main(int argc, char* argv[])
{
	int velocidad, tmedio;
	//int vectorTroncos[7] = { 6,7,8,9,10,11,12 };
	int vectorTroncos[7] = { 11,11,11,11,11,11,11 };
	int vectorAgua[7] = { 7,6,5,4,3,2,1 };
	int vectorDirs[7] = { DERECHA,IZQUIERDA,DERECHA,IZQUIERDA,DERECHA,IZQUIERDA,DERECHA };
	srand(time(NULL));
	if (argc <= 2) {
		printf("Los argumentos son erroneos. Asegurate de llamarlo de la siguiente manera: \n");
		printf("batracios.exe <velocidad> <tmedio>\n");
		printf("En el caso de estar utilizando Visual Studio, se configura en Propiedades > Depurar > Argumentos de comandos");
		return -1;
	}

	velocidad = atoi(argv[1]);
	tmedio = atoi(argv[2]);

	if (velocidad < MIN_VELOCIDAD || velocidad > MAX_VELOCIDAD) {
		printf("<velocidad> debe estar entre %d y %d.\n", MIN_VELOCIDAD, MAX_VELOCIDAD);
		return 6;
	}
	else if (tmedio <= MIN_TMEDIO) {
		printf("<tiempo medio> debe ser mayor que %d.\n", MIN_TMEDIO);
		return 7;
	}


	semProcesos = CreateSemaphore(NULL, NUM_PROCESOS - 1, NUM_PROCESOS - 1, "SEMAFORO PROCESOS");
	InitializeCriticalSection(&SC_SALTORANAS);
	for (int i = 0; i < 4; i++) {
		InitializeCriticalSection(&(SC_PRIMERMOVIMIENTO[i]));
	}
	HINSTANCE libreria = LoadLibrary("ranas.dll");
	if (libreria == NULL) {
		PERROR("main: LoadLibrary");
		return -2;
	}

	FINRANAS = GetProcAddress(libreria, "FinRanas");
	if (FINRANAS == NULL) {
		PERROR("main: GetProcAddress, FINRANAS")
			return -3;
	}
	PAUSA = GetProcAddress(libreria, "Pausa");
	if (PAUSA == NULL) {
		PERROR("main: GetProcAddress, PAUSA");
		return -3;
	}
	AVANCERANA = (BOOL(*)(int*, int*, int)) GetProcAddress(libreria, "AvanceRana");
	if (AVANCERANA == NULL) {
		PERROR("main: GetProcAddress, AVANCERANA")
			return -3;
	}
	AVANCERANAFIN = (BOOL(*)(int, int)) GetProcAddress(libreria, "AvanceRanaFin");
	if (AVANCERANAFIN == NULL) {
		PERROR("main: GetProcAddress, AVANCERANAFIN");
		return -3;
	}
	AVANCERANAINI = (BOOL(*)(int, int)) GetProcAddress(libreria, "AvanceRanaIni");
	if (AVANCERANAINI == NULL) {
		PERROR("main: GetProcAddress, AVANCERANAINI");
		return -3;
	}
	AVANCETRONCOS = (BOOL(*)(int)) GetProcAddress(libreria, "AvanceTroncos");
	if (AVANCETRONCOS == NULL) {
		PERROR("main: GetProcAddress, AVANCETRONCOS");
		return -3;
	}
	COMPROBARESTADISTICAS = (BOOL(*)(LONG, LONG, LONG)) GetProcAddress(libreria, "ComprobarEstadIsticas");
	if (COMPROBARESTADISTICAS == NULL) {
		PERROR("main: GetProcAddress, COMPROBARESTADISTICAS")
			return -3;
	}
	INICIORANAS = (BOOL(*)(int, int*, int*, int*, int, TIPO_CRIAR)) GetProcAddress(libreria, "InicioRanas");
	if (INICIORANAS == NULL) {
		PERROR("main: GetProcAddress, INICIORANAS")
			return -3;
	}
	PARTORANAS = (BOOL(*)(int)) GetProcAddress(libreria, "PartoRanas");
	if (PARTORANAS == NULL) {
		PERROR("main: GetProcAddress, PARTORANAS")
			return -3;
	}
	PUEDOSALTAR = (BOOL(*)(int, int, int)) GetProcAddress(libreria, "PuedoSaltar");
	if (PUEDOSALTAR == NULL) {
		PERROR("main: GetProcAddress, PUEDOSALTAR")
			return -3;
	}
	PRINTMSG = (VOID(*)(char*)) GetProcAddress(libreria, "PrintMsg");
	if (PRINTMSG == NULL) {
		PERROR("main: GetProcAddress, PRINTMSG")
			return -3;
	}

	INICIORANAS(velocidad, vectorTroncos, vectorAgua, vectorDirs, tmedio, func_criar);

	
	
	while(1);
	return 0;
}

void func_criar(int i) {
	WaitForSingleObject(semProcesos, INFINITE);
	EnterCriticalSection(&(SC_PRIMERMOVIMIENTO[i]));
	if (PARTORANAS(i)) {
		HANDLE hija = CreateThread(NULL, 0, bucleRanasHija, (LPVOID)i, 0, NULL);
		if (hija == NULL) {
			PERROR("func_criar: CreateThread: hija[%d]", i);
		}
	}
}

DWORD WINAPI bucleRanasHija(LPVOID i) {
	int posX = (15 + 16 * (int)i);
	int posY = 0;
	BOOL esPrimerMovimiento = TRUE;
	int rnd;
	int dirs[] = { IZQUIERDA, DERECHA };
	while (1) {
		rnd = rand() % 2;
		EnterCriticalSection(&SC_SALTORANAS);
		if (PUEDOSALTAR(posX, posY, ARRIBA)) {
			AVANCERANAINI(posX, posY);
			AVANCERANA(&posX, &posY, ARRIBA);
			LeaveCriticalSection(&SC_SALTORANAS);
			PAUSA();
			AVANCERANAFIN(posX, posY);
			if (esPrimerMovimiento) {
				esPrimerMovimiento = FALSE;
				LeaveCriticalSection(&(SC_PRIMERMOVIMIENTO[(int)i]));
			}
		}
		else if (PUEDOSALTAR(posX, posY, dirs[rnd])) {
			AVANCERANAINI(posX, posY);
			AVANCERANA(&posX, &posY, dirs[rnd]);
			LeaveCriticalSection(&SC_SALTORANAS);
			PAUSA();
			AVANCERANAFIN(posX, posY);
			if (esPrimerMovimiento) {
				esPrimerMovimiento = FALSE;
				LeaveCriticalSection(&(SC_PRIMERMOVIMIENTO[(int)i]));
			}
		}
		else if (PUEDOSALTAR(posX, posY, dirs[1 - rnd])) {
			AVANCERANAINI(posX, posY);
			AVANCERANA(&posX, &posY, dirs[1 - rnd]);
			LeaveCriticalSection(&SC_SALTORANAS);
			PAUSA();
			AVANCERANAFIN(posX, posY);
			if (esPrimerMovimiento) {
				esPrimerMovimiento = FALSE;
				LeaveCriticalSection((&SC_PRIMERMOVIMIENTO[(int)i]));
			}
		}
		else {
			LeaveCriticalSection(&SC_SALTORANAS);
			PAUSA();
		}
		if (posY > FILA_ULTIMO_TRONCO) {
			ReleaseSemaphore(semProcesos, 1, NULL);
			ExitThread(0);
		}
	}
}