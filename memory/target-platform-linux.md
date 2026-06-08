---
name: target-platform-linux
description: The BACP project must compile and run on Linux, even though dev machine is Windows
metadata:
  type: project
---

El proyecto BACP (Búsqueda Tabú, C++) **debe compilar y ejecutarse en Linux**, aunque la máquina de desarrollo de Oscar es Windows 11.

**Why:** es un requisito de la entrega/evaluación del ramo IA — el código se corre en un entorno Linux.

**How to apply:** no instalar toolchains de Windows (MSYS2/MinGW/MSVC) como solución principal. Usar WSL (Ubuntu) con `g++`/`build-essential`, o verificar en un Linux real. Compilar con `g++` estándar y evitar APIs específicas de Windows.
