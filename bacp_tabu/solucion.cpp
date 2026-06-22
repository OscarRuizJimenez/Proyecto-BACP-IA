#include "bacp.h"
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <stdexcept>

using namespace std;

// ============================================================
//  Función de evaluación (fitness)
//
//  Objetivo: minimizar el desbalance de la carga académica
//  entre semestres, medido como la suma de desviaciones
//  cuadradas respecto a la carga semestral promedio (modelo
//  matemático del informe, Sección 4.3):
//
//      f(x) = sum_{j=1..N} ( C_j - C_media )^2
//
//  Una solución con f(x) = 0 tiene carga perfectamente
//  balanceada.  Soluciones INFACTIBLES reciben una penalización
//  proporcional a las violaciones.
// ============================================================

double calcular_fitness(const Solucion& sol, const Instancia& inst) {
    int N = inst.n_semestres;
    vector<double> carga(N + 1, 0.0);   // indexado 1..N
    vector<int>    count(N + 1, 0);

    for (int i = 0; i < inst.n_asignaturas; i++) {
        int s = sol.semestre[i];
        carga[s] += inst.creditos[i];
        count[s]++;
    }

    // --- Suma de desviaciones cuadradas respecto al promedio ---
    double media = 0.0;
    for (int s = 1; s <= N; s++) media += carga[s];
    media /= N;

    double desbalance = 0.0;
    for (int s = 1; s <= N; s++)
        desbalance += (carga[s] - media) * (carga[s] - media);

    // --- Penalización por violaciones de restricciones ---
    const double PESO_PENALIZACION = 1000.0;
    double penalizacion = 0.0;

    for (int s = 1; s <= N; s++) {
        // Carga fuera de rango
        if (carga[s] < inst.carga_min)
            penalizacion += PESO_PENALIZACION * (inst.carga_min - carga[s]);
        if (carga[s] > inst.carga_max)
            penalizacion += PESO_PENALIZACION * (carga[s] - inst.carga_max);
        // Cantidad de asignaturas fuera de rango
        if (count[s] < inst.asign_min)
            penalizacion += PESO_PENALIZACION * (inst.asign_min - count[s]);
        if (count[s] > inst.asign_max)
            penalizacion += PESO_PENALIZACION * (count[s] - inst.asign_max);
    }

    // Violaciones de prerrequisito: prereq debe estar en semestre anterior
    for (int i = 0; i < inst.n_asignaturas; i++) {
        for (int pre : inst.prereq[i]) {
            if (sol.semestre[pre] >= sol.semestre[i]) {
                penalizacion += PESO_PENALIZACION;
            }
        }
    }

    return desbalance + penalizacion;
}


//  Verificación de factibilidad
bool es_factible(const Solucion& sol, const Instancia& inst) {
    int N = inst.n_semestres;
    vector<double> carga(N + 1, 0.0);
    vector<int>    count(N + 1, 0);

    for (int i = 0; i < inst.n_asignaturas; i++) {
        int s = sol.semestre[i];
        if (s < 1 || s > N) return false;
        carga[s] += inst.creditos[i];
        count[s]++;
    }

    for (int s = 1; s <= N; s++) {
        if (carga[s] < inst.carga_min || carga[s] > inst.carga_max) return false;
        if (count[s] < inst.asign_min || count[s] > inst.asign_max) return false;
    }

    for (int i = 0; i < inst.n_asignaturas; i++)
        for (int pre : inst.prereq[i])
            if (sol.semestre[pre] >= sol.semestre[i]) return false;

    return true;
}

// ============================================================
//  Grafo inverso de prerrequisitos
//
//  inst.prereq[i] lista los PREDECESORES de i (deben ir antes).
//  sucesores[i] lista las asignaturas q que tienen a i como
//  prerrequisito (deben ir DESPUÉS).  Se usa para acotar el
//  vecindario del movimiento Move (ventana de precedencia).
// ============================================================
vector<vector<int>> construir_sucesores(const Instancia& inst) {
    vector<vector<int>> sucesores(inst.n_asignaturas);
    for (int i = 0; i < inst.n_asignaturas; i++)
        for (int pre : inst.prereq[i])
            sucesores[pre].push_back(i);
    return sucesores;
}

// ============================================================
//  Generación de solución inicial (greedy topológico ALEATORIZADO)
//
//  Estrategia (atiende la Retroalimentación: "agregar aleatoriedad
//  al greedy"):
//  1. Calcular el "nivel mínimo" de cada asignatura usando BFS
//     topológico: una asignatura sin prerrequisitos tiene nivel 1;
//     si tiene prerrequisitos, su nivel = max(nivel_prereqs) + 1.
//  2. Ordenar asignaturas por nivel ascendente, BARAJANDO el orden
//     dentro de un mismo nivel (rompe el determinismo).
//  3. Asignar mediante una Lista Restringida de Candidatos (RCL,
//     estilo GRASP): entre los semestres factibles (>= nivel, sin
//     violar carga_max ni asign_max) se toma al azar uno de los
//     menos cargados (mejor fracción 'alpha'), favoreciendo el
//     balance pero diversificando los puntos de partida.
//
//  Distintas semillas producen soluciones iniciales distintas, lo
//  que habilita el multi-arranque de la Búsqueda Tabú.
// ============================================================

// Calcula nivel mínimo de cada asignatura por topología
static vector<int> calcular_niveles(const Instancia& inst) {
    int n = inst.n_asignaturas;
    vector<int> nivel(n, 1);
    // iteraciones hasta convergencia (máx n veces)
    bool cambio = true;
    for (int iter = 0; iter < n && cambio; iter++) {
        cambio = false;
        for (int i = 0; i < n; i++) {
            for (int pre : inst.prereq[i]) {
                if (nivel[pre] + 1 > nivel[i]) {
                    nivel[i] = nivel[pre] + 1;
                    cambio = true;
                }
            }
        }
    }
    return nivel;
}

// Longitud de la cadena más larga de SUCESORES bajo cada asignatura: cola[i] =
// cuántos semestres deben venir necesariamente después de i.  Una asignatura
// debe ubicarse en un semestre s con s + cola[i] <= N para que toda su cadena
// de sucesores quepa.  Acotar por esto evita que el greedy empuje un curso tan
// tarde que sus sucesores ya no quepan (lo que generaría violaciones de
// prerrequisito imposibles de reparar con el vecindario restringido).
static vector<int> calcular_cola(const Instancia& inst) {
    int n = inst.n_asignaturas;
    vector<int> cola(n, 0);
    bool cambio = true;
    for (int iter = 0; iter < n && cambio; iter++) {
        cambio = false;
        for (int i = 0; i < n; i++)
            for (int pre : inst.prereq[i])           // arista pre -> i
                if (cola[i] + 1 > cola[pre]) { cola[pre] = cola[i] + 1; cambio = true; }
    }
    return cola;
}

Solucion generar_solucion_inicial(const Instancia& inst, mt19937& rng, double alpha) {
    int n = inst.n_asignaturas;
    int N = inst.n_semestres;

    Solucion sol;
    sol.semestre.resize(n, 0);

    // Niveles mínimos por prerrequisito y cadena de sucesores (cota superior)
    vector<int> nivel = calcular_niveles(inst);
    vector<int> cola  = calcular_cola(inst);

    // Orden de asignación: nivel ascendente.  El desempate dentro de un
    // mismo nivel es ALEATORIO (se baraja), no determinista.
    vector<int> orden(n);
    iota(orden.begin(), orden.end(), 0);
    shuffle(orden.begin(), orden.end(), rng);
    stable_sort(orden.begin(), orden.end(), [&](int a, int b) {
        return nivel[a] < nivel[b];
    });

    vector<double> carga_sem(N + 1, 0.0);
    vector<int>    count_sem(N + 1, 0);

    for (int i : orden) {
        // Semestre mínimo válido: estrictamente después de la posición REAL de
        // todos los prerrequisitos ya asignados (no solo de su nivel teórico).
        // Esto garantiza que la solución inicial nunca viola prerrequisitos.
        int sem_minimo = nivel[i];
        for (int pre : inst.prereq[i])
            sem_minimo = max(sem_minimo, sol.semestre[pre] + 1);
        // Semestre máximo: deja espacio para la cadena de sucesores (s + cola <= N).
        int sem_maximo = N - cola[i];
        if (sem_minimo > N)        sem_minimo = N;          // salvaguarda
        if (sem_maximo < sem_minimo) sem_maximo = sem_minimo; // salvaguarda

        // Reunir los semestres factibles (en [sem_minimo, sem_maximo], sin exceder
        // cotas máximas) junto con su carga actual, para construir la RCL.
        vector<pair<double,int>> candidatos;  // (carga_actual, semestre)
        for (int s = sem_minimo; s <= sem_maximo; s++) {
            bool ok_carga = (carga_sem[s] + inst.creditos[i] <= inst.carga_max);
            bool ok_count = (count_sem[s] + 1 <= inst.asign_max);
            if (ok_carga && ok_count)
                candidatos.push_back({carga_sem[s], s});
        }

        int elegido;
        if (!candidatos.empty()) {
            // RCL: ordenar por carga ascendente y elegir al azar entre la
            // mejor fracción 'alpha' (al menos 1 candidato).
            sort(candidatos.begin(), candidatos.end());
            int tam_rcl = max(1, (int)(alpha * candidatos.size()));
            uniform_int_distribution<int> dist(0, tam_rcl - 1);
            elegido = candidatos[dist(rng)].second;
        } else {
            // Si no cabe en ningún semestre válido, forzar al mínimo posible
            // (solución infactible que la Búsqueda Tabú reparará).
            elegido = sem_minimo;
        }

        sol.semestre[i] = elegido;
        carga_sem[elegido] += inst.creditos[i];
        count_sem[elegido]++;
    }

    sol.fitness = calcular_fitness(sol, inst);
    return sol;
}

//  Impresión de la solución
void imprimir_solucion(const Solucion& sol, const Instancia& inst) {
    int N = inst.n_semestres;
    vector<double> carga(N + 1, 0.0);
    vector<int>    count(N + 1, 0);

    cout << "\n===== SOLUCION =====\n";
    for (int s = 1; s <= N; s++) {
        cout << "Semestre " << s << ": ";
        bool primero = true;
        for (int i = 0; i < inst.n_asignaturas; i++) {
            if (sol.semestre[i] == s) {
                if (!primero) cout << ", ";
                cout << inst.nombres[i] << "(" << inst.creditos[i] << ")";
                primero = false;
                carga[s] += inst.creditos[i];
                count[s]++;
            }
        }
        cout << "\n  -> carga=" << carga[s]
             << "  asignaturas=" << count[s] << "\n";
    }

    cout << "\nFitness (desbalance: suma de desviaciones cuadradas): " << sol.fitness << "\n";
    cout << "Factible: " << (es_factible(sol, inst) ? "SI" : "NO") << "\n";
}
