#include "bacp.h"
#include <vector>
#include <limits>
#include <algorithm>

using namespace std;

// ============================================================
//  Búsqueda Tabú para el BACP
//
//  Diseño (basado en la Dynamic Tabu Search de Chiarandini et
//  al. 2012):
//    - Representación vectorial: sol.semestre[i] = semestre de i.
//    - Movimiento único: Move (trasladar 1 asignatura a otro
//      semestre).  Vecindario RESTRINGIDO a la ventana de
//      precedencia (atiende la Retroalimentación: "el movimiento
//      puede generar muchas soluciones infactibles").
//    - Prerrequisitos: restricción dura, garantizada por la
//      ventana (ningún candidato los viola).
//    - Carga / cantidad por semestre: restricción blanda con
//      PENALIZACIÓN DINÁMICA (mecanismo ξ del DTS): cada semestre
//      tiene su propio peso; si la restricción se viola, el peso
//      se endurece (×ξ); si se satisface, se relaja (÷ξ).  Esto
//      fuerza a la búsqueda a reparar semestres persistentemente
//      sobrecargados (necesario para las instancias más rígidas).
//    - Lista tabú de tenencia aleatoria [kmin, kmax] que prohíbe
//      deshacer el último movimiento; criterio de aspiración.
//    - Multi-arranque: aprovecha el greedy aleatorizado.
//
//  La calidad de una solución (mejor global y aspiración) se mide
//  con una métrica ESTABLE de peso fijo P_BASE, independiente de
//  los pesos dinámicos, para que las comparaciones entre
//  iteraciones sean consistentes.
// ============================================================

static const double P_BASE = 1000.0;   // peso base (== calcular_fitness); piso de los pesos dinámicos
static const double XI      = 1.5;      // factor de endurecimiento/relajación de penalización
static const double W_MAX   = 1e7;      // tope superior de un peso dinámico

// Magnitud de violación de la cota de carga de un semestre.
static double mag_carga(double carga, const Instancia& inst) {
    double m = 0.0;
    if (carga < inst.carga_min) m += inst.carga_min - carga;
    if (carga > inst.carga_max) m += carga - inst.carga_max;
    return m;
}

// Magnitud de violación de la cota de cantidad de asignaturas de un semestre.
static double mag_count(int count, const Instancia& inst) {
    double m = 0.0;
    if (count < inst.asign_min) m += inst.asign_min - count;
    if (count > inst.asign_max) m += count - inst.asign_max;
    return m;
}

// Número de prerrequisitos violados INCIDENTES en la asignatura i
// (aristas en las que i participa como sucesor o como predecesor).
static int viol_prereq_de(int i, const vector<int>& sem, const Instancia& inst,
                          const vector<vector<int>>& suc) {
    int v = 0;
    for (int pre : inst.prereq[i]) if (sem[pre] >= sem[i]) v++;  // i debe ir tras sus prereqs
    for (int q : suc[i])           if (sem[i] >= sem[q]) v++;     // i debe ir antes que sus sucesores
    return v;
}

// ------------------------------------------------------------
//  Un arranque de Búsqueda Tabú desde una solución inicial.
// ------------------------------------------------------------
//  El parámetro 'adaptativo' selecciona el esquema de penalización:
//    true  -> pesos dinámicos (mecanismo ξ del DTS).
//    false -> penalización fija P_BASE (los pesos no se actualizan).
//  En la configuración actual, busqueda_tabu() ejecuta todos los arranques
//  con el esquema dinámico (adaptativo=true), que es el que mejor drena los
//  semestres persistentemente sobrecargados; la rama de penalización fija
//  se conserva como alternativa configurable.
static Solucion tabu_un_arranque(const Instancia& inst,
                                 const vector<vector<int>>& suc,
                                 const ParametrosTabu& par, mt19937& rng,
                                 bool adaptativo) {
    int n = inst.n_asignaturas;
    int N = inst.n_semestres;

    Solucion sol  = generar_solucion_inicial(inst, rng, par.alpha);
    Solucion best = sol;

    // Estructuras incrementales: carga y cantidad por semestre.
    vector<double> carga(N + 1, 0.0);
    vector<int>    count(N + 1, 0);
    double total = 0.0;
    for (int i = 0; i < n; i++) {
        carga[sol.semestre[i]] += inst.creditos[i];
        count[sol.semestre[i]]++;
        total += inst.creditos[i];
    }
    double mean = total / N;            // promedio constante (créditos totales fijos)

    // Pesos dinámicos por semestre (mecanismo ξ del DTS).  Parten en P_BASE,
    // que es también su piso, de modo que el comportamiento inicial coincide
    // con la penalización fija; solo se endurecen sobre semestres conflictivos.
    vector<double> w_carga(N + 1, P_BASE);
    vector<double> w_count(N + 1, P_BASE);

    // f_estable: fitness con peso fijo P_BASE (== calcular_fitness).  Sirve para
    // rastrear el mejor y el criterio de aspiración; se mantiene incremental.
    double f_estable = sol.fitness;

    // tabu_until[i][s] = iteración hasta la cual está prohibido mover i a s.
    vector<vector<int>> tabu_until(n, vector<int>(N + 1, 0));
    uniform_int_distribution<int> dist_tenencia(par.kmin, par.kmax);

    int sin_mejora = 0;
    for (int iter = 0; iter < par.max_iter && sin_mejora < par.max_sin_mejora; iter++) {
        int    mejor_i = -1, mejor_s = -1;
        double mejor_work   = numeric_limits<double>::infinity();  // delta con pesos dinámicos (ranking)
        double mejor_estable = 0.0;                                // delta estable del movimiento elegido

        for (int i = 0; i < n; i++) {
            int    a = sol.semestre[i];
            double w = inst.creditos[i];

            // Ventana de precedencia [lo, hi] del movimiento Move:
            //   lo = tras el último prerrequisito;  hi = antes del primer sucesor.
            int lo = 1, hi = N;
            for (int pre : inst.prereq[i]) lo = max(lo, sol.semestre[pre] + 1);
            for (int q : suc[i])           hi = min(hi, sol.semestre[q] - 1);

            // Magnitudes/violaciones actuales (estado base, no dependen del destino).
            double mca_a = mag_carga(carga[a], inst);
            double mcn_a = mag_count(count[a], inst);
            int    vprq_old = viol_prereq_de(i, sol.semestre, inst, suc);
            double ca_new = carga[a] - w;
            double mca_a_new = mag_carga(ca_new, inst);
            double mcn_a_new = mag_count(count[a] - 1, inst);

            for (int s = lo; s <= hi; s++) {
                if (s == a) continue;

                // Variación de varianza (solo afecta a los semestres a y s).
                double cb_new = carga[s] + w;
                double var_old = (carga[a] - mean) * (carga[a] - mean)
                               + (carga[s] - mean) * (carga[s] - mean);
                double var_new = (ca_new - mean) * (ca_new - mean)
                               + (cb_new - mean) * (cb_new - mean);
                double dvar = var_new - var_old;

                // Variación de magnitudes de capacidad en a y s.
                double dmag_ca_a = mca_a_new - mca_a;                         // carga, semestre a
                double dmag_cn_a = mcn_a_new - mcn_a;                         // cantidad, semestre a
                double dmag_ca_s = mag_carga(cb_new, inst)   - mag_carga(carga[s], inst);
                double dmag_cn_s = mag_count(count[s] + 1, inst) - mag_count(count[s], inst);

                // Variación de prerrequisitos incidentes en i (peso fijo: restricción dura).
                int viol_new = 0;
                for (int pre : inst.prereq[i]) if (sol.semestre[pre] >= s) viol_new++;
                for (int q : suc[i])           if (s >= sol.semestre[q]) viol_new++;
                double dprq = P_BASE * (viol_new - vprq_old);

                // Delta con pesos DINÁMICOS (guía la selección del movimiento).
                double dwork = dvar + dprq
                             + w_carga[a] * dmag_ca_a + w_count[a] * dmag_cn_a
                             + w_carga[s] * dmag_ca_s + w_count[s] * dmag_cn_s;

                // Delta ESTABLE (peso fijo P_BASE) para aspiración y mejor global.
                double destable = dvar + dprq
                                + P_BASE * (dmag_ca_a + dmag_cn_a + dmag_ca_s + dmag_cn_s);

                bool es_tabu = tabu_until[i][s] > iter;
                bool aspira  = (f_estable + destable) < best.fitness;
                if (es_tabu && !aspira) continue;

                if (dwork < mejor_work) {
                    mejor_work    = dwork;
                    mejor_estable = destable;
                    mejor_i = i; mejor_s = s;
                }
            }
        }

        if (mejor_i < 0) break;  // ningún movimiento admisible (vecindario vacío)

        // Aplicar el mejor movimiento.
        int    i = mejor_i, b = mejor_s, a = sol.semestre[i];
        double w = inst.creditos[i];
        carga[a] -= w;  carga[b] += w;
        count[a]--;     count[b]++;
        sol.semestre[i] = b;
        f_estable += mejor_estable;
        sol.fitness = f_estable;

        // Prohibir el movimiento inverso (devolver i a su semestre anterior).
        tabu_until[i][a] = iter + dist_tenencia(rng);

        // Actualización dinámica de penalizaciones (mecanismo ξ): endurecer los
        // semestres que violan una cota, relajar los que la cumplen.  Solo en
        // los arranques adaptativos; en los demás los pesos quedan en P_BASE
        // (penalización fija).
        if (adaptativo) {
            for (int s = 1; s <= N; s++) {
                if (mag_carga(carga[s], inst) > 0) w_carga[s] = min(W_MAX, w_carga[s] * XI);
                else                               w_carga[s] = max(P_BASE, w_carga[s] / XI);
                if (mag_count(count[s], inst) > 0) w_count[s] = min(W_MAX, w_count[s] * XI);
                else                               w_count[s] = max(P_BASE, w_count[s] / XI);
            }
        }

        if (f_estable < best.fitness - 1e-9) { best = sol; sin_mejora = 0; }
        else                                   sin_mejora++;
    }

    return best;
}

// ------------------------------------------------------------
//  Multi-arranque: varias corridas con greedy aleatorizado.
// ------------------------------------------------------------
Solucion busqueda_tabu(const Instancia& inst, const ParametrosTabu& par) {
    vector<vector<int>> suc = construir_sucesores(inst);
    mt19937 rng(par.seed);

    Solucion best;
    bool tiene = false;
    int  R = max(1, par.restarts);
    for (int r = 0; r < R; r++) {
        Solucion s = tabu_un_arranque(inst, suc, par, rng, /*adaptativo=*/true);
        if (!tiene || s.fitness < best.fitness) { best = s; tiene = true; }
    }
    return best;
}

// Parámetros por defecto (tunables; ajustables vía CLI en main.cpp).
ParametrosTabu parametros_por_defecto() {
    ParametrosTabu p;
    p.max_iter       = 50000;
    p.max_sin_mejora = 10000;
    p.kmin           = 5;
    p.kmax           = 10;
    p.restarts       = 30;
    p.alpha          = 0.3;
    p.seed           = 12345u;
    return p;
}
