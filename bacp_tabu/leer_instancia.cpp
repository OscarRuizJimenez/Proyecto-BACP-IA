#include "bacp.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>

using namespace std;



// Lee el archivo completo como un único string (líneas separadas por espacio)
static string leer_contenido(const string& archivo) {
    ifstream f(archivo);
    if (!f.is_open()) return "";
    string contenido, linea;
    while (getline(f, linea))
        contenido += linea + " ";
    return contenido;
}

// Extrae entero de "clave = N" o "clave=N" (una sola letra como p,a,b,c,d)
// Solo para claves de un carácter para evitar falsos positivos
static int extraer_param(const string& texto, char clave) {
    for (size_t i = 0; i < texto.size(); i++) {
        if (texto[i] != clave) continue;
        // verificar que sea palabra sola (no parte de identificador)
        if (i > 0 && (isalnum(texto[i-1]) || texto[i-1]=='_')) continue;
        size_t j = i + 1;
        while (j < texto.size() && isspace(texto[j])) j++;
        if (j >= texto.size() || texto[j] != '=') continue;
        j++;
        while (j < texto.size() && isspace(texto[j])) j++;
        string num;
        while (j < texto.size() && isdigit(texto[j])) num += texto[j++];
        if (!num.empty()) return stoi(num);
    }
    return -1;
}

// Busca la posición de "keyword" como palabra completa (no parte de identificador)
static size_t buscar_keyword(const string& texto, const string& kw, size_t desde = 0) {
    size_t pos = desde;
    while (pos < texto.size()) {
        size_t f = texto.find(kw, pos);
        if (f == string::npos) return string::npos;
        // no debe estar pegada a identificador antes ni después
        bool ok_antes   = (f == 0 || !(isalnum(texto[f-1]) || texto[f-1]=='_'));
        bool ok_despues = (f+kw.size() >= texto.size() ||
                           !(isalnum(texto[f+kw.size()]) || texto[f+kw.size()]=='_'));
        if (ok_antes && ok_despues) return f;
        pos = f + 1;
    }
    return string::npos;
}

// Extrae tokens de { ... } tras la keyword dada
static vector<string> extraer_bloque(const string& texto, const string& kw) {
    size_t pos = buscar_keyword(texto, kw);
    if (pos == string::npos) return {};
    pos = texto.find('{', pos);
    if (pos == string::npos) return {};
    size_t fin = texto.find('}', pos);
    if (fin == string::npos) return {};

    string bloque = texto.substr(pos+1, fin-pos-1);
    for (char& c : bloque) if (c==',' || c==';') c = ' ';

    vector<string> tokens;
    istringstream ss(bloque);
    string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

// Extrae lista de enteros de [ ... ] tras la keyword dada
static vector<int> extraer_lista_int(const string& texto, const string& kw) {
    size_t pos = buscar_keyword(texto, kw);
    if (pos == string::npos) return {};
    pos = texto.find('[', pos);
    if (pos == string::npos) return {};
    size_t fin = texto.find(']', pos);
    if (fin == string::npos) return {};

    string bloque = texto.substr(pos+1, fin-pos-1);
    for (char& c : bloque) if (c==',') c = ' ';

    vector<int> vals;
    istringstream ss(bloque);
    int v;
    while (ss >> v) vals.push_back(v);
    return vals;
}

// Extrae pares de prerrequisitos <asign_a , asign_b>
// Semántica: asign_b es prerrequisito de asign_a
static vector<pair<string,string>> extraer_prereqs(const string& texto) {
    vector<pair<string,string>> pares;
    size_t pos = buscar_keyword(texto, "prereq");
    if (pos == string::npos) return pares;
    pos = texto.find('{', pos);
    if (pos == string::npos) return pares;
    size_t fin = texto.find('}', pos);
    if (fin == string::npos) return pares;

    string bloque = texto.substr(pos+1, fin-pos-1);
    auto trim = [](string s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        auto last = s.find_last_not_of(" \t\r\n");
        if (last != string::npos) s.erase(last+1);
        return s;
    };

    size_t i = 0;
    while (i < bloque.size()) {
        size_t ap = bloque.find('<', i);
        if (ap == string::npos) break;
        size_t ci = bloque.find(',', ap);
        size_t cp = bloque.find('>', ap);
        if (ci == string::npos || cp == string::npos) break;
        string a = trim(bloque.substr(ap+1, ci-ap-1));
        string b = trim(bloque.substr(ci+1, cp-ci-1));
        if (!a.empty() && !b.empty()) pares.push_back({a, b});
        i = cp + 1;
    }
    return pares;
}


//  Función principal: leer_instancia
bool leer_instancia(const string& archivo, Instancia& inst) {
    string texto = leer_contenido(archivo);
    if (texto.empty()) {
        cerr << "Error: no se pudo abrir el archivo '" << archivo << "'\n";
        return false;
    }

    // --- Parámetros escalares (letras solas: p, a, b, c, d) ---
    inst.n_semestres = extraer_param(texto, 'p');
    inst.carga_min   = extraer_param(texto, 'a');
    inst.carga_max   = extraer_param(texto, 'b');
    inst.asign_min   = extraer_param(texto, 'c');
    inst.asign_max   = extraer_param(texto, 'd');

    if (inst.n_semestres < 1) {
        cerr << "Error: parámetro 'p' (semestres) no encontrado.\n";
        return false;
    }

    // --- Lista de asignaturas ---
    inst.nombres = extraer_bloque(texto, "courses");
    inst.n_asignaturas = (int)inst.nombres.size();
    if (inst.n_asignaturas == 0) {
        cerr << "Error: lista 'courses' vacía o no encontrada.\n";
        return false;
    }

    // --- Créditos ---
    inst.creditos = extraer_lista_int(texto, "credit");
    if ((int)inst.creditos.size() != inst.n_asignaturas) {
        cerr << "Advertencia: cantidad de créditos (" << inst.creditos.size()
             << ") no coincide con asignaturas (" << inst.n_asignaturas << ").\n";
        inst.creditos.resize(inst.n_asignaturas, 1);
    }

    // --- Prerrequisitos ---
    inst.prereq.assign(inst.n_asignaturas, {});
    map<string,int> idx;
    for (int i = 0; i < inst.n_asignaturas; i++) idx[inst.nombres[i]] = i;

    auto pares = extraer_prereqs(texto);
    for (auto& [a, b] : pares) {
        if (idx.count(a) && idx.count(b)) {
            int ia = idx[a], ib = idx[b];
            // Evitar prerrequisitos duplicados,
            // que de lo contrario penalizarían dos veces la misma restricción.
            auto& lista = inst.prereq[ia];
            if (find(lista.begin(), lista.end(), ib) == lista.end())
                lista.push_back(ib);
        }
    }

    return true;
}
