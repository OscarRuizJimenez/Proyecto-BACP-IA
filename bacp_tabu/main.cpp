#include "bacp.h"
#include <iostream>
#include <string>

using namespace std;

// ============================================================
//  Uso: ./bacp <archivo_instancia.txt>
//
//  Ejecuta la implementación mínima del Estado de Avance:
//    1. Lectura de la instancia
//    2. Generación de solución inicial (heurística greedy)
//    3. Cálculo e impresión de la función de evaluación
// ============================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <instancia.txt>\n";
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

    // ---- 2. Generación de solución inicial ----
    cout << "\nGenerando solucion inicial (greedy por nivel)...\n";
    Solucion sol = generar_solucion_inicial(inst);

    // ---- 3. Evaluación e impresión ----
    imprimir_solucion(sol, inst);

    return 0;
}
