# BACP - Búsqueda Tabú
## Oscar Mauricio Ruiz Jimenez — Rol 202273077-3

Resolución del **Balanced Academic Curriculum Problem (BACP)** mediante la
metaheurística **Búsqueda Tabú** (Dynamic Tabu Search, Chiarandini et al. 2012).

---

## Compilación

```bash
make
```

Para limpiar los archivos de compilación:

```bash
make clean
```

## Ejecución

```bash
./bacp instancias/<archivo_instancia.txt> [seed] [max_iter] [restarts] [alpha]
```

Parámetros opcionales (posicionales). Si se omiten, se usan los valores por defecto
indicados:

| Parámetro  | Descripción                                              | Por defecto |
|------------|----------------------------------------------------------|:-----------:|
| `seed`     | semilla del generador aleatorio, para reproducir una corrida | `12345` |
| `max_iter` | iteraciones máximas por arranque                         | `50000`     |
| `restarts` | número de arranques (multi-arranque)                     | `30`        |
| `alpha`    | fracción de la RCL del greedy aleatorizado (0..1)        | `0.3`       |

> Los parámetros son posicionales: para fijar uno hay que indicar también los
> anteriores (p. ej. para `restarts` se deben dar antes `seed` y `max_iter`).
> Otros parámetros internos de la tenencia tabú se fijan en el código
> (`parametros_por_defecto` en `tabu.cpp`): `kmin = 5`, `kmax = 10`,
> `max_sin_mejora = 10000`.

**Ejemplos:**
```bash
./bacp instancias/bacp8.txt
./bacp instancias/utfsm7313.txt 42
./bacp instancias/bacp12.txt 7 20000
./bacp instancias/utfsm2311.txt 3 50000 30 0.9
```

## Experimentos

El script `experimentos.sh` reproduce los experimentos del informe (8 instancias ×
5 semillas, comparación del parámetro `alpha` y análisis de convergencia). El script
escribe las tres tablas por **salida estándar**, por lo que para guardarlas se debe
**redirigir** la salida a un archivo CSV:

```bash
bash experimentos.sh > resultados.csv
```

> Nota: el operador `>` reemplaza el contenido previo de `resultados.csv`. Si se
> ejecuta `bash experimentos.sh` sin redirección, las tablas solo se muestran en
> pantalla y no se guarda ningún archivo.

## Gráficos

A partir del CSV anterior, el script `graficar.py` genera los gráficos del informe
(`graficos/e1_instancias.png`, `graficos/e2_alpha.png`, `graficos/e3_convergencia.png`):

```bash
python3 graficar.py resultados.csv
```

Cada ejecución **sobrescribe** los tres PNG con los mismos nombres dentro de `graficos/`.

### Dependencias (solo para graficar)

El programa principal (`bacp`) **no** requiere librerías externas: usa únicamente la
biblioteca estándar de C++. El script de graficado `graficar.py` sí requiere Python 3
con las librerías `pandas` y `matplotlib`, que pueden instalarse con:

```bash
pip install pandas matplotlib
```

---

## Diseño del algoritmo

- **Representación:** vector `semestre[i] = s` (semestre asignado a la asignatura `i`).
- **Solución inicial:** greedy topológico **aleatorizado** (orden barajado dentro de
  cada nivel + lista restringida de candidatos estilo GRASP). Cada asignatura se ubica
  dentro de `[nivel, N − cola]` (`cola` = cadena más larga de sucesores), garantizando
  que la solución inicial nunca viola prerrequisitos.
- **Movimiento:** *Move* (trasladar una asignatura a otro semestre), con el
  **vecindario restringido a la ventana de precedencia** de cada asignatura, de modo
  que ningún candidato viola prerrequisitos.
- **Función objetivo:** `f(x) = Σ (C_j − C̄)²  +  penalizaciones` por violar las cotas
  de carga/cantidad por semestre. Las penalizaciones usan **pesos dinámicos**
  (mecanismo ξ de la Dynamic Tabu Search): el peso de un semestre se endurece (×ξ)
  mientras viola una cota y se relaja (÷ξ) cuando la cumple, con piso en `P = 1000`.
  La calidad se mide con una métrica estable de peso fijo `P` para comparaciones
  consistentes entre iteraciones.
- **Lista tabú:** prohíbe deshacer el último movimiento durante una tenencia aleatoria
  en `[kmin, kmax]` iteraciones; con **criterio de aspiración** (acepta un movimiento
  tabú si mejora la mejor solución global).
- **Multi-arranque:** varias corridas desde soluciones iniciales distintas; se reporta
  la mejor.

### Retroalimentación del Estado de Avance atendida
1. **Aleatoriedad en el greedy** — `generar_solucion_inicial` (en `solucion.cpp`).
2. **Vecindario restringido del movimiento Move** — ventana de precedencia en
   `tabu.cpp` (`busqueda_tabu`), que evita generar soluciones infactibles por
   prerrequisitos.

## Archivos
- `bacp.h` — estructuras y declaraciones.
- `leer_instancia.cpp` — lectura del formato `.dat` de las instancias.
- `solucion.cpp` — fitness, factibilidad, solución inicial e impresión.
- `tabu.cpp` — núcleo de la Búsqueda Tabú (movimiento, lista tabú, aspiración, multi-arranque).
- `main.cpp` — orquestación y línea de comandos.
