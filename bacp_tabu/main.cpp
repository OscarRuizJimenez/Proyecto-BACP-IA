#include "bacp.h"
#include <iostream>
#include <string>
#include <chrono>

using namespace std;

// ============================================================
//  Uso: ./bacp <archivo_instancia.txt> [seed] [max_iter]
//
//  Resuelve el BACP mediante Búsqueda Tabú:
//    1. Lectura de la instancia
//    2. Búsqueda Tabú con multi-arranque (greedy aleatorizado +
//       movimiento Move de vecindario restringido)
//    3. Impresión de la mejor solución encontrada
//
//  Parámetros opcionales (posicionales):
//    seed     : semilla del generador aleatorio (reproducibilidad)
//    max_iter : iteraciones máximas por arranque
//    restarts : número de arranques (multi-arranque)
//    alpha    : fracción de la RCL del greedy aleatorizado (0..1)
// ============================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <instancia.txt> [seed] [max_iter] [restarts] [alpha]\n";
        return 1;
    }

    string archivo = argv[1];

    // ---- 1. Lectura de instancia ----
    Instancia inst;
    cout << "Leyendo instancia: " << archivo << "\n";
    if (!leer_instancia(archivo, inst)) {
        cerr << "Error al leer la instancia.\n";
        return 1;
    }

    cout << "Instancia cargada:\n"
         << "  Asignaturas  : " << inst.n_asignaturas << "\n"
         << "  Semestres    : " << inst.n_semestres   << "\n"
         << "  Carga min/max: " << inst.carga_min << " / " << inst.carga_max << "\n"
         << "  Asign min/max: " << inst.asign_min << " / " << inst.asign_max << "\n";

    int total_prereqs = 0;
    for (auto& v : inst.prereq) total_prereqs += (int)v.size();
    cout << "  Prerrequisitos: " << total_prereqs << "\n";

    // ---- 2. Parámetros (con overrides por línea de comandos) ----
    ParametrosTabu par = parametros_por_defecto();
    if (argc >= 3) par.seed     = (unsigned)stoul(argv[2]);
    if (argc >= 4) par.max_iter = stoi(argv[3]);
    if (argc >= 5) par.restarts = stoi(argv[4]);
    if (argc >= 6) par.alpha    = stod(argv[5]);

    cout << "\nParametros Busqueda Tabu:\n"
         << "  seed=" << par.seed
         << "  max_iter=" << par.max_iter
         << "  max_sin_mejora=" << par.max_sin_mejora
         << "  tabu=[" << par.kmin << "," << par.kmax << "]"
         << "  restarts=" << par.restarts
         << "  alpha=" << par.alpha << "\n";

    // ---- 3. Búsqueda Tabú ----
    cout << "\nEjecutando Busqueda Tabu...\n";
    auto t0 = chrono::high_resolution_clock::now();
    Solucion sol = busqueda_tabu(inst, par);
    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();

    // ---- 4. Resultado ----
    imprimir_solucion(sol, inst);
    cout << "Tiempo de ejecucion: " << ms << " ms\n";

    return 0;
}
