#ifndef GUARD_SPECTRE_H
#define GUARD_SPECTRE_H

#define CACHE_LINE_SIZE 512// 64 // 128

#define BYTE 256
#define ARRAY1_SIZE 128
#define ARRAY2_SIZE BYTE*CACHE_LINE_SIZE


#define THRESSHOLD 150//80 // 120
#define TRIES 5000 // A menos mas rapido pero menos consistente
#define LEVELS 2


/* ------------ Victima ------------*/
#define STR_SIZE 20  

int array1_size; // Siempre contiene el limite de acceso de getData

int pading[CACHE_LINE_SIZE*4]; // Para que al acceder a otros datos no se traigan los del array2
char array2[ARRAY2_SIZE];

int pading2[CACHE_LINE_SIZE*4];
char array1[ARRAY1_SIZE] = {0};

int pading3[CACHE_LINE_SIZE*4];

char aux;

void getData(size_t offset);


/* ---------------------------------*/


inline unsigned int time(void * adr) __attribute__((always_inline));

int probe(char *adr);

inline void misstrain(size_t index) __attribute__((always_inline));

void acceso_virtual(unsigned int datos[][LEVELS],unsigned int * timing);

int main(int argc, char* argv[]);

#endif
