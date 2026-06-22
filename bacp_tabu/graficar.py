#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Grafica los resultados de experimentos.sh (BACP - Busqueda Tabu).
Lee 'resultados.csv' (las tres tablas concatenadas) y produce un PNG por
experimento en la carpeta 'graficos/'.

Uso:  python graficar.py [resultados.csv]
"""
import io
import sys
import os
import pandas as pd
import matplotlib
matplotlib.use("Agg")               # backend sin ventana (guarda a archivo)
import matplotlib.pyplot as plt

ARCHIVO = sys.argv[1] if len(sys.argv) > 1 else "resultados.csv"
SALIDA = "graficos"
os.makedirs(SALIDA, exist_ok=True)


def cargar_secciones(ruta):
    """Divide el archivo en bloques separados por las lineas '====='
    y devuelve un DataFrame por cada bloque que tenga cabecera CSV."""
    with open(ruta, encoding="utf-8") as f:
        lineas = f.read().splitlines()

    bloques, actual = [], []
    for ln in lineas:
        if ln.startswith("====="):
            if actual:
                bloques.append(actual)
            actual = []
        elif ln.strip():
            actual.append(ln)
    if actual:
        bloques.append(actual)

    dfs = []
    for b in bloques:
        # la primera linea del bloque debe ser una cabecera CSV (tiene comas)
        if "," in b[0]:
            dfs.append(pd.read_csv(io.StringIO("\n".join(b))))
    return dfs


def localizar(dfs, columnas):
    """Devuelve el primer DataFrame cuyas columnas contengan 'columnas'."""
    for df in dfs:
        if set(columnas).issubset(df.columns):
            return df
    raise ValueError(f"No se encontro tabla con columnas {columnas}")


def main():
    dfs = cargar_secciones(ARCHIVO)

    # ---- E1: 8 instancias x 5 semillas (parametros por defecto) ----------
    t1 = localizar(dfs, ["instancia", "seed", "fitness"])
    g = t1.groupby("instancia")["fitness"]
    resumen = pd.DataFrame({"media": g.mean(), "min": g.min(),
                            "std": g.std()}).sort_values("media")
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.bar(resumen.index, resumen["media"], yerr=resumen["std"],
           capsize=4, color="#4C72B0", label="Promedio (5 semillas)")
    ax.scatter(resumen.index, resumen["min"], color="#C44E52",
               zorder=3, label="Mejor (min)")
    ax.set_ylabel("Desbalance  f(x)")
    ax.set_xlabel("Instancia")
    ax.set_title("E1 - Desbalance por instancia (parametros por defecto)")
    ax.legend()
    plt.xticks(rotation=30, ha="right")
    fig.tight_layout()
    fig.savefig(f"{SALIDA}/e1_instancias.png", dpi=150)
    plt.close(fig)

    # ---- E2: efecto del parametro alpha ----------------------------------
    t2 = localizar(dfs, ["instancia", "alpha", "fitness"])
    piv = t2.groupby(["instancia", "alpha"])["fitness"].mean().unstack(0)
    insts = piv.columns
    fig, axes = plt.subplots(1, len(insts), figsize=(4.5 * len(insts), 4.2))
    if len(insts) == 1:
        axes = [axes]
    for ax, inst in zip(axes, insts):
        ax.plot(piv.index, piv[inst], "o-", color="#55A868")
        ax.set_title(inst)
        ax.set_xlabel(r"$\alpha$ (RCL del greedy)")
        ax.set_ylabel("Desbalance promedio")
        ax.grid(True, alpha=0.3)
    fig.suptitle(r"E2 - Efecto del parametro $\alpha$ (5 semillas)")
    fig.tight_layout()
    fig.savefig(f"{SALIDA}/e2_alpha.png", dpi=150)
    plt.close(fig)

    # ---- E3: convergencia (calidad y tiempo vs presupuesto) --------------
    t3 = localizar(dfs, ["max_iter", "fitness"])
    g3 = t3.groupby("max_iter")
    fit = g3["fitness"].mean()
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(fit.index, fit.values, "o-", color="#4C72B0", label="Desbalance")
    ax.set_xscale("log")
    ax.set_xlabel("max_iter por arranque (escala log)")
    ax.set_ylabel("Desbalance promedio", color="#4C72B0")
    ax.tick_params(axis="y", labelcolor="#4C72B0")
    if "tiempo_ms" in t3.columns:
        tim = g3["tiempo_ms"].mean()
        ax2 = ax.twinx()
        ax2.plot(tim.index, tim.values, "s--", color="#C44E52",
                 label="Tiempo (ms)")
        ax2.set_ylabel("Tiempo promedio (ms)", color="#C44E52")
        ax2.tick_params(axis="y", labelcolor="#C44E52")
    ax.set_title("E3 - Convergencia en utfsm2311")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(f"{SALIDA}/e3_convergencia.png", dpi=150)
    plt.close(fig)

    print(f"Listo. Graficos guardados en '{SALIDA}/':")
    for nombre in ("e1_instancias", "e2_alpha", "e3_convergencia"):
        print(f"  - {SALIDA}/{nombre}.png")


if __name__ == "__main__":
    main()
