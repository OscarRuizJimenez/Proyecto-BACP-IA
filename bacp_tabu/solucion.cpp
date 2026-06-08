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
//  Generación de solución inicial (heurística greedy por nivel)
//
//  Estrategia:
//  1. Calcular el "nivel mínimo" de cada asignatura usando BFS
//     topológico: una asignatura sin prerrequisitos tiene nivel 1;
//     si tiene prerrequisitos, su nivel = max(nivel_prereqs) + 1.
//  2. Ordenar asignaturas por nivel ascendente.
//  3. Asignar al primer semestre que no viole las cotas de
//     carga ni de cantidad.
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

Solucion generar_solucion_inicial(const Instancia& inst) {
    int n = inst.n_asignaturas;
    int N = inst.n_semestres;

    Solucion sol;
    sol.semestre.resize(n, 0);

    // Niveles mínimos por prerrequisito
    vector<int> nivel = calcular_niveles(inst);

    // Orden de asignación: nivel ascendente, desempate por créditos desc.
    vector<int> orden(n);
    iota(orden.begin(), orden.end(), 0);
    sort(orden.begin(), orden.end(), [&](int a, int b) {
        if (nivel[a] != nivel[b]) return nivel[a] < nivel[b];
        return inst.creditos[a] > inst.creditos[b];
    });

    vector<double> carga_sem(N + 1, 0.0);
    vector<int>    count_sem(N + 1, 0);

    for (int i : orden) {
        int sem_minimo = nivel[i];          // restricción de prerrequisito
        if (sem_minimo > N) sem_minimo = N; // clamp

        bool asignado = false;
        // Intentar desde el semestre mínimo hacia adelante
        for (int s = sem_minimo; s <= N; s++) {
            bool ok_carga = (carga_sem[s] + inst.creditos[i] <= inst.carga_max);
            bool ok_count = (count_sem[s] + 1 <= inst.asign_max);
            if (ok_carga && ok_count) {
                sol.semestre[i] = s;
                carga_sem[s] += inst.creditos[i];
                count_sem[s]++;
                asignado = true;
                break;
            }
        }
        // Si no cabe en ningún semestre válido, forzar al mínimo posible
        if (!asignado) {
            sol.semestre[i] = sem_minimo;
            carga_sem[sem_minimo] += inst.creditos[i];
            count_sem[sem_minimo]++;
        }
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
