// El siguiente bloque ifdef muestra la forma estándar de crear macros que facilitan 
// la exportación de archivos DLL. Todos los archivos de este archivo DLL se compilan con el símbolo RANAS_EXPORTS
// definido en la línea de comandos. Este símbolo no se debe definir en ningún proyecto
// que utilice este archivo DLL. De este modo, otros proyectos cuyos archivos de código fuente incluyan el archivo
// interpreta que las funciones RANAS_API se importan de un archivo DLL, mientras que este archivo DLL interpreta los símbolos
// definidos en esta macro como si fueran exportados.
#ifdef RANAS_EXPORTS
#define RANAS_API __declspec(dllexport)
#else
#define RANAS_API __declspec(dllimport)
#endif

#define PERROR(a) \
    {             \
        LPVOID lpMsgBuf;                                         \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |           \
                   FORMAT_MESSAGE_FROM_SYSTEM |                  \
                   FORMAT_MESSAGE_IGNORE_INSERTS, NULL,          \
                   GetLastError(),                               \
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),    \
                   (LPTSTR) &lpMsgBuf,0,NULL );                  \
        fprintf(stderr,"%s:(%d)%s\n",a,GetLastError(),lpMsgBuf); \
        LocalFree( lpMsgBuf );                                   \
    }    

#define DERECHA		0
#define IZQUIERDA	1
#define ARRIBA		2

extern RANAS_API int nranas;

typedef BOOL(*TIPO_AVANCERANA)(int*, int*, int);
typedef BOOL(*TIPO_AVANCERANAFIN) (int, int);
typedef BOOL(*TIPO_AVANCERANAINI) (int, int);
typedef BOOL(*TIPO_AVANCETRONCOS)(int);
typedef BOOL(*TIPO_COMPROBARESTADISTICAS)(LONG, LONG, LONG);
typedef void (*TIPO_CRIAR)(int);
typedef BOOL(*TIPO_FINRANAS)(void);
typedef BOOL(*TIPO_INICIORANAS)(int, int*, int*, int*, int, TIPO_CRIAR);
typedef BOOL(*TIPO_PARTORANAS)(int);
typedef void (*TIPO_PAUSA)(void);
typedef BOOL(*TIPO_PUEDOSALTAR)(int, int, int);
typedef void (*TIPO_PRINTMSG)(char*);

#ifdef RANAS_EXPORTS
extern "C" RANAS_API BOOL AvanceRana(int*, int*, int);
extern "C" RANAS_API BOOL AvanceRanaFin(int, int);
extern "C" RANAS_API BOOL AvanceRanaIni(int, int);
extern "C" RANAS_API BOOL AvanceTroncos(int i);
extern "C" RANAS_API BOOL ComprobarEstadIsticas(LONG, LONG, LONG);
extern "C" RANAS_API BOOL FinRanas(void);
extern "C" RANAS_API BOOL InicioRanas(int, int*, int*, int*, int, TIPO_CRIAR);
extern "C" RANAS_API BOOL PartoRanas(int);
extern "C" RANAS_API void Pausa(void);
extern "C" RANAS_API BOOL PuedoSaltar(int, int, int);
extern "C" RANAS_API void PrintMsg(char*);
#endif
