#include <stdlib.h>
#include "stdio.h"
#include "windows.h"
#include "stdlib.h"
#include "time.h"
#include "ranas.h"

#define MIN_VELOCIDAD (const int)0
#define MAX_VELOCIDAD (const int)1000
#define MIN_TMEDIO (const int)1
#define TIEMPO (const int)30000
#define MOVIMIENTO_TRONCOS (const int)0
#define FILA_ULTIMO_TRONCO (const int)10
#define FILA_PRIMER_TRONCO (const int)4
#define NUM_PROCESOS (const int)24
#define POSX_MAX (const int)79
#define POSX_MIN (const int)0
#define NUM_TRONCOS (const int)7
BOOL(*AVANCERANA)(int*, int*, int);
BOOL(*AVANCERANAFIN)(int, int);
BOOL(*AVANCERANAINI) (int, int);
BOOL(*AVANCETRONCOS)(int);
BOOL(*COMPROBARESTADISTICAS)(LONG, LONG, LONG);
BOOL(*INICIORANAS)(int, int*, int*, int*, int, TIPO_CRIAR);
BOOL(*PARTORANAS)(int);
BOOL(*PUEDOSALTAR)(int, int, int);
void(*PRINTMSG)(char*);
static void nlog(char* str, ...);
void liberarPIDActual();
void guardarPID(DWORD pid);
void func_criar(int i);
DWORD WINAPI bucleRanasHija(LPVOID i);
DWORD WINAPI manejadorTiempo(LPVOID t);
DWORD WINAPI manejadorTroncos(LPVOID i);
HANDLE semNacidas;
HANDLE semSalvadas;
HANDLE semPerdidas;
HANDLE eventoFinalizacion;
HANDLE eventoMovimientoTronco[7];
CRITICAL_SECTION SC_PRIMERMOVIMIENTO[4];
HANDLE semPrimerMovimiento[4];
CRITICAL_SECTION SC_SALTORANAS;
HANDLE SEM_SALTORANAS;
CRITICAL_SECTION SC_ARRAYPID;
HANDLE semPosiciones;
HANDLE semProcesos;
FARPROC PAUSA;
FARPROC FINRANAS;

typedef struct pos {
	int x, y; //Quizas se podría usar unsigned char porque ocupan el espacio mínimo de C (1 byte)
			// y es suficiente para CUALQUIER posicion (las Y de 0...11 y las X de 0...80).
			// sin embargo, por compatibilidad con la librería usamos int.
} pos;

typedef struct {
	long contadorNacidas;
	long contadorSalvadas;
	long contadorMuertas;
	pos arrayPosiciones[NUM_PROCESOS * 2]; //Guardamos suficientes posiciones para el supuesto de un tablero ABSOLUTAMENTE lleno.
								//De esta manera, nos libramos de hacer mallocs y otros problemas.
	DWORD arrayPID[(NUM_PROCESOS * 2)];
} datosCompartidos;
int vectorDirs[7] = { DERECHA,IZQUIERDA,DERECHA,IZQUIERDA,DERECHA,IZQUIERDA,DERECHA };
datosCompartidos datosCompartida;

int main(int argc, char* argv[])
{	
	int direcciones[2] = { DERECHA, IZQUIERDA };
	//int vectorTroncos[7] = { 6,7,8,9,10,11,12 };
	int vectorTroncos[7] = { 11,11,11,11,11,11,11 };
	int vectorAgua[7] = { 7,6,5,4,3,2,1 };
	int velocidad, tmedio;
	int temp;
	HANDLE ret;
	srand(time(NULL));
	for(int x = 0; x < NUM_TRONCOS; x++){
		vectorTroncos[x] = 2 * (rand() % 11 + 1);
		temp = rand() % 2;
		vectorDirs[x] = direcciones[temp];
		/*if (temp == DERECHA) {
			nlog((char*)"VectorTroncos [%d]: DERECHA", x);
		}
		else {
			nlog((char *)"VectorTroncos [%d]: IZQUIERDA", x);
		}
		*/
	}

	datosCompartida.contadorSalvadas = 0;
	datosCompartida.contadorMuertas = 0;
	datosCompartida.contadorNacidas = 0;
	
	for (int i = 0; i < NUM_PROCESOS * 2; i++) {
		datosCompartida.arrayPosiciones[i].x = -1;
		datosCompartida.arrayPosiciones[i].y = -1;
		datosCompartida.arrayPID[i] = -1;
	}
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

	//INICIALIZACION IPCS
	InitializeCriticalSection(&SC_SALTORANAS);
	InitializeCriticalSection(&SC_ARRAYPID);
	eventoFinalizacion = CreateEvent(NULL, TRUE, FALSE, NULL);
	semNacidas = CreateSemaphore(NULL, 1, 1, NULL);
	semPerdidas = CreateSemaphore(NULL, 1, 1, NULL);
	semSalvadas = CreateSemaphore(NULL, 1, 1, NULL);
	semProcesos = CreateSemaphore(NULL, NUM_PROCESOS, NUM_PROCESOS, NULL);
	SEM_SALTORANAS = CreateSemaphore(NULL, 1, 1, NULL);
	for (int i = 0; i < 7; i++) {
		eventoMovimientoTronco[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
	}
	for (int i = 0; i < 4; i++) {
		semPrimerMovimiento[i] = CreateSemaphore(NULL, 1, 1, NULL);
		InitializeCriticalSection(&(SC_PRIMERMOVIMIENTO[i]));
	}


	//CARGA DE LIBRERIA Y FUNCIONES
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
	CreateThread(NULL, 0, manejadorTiempo, (LPVOID)TIEMPO, NULL, NULL);
	ret = CreateThread(NULL, 0, manejadorTroncos, NULL, NULL, NULL);
	guardarPID(GetThreadId(ret));
	
	WaitForSingleObject(eventoFinalizacion, INFINITE);
	for (int i = 0; i < NUM_PROCESOS * 2; i++) {
		if (datosCompartida.arrayPID[i] != -1) {
			//nlog((char *)"Esperando por el hilo 0x%x", GetThreadId(datosCompartida.arrayPID[i]));
			WaitForSingleObject(OpenThread(THREAD_ALL_ACCESS, TRUE, datosCompartida.arrayPID[i]), INFINITE);
			//nlog((char*)"Espera finalizada");
		}
	}
	//nlog((char*)"Fin de esperas");
	FINRANAS();
	COMPROBARESTADISTICAS(datosCompartida.contadorNacidas, datosCompartida.contadorSalvadas, datosCompartida.contadorMuertas);
	if (semProcesos != NULL) CloseHandle(semProcesos);
	if (semNacidas != NULL) CloseHandle(semNacidas);
	if (semPerdidas != NULL) CloseHandle(semPerdidas);
	for (int i = 0; i < 4; i++) {
		if (semPrimerMovimiento[i] != NULL)
			CloseHandle(semPrimerMovimiento[i]);
	}

	if (eventoFinalizacion != NULL) CloseHandle(eventoFinalizacion);
	DeleteCriticalSection(&SC_SALTORANAS);
	FreeLibrary(libreria);
	nlog((char*)"Funcionamiento correcto.");
	return 0;
}
DWORD WINAPI manejadorTroncos(LPVOID i) {
	int j, ultimoEvento = 0, k;
	while (1) {
		for (j = 0; j < 7; j++) {
			if (WaitForSingleObject(eventoFinalizacion, 0) == WAIT_OBJECT_0) { // La macro indica que el evento está señalado
			//Además, esperar 0 milisegundos hace que se compruebe el estado del evento y se resuma la ejecución automáticamente.
				liberarPIDActual();
				return 0;
			}
			//EnterCriticalSection(&SC_SALTORANAS);
			WaitForSingleObject(SEM_SALTORANAS, INFINITE);
			/*
			if (vectorDirs[j] == DERECHA) {
				nlog((char *)"Moviendo tronco %d hacia la DERECHA.", j);
			}
			else {
				nlog((char*)"Moviendo tronco %d hacia la IZQUIERDA.", j);
			}
			*/
			AVANCETRONCOS(j);
			for (k = 0; k < NUM_PROCESOS * 2; k++) {
				if (datosCompartida.arrayPosiciones[k].x < 0 && datosCompartida.arrayPosiciones[k].y < 0) {
					continue;
				}
				if (datosCompartida.arrayPosiciones[k].y == ((6 - j) + FILA_PRIMER_TRONCO)) {
					if (vectorDirs[j] == DERECHA) {
						datosCompartida.arrayPosiciones[k].x += 1;
						//nlog((char*)"Moviendo rana de [%d, %d] a [%d, %d]", datosCompartida.arrayPosiciones[k].x-1, datosCompartida.arrayPosiciones[k].y, datosCompartida.arrayPosiciones[k].x, datosCompartida.arrayPosiciones[k].y);
					}
					else {
						datosCompartida.arrayPosiciones[k].x -= 1;
						//nlog((char*)"Moviendo rana de [%d, %d] a [%d, %d]", datosCompartida.arrayPosiciones[k].x + 1, datosCompartida.arrayPosiciones[k].y, datosCompartida.arrayPosiciones[k].x, datosCompartida.arrayPosiciones[k].y);
					}
				}
			}
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
			//LeaveCriticalSection(&SC_SALTORANAS);
			PAUSA();
		}
	}
}
DWORD WINAPI manejadorTiempo(LPVOID t) {
	Sleep((DWORD)t);
	SetEvent(eventoFinalizacion);
	return 0;
}

void func_criar(int i) {
	if (WaitForSingleObject(eventoFinalizacion, 0) == WAIT_OBJECT_0) { // La macro indica que el evento está señalado
		ExitThread(0);
	}
	WaitForSingleObject(semProcesos, INFINITE);
	WaitForSingleObject(semPrimerMovimiento[i], INFINITE);
	//EnterCriticalSection(&(SC_PRIMERMOVIMIENTO[i]));
	//nlog((char*)"Entrando en seccion critica %d", i);
	if (PARTORANAS(i)) {
		//nlog((char*)"Pariendo hijo en %d", i);
		WaitForSingleObject(semNacidas, INFINITE);
		datosCompartida.contadorNacidas += 1;
		ReleaseSemaphore(semNacidas, 1, NULL);
		HANDLE hija = CreateThread(NULL, 0, bucleRanasHija, (LPVOID)i, 0, NULL);
		guardarPID(GetThreadId(hija));
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
	int indicePosicion = 0;
	int dirs[] = { IZQUIERDA, DERECHA };
	for (int i = 0; i < NUM_PROCESOS * 2; i++) {
		if (datosCompartida.arrayPosiciones[i].x < 0 || datosCompartida.arrayPosiciones[i].y < 0) {
			indicePosicion = i;
			datosCompartida.arrayPosiciones[i].x = posX;
			datosCompartida.arrayPosiciones[i].y = posY;
			break;
		}
	}
	while (1) {
		if (WaitForSingleObject(eventoFinalizacion, 0) == WAIT_OBJECT_0) { // La macro indica que el evento está señalado
			//Además, esperar 0 milisegundos hace que se compruebe el estado del evento y se resuma la ejecución automáticamente.
			datosCompartida.arrayPosiciones[indicePosicion].x = -1;
			datosCompartida.arrayPosiciones[indicePosicion].y = -1;
			ReleaseSemaphore(semProcesos, 1, NULL);
			liberarPIDActual();
			ExitThread(0);
		}
		rnd = rand() % 2;
		WaitForSingleObject(SEM_SALTORANAS, INFINITE);
		//EnterCriticalSection(&SC_SALTORANAS);
		if (datosCompartida.arrayPosiciones[indicePosicion].x < POSX_MIN || datosCompartida.arrayPosiciones[indicePosicion].x > POSX_MAX) {
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
			WaitForSingleObject(semPerdidas, INFINITE);
			datosCompartida.contadorMuertas += 1;
			ReleaseSemaphore(semPerdidas, 1, NULL);
			datosCompartida.arrayPosiciones[indicePosicion].x = -1;
			datosCompartida.arrayPosiciones[indicePosicion].y = -1;
			//LeaveCriticalSection(&SC_SALTORANAS);
			ReleaseSemaphore(semProcesos, 1, NULL);
			liberarPIDActual();
			ExitThread(0);
		}
		if (PUEDOSALTAR(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y, ARRIBA)) {
			AVANCERANAINI(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			AVANCERANA(&(datosCompartida.arrayPosiciones[indicePosicion].x), &datosCompartida.arrayPosiciones[indicePosicion].y, ARRIBA);
			PAUSA();
			AVANCERANAFIN(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
			//LeaveCriticalSection(&SC_SALTORANAS);
			if (esPrimerMovimiento) {
				//LeaveCriticalSection(&(SC_PRIMERMOVIMIENTO[(int)i]));
				ReleaseSemaphore(semPrimerMovimiento[(int)i], 1, NULL);
				esPrimerMovimiento = FALSE;
				//nlog((char*)"Se ha salido de seccion critica %d", (int)i);
			}
		}
		else if (PUEDOSALTAR(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y, dirs[rnd])) {
			AVANCERANAINI(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			AVANCERANA(&(datosCompartida.arrayPosiciones[indicePosicion].x), &datosCompartida.arrayPosiciones[indicePosicion].y, dirs[rnd]);
			PAUSA();
			AVANCERANAFIN(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			//LeaveCriticalSection(&SC_SALTORANAS);
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
		}
		else if (PUEDOSALTAR(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y, dirs[1 - rnd])) {
			AVANCERANAINI(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			AVANCERANA(&(datosCompartida.arrayPosiciones[indicePosicion].x), &datosCompartida.arrayPosiciones[indicePosicion].y, dirs[1 - rnd]);
			PAUSA();
			AVANCERANAFIN(datosCompartida.arrayPosiciones[indicePosicion].x, datosCompartida.arrayPosiciones[indicePosicion].y);
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
		}
		else {
			//LeaveCriticalSection(&SC_SALTORANAS);
			ReleaseSemaphore(SEM_SALTORANAS, 1, NULL);
			PAUSA();
		}
		if (datosCompartida.arrayPosiciones[indicePosicion].y > FILA_ULTIMO_TRONCO) {
			WaitForSingleObject(semSalvadas, INFINITE);
			datosCompartida.contadorSalvadas += 1;
			ReleaseSemaphore(semSalvadas, 1, NULL);
			datosCompartida.arrayPosiciones[indicePosicion].x = -1;
			datosCompartida.arrayPosiciones[indicePosicion].y = -1;
			ReleaseSemaphore(semProcesos, 1, NULL);
			liberarPIDActual();
			ExitThread(0);
		}
	}
}

static void nlog(char* str, ...)
{
	HWND notepad, edit;
	va_list ap;
	char buf[256];
	notepad = FindWindow(NULL, "*Sin título: Bloc de notas");
	if (notepad == NULL) return;
	edit = FindWindowEx(notepad, NULL, "EDIT", NULL);
	va_start(ap, str);
	vsprintf(buf, str, ap);
	va_end(ap);
	strcat(buf, "\r\n");
	SendMessage(edit, EM_REPLACESEL, TRUE, (LPARAM)buf);
}

void liberarPIDActual() {
	EnterCriticalSection(&SC_ARRAYPID);
	for (int i = 0; i < NUM_PROCESOS * 2; i++) {
		if (datosCompartida.arrayPID[i] == GetCurrentThreadId()) {
			nlog((char *)"Liberado PID 0x%x", GetCurrentThreadId());
			datosCompartida.arrayPID[i] = -1;
			break;
		}
	}
	LeaveCriticalSection(&SC_ARRAYPID);
}

void guardarPID(DWORD pid) {
	EnterCriticalSection(&SC_ARRAYPID);
	for (int i = 0; i < NUM_PROCESOS * 2; i++) {
		if (datosCompartida.arrayPID[i] == -1) {
			datosCompartida.arrayPID[i] = pid;
			break;
		}
	}
	LeaveCriticalSection(&SC_ARRAYPID);
}