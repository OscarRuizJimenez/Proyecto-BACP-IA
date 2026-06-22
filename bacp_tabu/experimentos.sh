#!/bin/bash
# ============================================================
#  Experimentos BACP - Búsqueda Tabú
#  Genera los datos de las secciones Experimentos y Resultados.
#  Uso:  bash experimentos.sh
# ============================================================
cd "$(dirname "$0")"
make >/dev/null 2>&1

INSTS="bacp8 bacp10 bacp12 utfsm3 utfsm2311 utfsm2920 utfsm7310 utfsm7313"
SEEDS="1 2 3 4 5"

run() {  # $1=instancia $2=seed [$3=max_iter $4=restarts $5=alpha]
  ./bacp instancias/$1.txt $2 $3 $4 $5
}

echo "===== TABLA 1: 8 instancias x 5 semillas (parametros por defecto) ====="
echo "instancia,seed,fitness,factible,tiempo_ms"
for inst in $INSTS; do
  for s in $SEEDS; do
    out=$(run $inst $s)
    fit=$(echo "$out" | grep Fitness  | sed 's/.*: //')
    fac=$(echo "$out" | grep Factible | sed 's/Factible: //')
    tim=$(echo "$out" | grep Tiempo   | sed 's/.*: //; s/ ms//')
    echo "$inst,$s,$fit,$fac,$tim"
  done
done

echo
echo "===== TABLA 2: comparacion de alpha (RCL del greedy) ====="
echo "instancia,alpha,seed,fitness,factible"
for inst in bacp12 utfsm2311 utfsm7313; do
  for a in 0.1 0.3 0.6 0.9; do
    for s in $SEEDS; do
      out=$(run $inst $s 50000 30 $a)
      fit=$(echo "$out" | grep Fitness  | sed 's/.*: //')
      fac=$(echo "$out" | grep Factible | sed 's/Factible: //')
      echo "$inst,$a,$s,$fit,$fac"
    done
  done
done

echo
echo "===== TABLA 3: convergencia (calidad vs presupuesto/tiempo) en utfsm2311 ====="
echo "max_iter,seed,fitness,factible,tiempo_ms"
for mi in 1000 5000 20000 50000 200000; do
  for s in $SEEDS; do
    out=$(run utfsm2311 $s $mi 30 0.3)
    fit=$(echo "$out" | grep Fitness  | sed 's/.*: //')
    fac=$(echo "$out" | grep Factible | sed 's/Factible: //')
    tim=$(echo "$out" | grep Tiempo   | sed 's/.*: //; s/ ms//')
    echo "$mi,$s,$fit,$fac,$tim"
  done
done
