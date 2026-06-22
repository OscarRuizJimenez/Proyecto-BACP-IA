#ifndef BACP_H
#define BACP_H

#include <vector>
#include <string>
#include <random>

using namespace std;

// ============================================================
//  Estructura de la instancia BACP
// ============================================================
struct Instancia {
    int n_asignaturas;   // cantidad de asignaturas
    int n_semestres;     // número máximo de semestres (p)
    int carga_min;       // carga académica mínima por semestre (a)
    int carga_max;       // carga académica máxima por semestre (b)
    int asign_min;       // asignaturas mínimas por semestre (c)
    int asign_max;       // asignaturas máximas por semestre (d)

    vector<string>       nombres;   // nombre de cada asignatura
    vector<int>          creditos;  // créditos de cada asignatura
    // prereq[i] contiene los índices de las asignaturas que son
    // prerrequisito de la asignatura i
    vector<vector<int>>  prereq;
};

// ============================================================
//  Solución: asignación de cada asignatura a un semestre
// ============================================================
struct Solucion {
    vector<int> semestre;  // semestre[i] = semestre asignado a asignatura i (1..N)
    double fitness;        // valor de la función objetivo (desbalance: suma de desviaciones cuadradas)
};

// ============================================================
//  Parámetros de la Búsqueda Tabú (identifican una corrida)
// ============================================================
struct ParametrosTabu {
    int      max_iter;        // tope de iteraciones por arranque
    int      max_sin_mejora;  // iteraciones sin mejorar el best antes de cortar
    int      kmin;            // tenencia tabú mínima (iteraciones)
    int      kmax;            // tenencia tabú máxima (iteraciones)
    int      restarts;        // número de arranques (multi-arranque)
    double   alpha;           // fracción de la RCL del greedy aleatorizado (0..1)
    unsigned seed;            // semilla del generador aleatorio (reproducibilidad)
};

ParametrosTabu parametros_por_defecto();

// ============================================================
//  Funciones declaradas
// ============================================================
bool     leer_instancia(const string& archivo, Instancia& inst);
Solucion generar_solucion_inicial(const Instancia& inst, mt19937& rng, double alpha);
double   calcular_fitness(const Solucion& sol, const Instancia& inst);
bool     es_factible(const Solucion& sol, const Instancia& inst);
void     imprimir_solucion(const Solucion& sol, const Instancia& inst);

// Grafo inverso de prerrequisitos: sucesores[i] = asignaturas que tienen a i
// como prerrequisito (necesario para la ventana de precedencia del movimiento Move).
vector<vector<int>> construir_sucesores(const Instancia& inst);

// Núcleo de la metaheurística: Búsqueda Tabú con multi-arranque.
Solucion busqueda_tabu(const Instancia& inst, const ParametrosTabu& par);

#endif
