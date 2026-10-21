/* ==========================================================================
 * generador_python.h  —  Generador de Codigo Python
 *
 * Traductor: mini-C (lenguaje del analizador)  →  Python
 *
 * Recibe el AST producido por el parser (analizar_sintaxis) y genera el
 * codigo fuente equivalente en Python:
 *   - Declaraciones con tipo  →  asignacion simple (Python no declara tipos)
 *   - if / else               →  if: / elif: / else:   (con sangria)
 *   - while                   →  while:
 *   - for (init; cond; incr)  →  init + while: + cuerpo + incr al final
 *   - do-while                →  while True: + if not (cond): break
 *   - break / continue        →  break / continue
 *   - return                  →  comentario (a nivel de modulo no es valido)
 *   - && || !                 →  and or not
 *   - true / false            →  True / False
 *   - ++ / --                 →  += 1 / -= 1 (como sentencia o en el for)
 * ========================================================================== */

#ifndef GENERADOR_PYTHON_H
#define GENERADOR_PYTHON_H

#include "parser.h"

/* --------------------------------------------------------------------------
 *  Funcion publica del generador
 * -------------------------------------------------------------------------- */

/**
 * generar_codigo_python()
 *
 * Recorre el AST recibido y genera el codigo Python equivalente.
 *
 * @param raiz  Raiz del AST (producido por analizar_sintaxis)
 * @return      Cadena malloc'ada con el codigo Python (liberar con free()),
 *              o NULL en caso de error
 */
char* generar_codigo_python(NodoAST* raiz);

#endif /* GENERADOR_PYTHON_H */
