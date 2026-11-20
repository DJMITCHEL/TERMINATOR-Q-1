# TERMINATOR Q

Plugin **VST3** para Windows (x64) pensado para **FL Studio**: un EQ de 10 bandas con
analizador y *target matching*, con estética dorado sobre negro.

## Qué hace

1. **ANALYZE**: pulsa y reproduce tu mezcla. El plugin promedia el espectro de entrada.
2. **STOP & MATCH**: pulsa de nuevo. Compara el espectro promedio con la curva *target* del
   preset elegido y coloca automáticamente las 10 bandas del EQ.
3. **AMOUNT (%)**: cuánto del ajuste se aplica (0–100 %).
4. **Nodos arrastrables** en la gráfica para retocar a mano (doble clic = poner a 0 dB).
5. **Presets target**: Trap Bass-heavy, Trap Dark & Wide, Hip Hop, Pop, EDM, Rock, Flat.
   Al cambiar de preset después de analizar, se recalcula el match al instante.
6. **OUTPUT**, **BYPASS**, **RESET EQ**. Todos los parámetros son automatizables en FL Studio.

Bandas: 40 Hz (low shelf), 80, 160, 320, 640, 1.25k, 2.5k, 5k, 9k (peak) y 14k (high shelf). Rango ±9 dB.

## Compilar en GitHub (sin instalar nada)

1. Crea un repositorio vacío en tu cuenta (por ejemplo `TerminatorQ`).
2. Sube esta carpeta:
   ```
   git init
   git add .
   git commit -m "TERMINATOR Q"
   git branch -M main
   git remote add origin https://github.com/TU_USUARIO/TerminatorQ.git
   git push -u origin main
   ```
3. En GitHub: pestaña **Actions** → workflow *Build TERMINATOR Q* → cuando termine, descarga
   el artefacto **TERMINATOR-Q-VST3-Windows**.
4. (Opcional) Para publicar una release descargable: `git tag v1.0.0 && git push origin v1.0.0`.

## Instalar en FL Studio

1. Descomprime y copia la carpeta `TERMINATOR Q.vst3` a `C:\Program Files\Common Files\VST3\`.
2. FL Studio → *Options → Manage plugins* → *Find installed plugins* (con *Verify plugins* activado).
3. Búscalo en el Plugin database → Installed → Effects → TERMINATOR Q.

## Compilar en local (opcional)

Requiere Visual Studio 2022 (carga de trabajo C++) y CMake:
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Resultado: `build/TerminatorQ_artefacts/Release/VST3/TERMINATOR Q.vst3`

## Licencia

Usa [JUCE](https://juce.com) (AGPLv3 o licencia comercial) y el SDK de VST3. Si distribuyes el
plugin, tu código debe publicarse bajo AGPLv3, o necesitas una licencia comercial de JUCE.
TERMINATOR Q es una implementación independiente: no usa código ni recursos de Carbon Q.
