# Contexto del Proyecto: Fork de GameYob para Nintendo 3DS

## 1. Identidad y Propósito
Este repositorio (`cframo/GAMEBOYC-3DS-GameYob`) es un fork de desarrollo activo del emulador GameYob optimizado exclusivamente para Nintendo 3DS (CTR). El objetivo principal es mantener una base de código limpia, eficiente y sin advertencias, garantizando una emulación fidedigna de Game Boy (DMG) y Game Boy Color (CGB), con especial énfasis en el comportamiento del hardware dual-cartridge y soporte visual preciso.

---

## 2. Entorno y Herramientas (Contenedor Podman)
* **Aislamiento:** La toolchain no está instalada en el host. Todo corre encapsulado vía Podman utilizando la imagen oficial `docker.io/devkitpro/devkitarm`.
* **Herramientas y Wrappers disponibles:**
  * `./dkp-make`: Compilación directa usando el contenedor.
  * `./dkp-3dslink`: Wrapper de `3dslink` ejecutado con `--net=host` para despliegue por red local a la consola.
  * `./dev.sh`: Script unificador del ciclo de desarrollo (`./dev.sh build`, `./dev.sh clean`, `./dev.sh send <IP>`).
* **Restricción de comandos:** Prohibido invocar `make` o `3dslink` directamente en el host. Usar siempre `./dev.sh` o los scripts `./dkp-*`.
* **Target y Makefiles:** Target exclusivo `TARGET := 3DS`. Prohibido modificar el `Makefile` para otros sistemas (NDS).
* **Infraestructura:** La carpeta `buildtools/` está absorbida de forma nativa en el árbol de trabajo. No reintroducir submódulos de Git ni archivos `.gitmodules`.
* **Artefactos ignorados:** No rastrear ni sugerir cambios sobre binarios `.shbin`, cabeceras generadas en `include/platform/`, `bios_bin.h`, `dummy_bios_bin.h` ni carpetas intermedias (`build/`, `output/`).

---

## 3. Arquitectura del Núcleo de Emulación
* **`CPU` (`include/cpu.h`, `source/cpu.cpp`):**
  * Mantener el encapsulamiento estricto de `struct Registers registers` (visibilidad privada).
  * Las modificaciones externas a los registros durante eventos de hardware o BIOS deben realizarse a través de helpers en línea dedicados (ej. `setA(u8 val)`).
  * Respetar la sincronización de ciclos por instrucción (`cycleCount`, `eventCycle`). No añadir retardos ni alterar la cadencia de ejecución de opcodes.
* **`MMU` (`source/mmu.cpp`):**
  * Controla el mapeo de memoria dinámica, MBCs y el desmapeo del arranque de la BIOS (`0xFF50`).
  * En la transición post-BIOS, el registro `A` de la CPU debe quedar fijado en `0x11` para `MODE_CGB` y `0x01` para `MODE_DMG` para asegurar que los cartuchos duales ejecuten sus rutinas CGB nativas (ej. *Tetris DX*).
  * No implementar hooks mágicos ni parches sobre direcciones de memoria específicas (`0x00FD`, etc.) sin justificación de hardware comprobada.
* **`PPU` (`source/ppu.cpp`):**
  * Manejo de VRAM, OAM y registros de paletas (BG/OBJ).
  * Las paletas `bgPalette` y `sprPalette` deben inicializarse con valores de escala de grises de respaldo (`grayScalePalette`) para prevenir bloques negros en sprites durante los primeros cuadros del juego.
* **`Platform 3DS` (`source/platform/3ds/gfx_3ds.cpp`):**
  * Renderizado mediante Citro3D / GPU PICA200.
  * Mantener la configuración de `C3D_TexEnv` sin llamadas duplicadas o estados redundantes en el pipeline de rasterizado.

---

## 4. Reglas de Calidad de Código y Memoria
1. **Estándar C/C++:** C++11 y C99 estrictos. Sin dependencias externas pesadas ni características de C++17/20.
2. **Gestión de memoria dinámica:** Es obligatorio usar `delete[]` para liberar cualquier arreglo reservado mediante `new[]` (verificable en `Cartridge` y `manager.cpp`). No tolerar discrepancias de punteros ni fugas de memoria.
3. **Inicialización segura:** En C++11, reiniciar arreglos de `std::function` o callbacks mediante bucles asignando `nullptr`, nunca mediante `memset` para evitar la corrupción de estructuras polimórficas o vtables.
4. **Optimización homebrew:** El rendimiento en la CPU ARM11 de la 3DS es crítico. Priorizar código predecible en caché, evitar asignaciones dinámicas en el bucle principal de emulación y evitar abstracciones innecesarias.

---

## 5. Directrices de Git y Flujo de Trabajo
* Estructurar siempre las soluciones siguiendo el formato de **Conventional Commits**:
  * `feat(scope): ...`
  * `fix(scope): ...`
  * `build(scope): ...`
  * `refactor(scope): ...`
* Los commits deben ser atómicos y separar claramente las correcciones de plataforma de las modificaciones en el core.
* Trabajar con diffs limpios: nunca dejar líneas de depuración huérfanas (`printf`, logs temporales) en el código a commitear.
* Comandos permitidos para verificación del agente: `./dev.sh build` o `./dkp-make`. Prohibido ejecutar comandos de envío por red (`send`) sin intervención del usuario.

---

## 6. Convenciones de Código y Estilo
1. **Indentación y Formato:** 4 espacios estrictos por nivel de indentación (`UseTab: Never`).
2. **Posición de Llaves y Control:** Estilo K&R / Attach (`BreakBeforeBraces: Attach`) y sin espacio previo al paréntesis de control (`SpaceBeforeParens: Never`, ej. `if(cond) {`, `while(cond) {`).
3. **Uso de `this->`:** En todas las clases del núcleo de emulación (`CPU`, `MMU`, `PPU`, `Cartridge`, `APU`, `Gameboy`, etc.), es obligatorio el uso explícito y sistemático de `this->` para acceder a variables miembro y métodos internos.
4. **Punteros Nulos:** Uso estricto de `nullptr` para todo código nuevo o modificado bajo C++11. La sustitución de `NULL` heredado se realizará de forma progresiva, evitando reformatos masivos que ensucien el historial.
5. **Bibliotecas de Terceros:** El código en `source/gb_apu/`, `include/gb_apu/`, `include/libs/stb_image/` e `include/libs/inih/` no debe reformatearse ni alterarse sin justificación estricta.
