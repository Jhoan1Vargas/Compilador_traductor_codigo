/* ==========================================================================
 * generador_python.c  —  Implementacion del Generador de Codigo Python
 *
 * Traductor: mini-C (lenguaje del analizador)  →  Python
 *
 * Fase 4 del compilador: recorre el AST construido por el parser y emite
 * codigo Python equivalente. Es el reverso del analizador sintactico:
 * en lugar de consumir tokens, cada nodo del arbol produce texto.
 * ========================================================================== */

#include "generador_python.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 *  Buffer de texto dinamico
 * -------------------------------------------------------------------------- */
typedef struct {
    char* datos;             /* Texto generado */
    int   longitud;          /* Caracteres usados (sin el '\0') */
    int   capacidad;         /* Capacidad reservada */
} BufferTexto;

static void buffer_iniciar(BufferTexto* b) {
    b->capacidad = 256;
    b->longitud  = 0;
    b->datos     = (char*)malloc(b->capacidad);
    b->datos[0]  = '\0';
}

static void buffer_agregar(BufferTexto* b, const char* s) {
    int largo = (int)strlen(s);
    if (b->longitud + largo + 1 > b->capacidad) {
        while (b->longitud + largo + 1 > b->capacidad)
            b->capacidad *= 2;
        b->datos = (char*)realloc(b->datos, b->capacidad);
    }
    memcpy(b->datos + b->longitud, s, largo + 1);
    b->longitud += largo;
}

/* Agrega la indentacion (4 espacios por nivel) */
static void buffer_indentar(BufferTexto* b, int nivel) {
    for (int i = 0; i < nivel; i++)
        buffer_agregar(b, "    ");
}

/* --------------------------------------------------------------------------
 *  Traduccion de operadores
 * -------------------------------------------------------------------------- */
static const char* traducir_operador(const char* op) {
    if (!op) return op;
    if (strcmp(op, "&&") == 0) return "and";
    if (strcmp(op, "||") == 0) return "or";
    if (strcmp(op, "!") == 0)   return "not";
    /* El resto de operadores son identicos en Python */
    return op;
}

/* --------------------------------------------------------------------------
 *  Declaraciones forward
 * -------------------------------------------------------------------------- */
static void generar_expresion(NodoAST* nodo, BufferTexto* b);
static void generar_sentencia(NodoAST* nodo, int nivel, BufferTexto* b);

/* Indica si un nodo de expresion necesita parentesis (como operando de !) */
static bool expresion_necesita_parentesis(NodoAST* nodo) {
    return (nodo && (nodo->tipo == AST_EXPR_BINARIA ||
                     nodo->tipo == AST_EXPR_UNARIA));
}

/**
 * generar_expresion()
 * Emite la representacion Python de un nodo de expresion.
 * Nota: dentro de expresiones, ++/-- se aproximan con operador morsa (:=)..
 */
static void generar_expresion(NodoAST* nodo, BufferTexto* b) {
    if (!nodo) return;
    
    switch (nodo->tipo) {
        case AST_IDENTIFICADOR:
        case AST_NUMERO:
        case AST_NUMERO_DECIMAL:
            buffer_agregar(b, nodo->valor);
            break;
            
        case AST_CONSTANTE_BOOL:
            buffer_agregar(b, (strcmp(nodo->valor, "true") == 0) ? "True"
                                                                 : "False");
            break;
            
        case AST_EXPR_BINARIA:
            buffer_agregar(b, "(");
            generar_expresion(nodo->hijo1, b);
            buffer_agregar(b, " ");
            buffer_agregar(b, traducir_operador(nodo->valor));
            buffer_agregar(b, " ");
            generar_expresion(nodo->hijo2, b);
            buffer_agregar(b, ")");
            break;
            
        case AST_EXPR_UNARIA: {
            const char* op = nodo->valor;
            
            /* Negacion logica: !  →  not */
            if (op && strcmp(op, "!") == 0) {
                buffer_agregar(b, "not ");
                if (expresion_necesita_parentesis(nodo->hijo1))
                    buffer_agregar(b, "(");
                generar_expresion(nodo->hijo1, b);
                if (expresion_necesita_parentesis(nodo->hijo1))
                    buffer_agregar(b, ")");
                break;
            }
            
            /* Incremento/decremento dentro de una expresion: aproximacion
             * con operador morsa (:=) para conservar el valor y el efecto.
             * Por ejemplo: x++ -> (x := x + 1) - 1 ; ++x -> (x := x + 1) */
            if (op && (strcmp(op, "++") == 0 || strcmp(op, "--") == 0)) {
                bool postfijo = (nodo->hijo2 != NULL);
                const char* var = nodo->hijo1 ? nodo->hijo1->valor : "?";
                buffer_agregar(b, "(");
                buffer_agregar(b, "(");
                buffer_agregar(b, var);
                buffer_agregar(b, " := ");
                buffer_agregar(b, var);
                buffer_agregar(b, " ");
                buffer_agregar(b, (strcmp(op, "++") == 0) ? "+" : "-");
                buffer_agregar(b, " 1)");
                if (postfijo) {
                    buffer_agregar(b, " ");
                    buffer_agregar(b, (strcmp(op, "++") == 0) ? "-" : "+");
                    buffer_agregar(b, " 1");
                }
                buffer_agregar(b, ")");
                break;
            }
            
            /* Cualquier otro unario: emitir el operando */
            generar_expresion(nodo->hijo1, b);
            break;
        }
            
        default:
            /* Nodo no esperado en una expresion */
            buffer_agregar(b, "(?)");
            break;
    }
}

/* --------------------------------------------------------------------------
 *  Utilidades para el 'for'
 * -------------------------------------------------------------------------- */

/* Busca si el cuerpo de un for contiene 'continue' (para nota al traducir) */
static bool contiene_continue(NodoAST* nodo) {
    if (!nodo) return false;
    if (nodo->tipo == AST_CONTINUE) return true;
    return contiene_continue(nodo->hijo1) || contiene_continue(nodo->hijo2) ||
           contiene_continue(nodo->hijo3) || contiene_continue(nodo->cuerpo) ||
           contiene_continue(nodo->siguiente);
}

/* Renderiza un componente de la cabecera del for como aparecia en el
 * codigo fuente (para el comentario de equivalencia). */
static void renderizar_componente_for(NodoAST* nodo, BufferTexto* b) {
    if (!nodo) return;
    
    if (nodo->tipo == AST_ASIGNACION) {
        generar_expresion(nodo->hijo1, b);
        buffer_agregar(b, " ");
        buffer_agregar(b, traducir_operador(nodo->valor));
        buffer_agregar(b, " ");
        generar_expresion(nodo->hijo2, b);
        return;
    }
    
    if (nodo->tipo == AST_EXPR_UNARIA) {
        const char* op = nodo->valor;
        if (op && (strcmp(op, "++") == 0 || strcmp(op, "--") == 0)) {
            bool postfijo = (nodo->hijo2 != NULL);
            if (postfijo) {
                generar_expresion(nodo->hijo1, b);
                buffer_agregar(b, op);
            } else {
                buffer_agregar(b, op);
                generar_expresion(nodo->hijo1, b);
            }
            return;
        }
    }
    
    generar_expresion(nodo, b);
}

/* Emite un componente de la cabecera del for como sentencia Python
 * (la forma simple para ++/-- es += 1 / -= 1). */
static void generar_componente_for_sentencia(NodoAST* nodo, int nivel,
                                             BufferTexto* b) {
    if (!nodo) return;
    
    buffer_indentar(b, nivel);
    
    if (nodo->tipo == AST_ASIGNACION) {
        generar_expresion(nodo->hijo1, b);
        buffer_agregar(b, " ");
        buffer_agregar(b, traducir_operador(nodo->valor));
        buffer_agregar(b, " ");
        generar_expresion(nodo->hijo2, b);
        buffer_agregar(b, "\n");
        return;
    }
    
    if (nodo->tipo == AST_EXPR_UNARIA) {
        const char* op = nodo->valor;
        if (op && (strcmp(op, "++") == 0 || strcmp(op, "--") == 0)) {
            const char* var = nodo->hijo1 ? nodo->hijo1->valor : "?";
            buffer_agregar(b, var);
            buffer_agregar(b, (strcmp(op, "++") == 0) ? " += 1\n" : " -= 1\n");
            return;
        }
    }
    
    generar_expresion(nodo, b);
    buffer_agregar(b, "\n");
}

/* --------------------------------------------------------------------------
 *  Declaraciones hook para sentencias anidadas
 * -------------------------------------------------------------------------- */
static void generar_lista_sentencias(NodoAST* lista, int nivel, BufferTexto* b);

/**
 * generar_sentencia()
 * Emite una sentencia completa en Python (incluye el salto de linea final).
 */
static void generar_sentencia(NodoAST* nodo, int nivel, BufferTexto* b) {
    if (!nodo) return;
    
    switch (nodo->tipo) {
        case AST_PROGRAMA:
        case AST_BLOQUE:
            generar_lista_sentencias(nodo->hijo1, nivel, b);
            break;
            
        case AST_SENTENCIA_IF: {
            /* Cadena if / elif / else */
            NodoAST* actual = nodo;
            const char* palabra = "if";
            while (actual) {
                buffer_indentar(b, nivel);
                buffer_agregar(b, palabra);
                buffer_agregar(b, " ");
                generar_expresion(actual->hijo1, b);
                buffer_agregar(b, ":\n");
                generar_sentencia(actual->hijo2, nivel + 1, b);
                
                if (actual->hijo3 && actual->hijo3->tipo == AST_SENTENCIA_IF) {
                    actual = actual->hijo3;
                    palabra = "elif";
                } else {
                    break;
                }
            }
            
            if (actual && actual->hijo3) {
                buffer_indentar(b, nivel);
                buffer_agregar(b, "else:\n");
                generar_sentencia(actual->hijo3, nivel + 1, b);
            }
            break;
        }
            
        case AST_SENTENCIA_WHILE:
            buffer_indentar(b, nivel);
            buffer_agregar(b, "while ");
            generar_expresion(nodo->hijo1, b);
            buffer_agregar(b, ":\n");
            generar_sentencia(nodo->hijo2, nivel + 1, b);
            break;
            
        case AST_SENTENCIA_FOR: {
            /* for (init; cond; incr)  →  init; while cond: cuerpo; incr */
            buffer_indentar(b, nivel);
            buffer_agregar(b, "# Equivalente a: for ( ");
            renderizar_componente_for(nodo->hijo1, b);
            buffer_agregar(b, " ; ");
            renderizar_componente_for(nodo->hijo2, b);
            buffer_agregar(b, " ; ");
            renderizar_componente_for(nodo->hijo3, b);
            buffer_agregar(b, " )\n");
            
            if (contiene_continue(nodo->cuerpo)) {
                buffer_indentar(b, nivel);
                buffer_agregar(b, "# Nota: un 'continue' en este for salta el "
                                  "incremento (equivalencia aproximada)\n");
            }
            
            generar_componente_for_sentencia(nodo->hijo1, nivel, b);
            
            buffer_indentar(b, nivel);
            buffer_agregar(b, "while ");
            generar_expresion(nodo->hijo2, b);
            buffer_agregar(b, ":\n");
            generar_sentencia(nodo->cuerpo, nivel + 1, b);
            generar_componente_for_sentencia(nodo->hijo3, nivel + 1, b);
            break;
        }
            
        case AST_SENTENCIA_DO_WHILE: {
            /* do { cuerpo } while (cond)
             *   →  while True: cuerpo; if not (cond): break */
            buffer_indentar(b, nivel);
            buffer_agregar(b, "# Equivalente a: do { ... } while ( ");
            generar_expresion(nodo->hijo2, b);
            buffer_agregar(b, " )\n");
            buffer_indentar(b, nivel);
            buffer_agregar(b, "while True:\n");
            generar_sentencia(nodo->hijo1, nivel + 1, b);
            buffer_indentar(b, nivel + 1);
            buffer_agregar(b, "if not (");
            generar_expresion(nodo->hijo2, b);
            buffer_agregar(b, "):\n");
            buffer_indentar(b, nivel + 2);
            buffer_agregar(b, "break\n");
            break;
        }
            
        case AST_DECLARACION: {
            /* int x = 10;  →  x = 10
             * int x;       →  # x (declarado sin inicializar) */
            buffer_indentar(b, nivel);
            char* id = nodo->hijo1 ? nodo->hijo1->valor : NULL;
            if (nodo->hijo2) {
                buffer_agregar(b, id);
                buffer_agregar(b, " = ");
                generar_expresion(nodo->hijo2, b);
                buffer_agregar(b, "\n");
            } else if (id) {
                buffer_agregar(b, "# ");
                buffer_agregar(b, id);
                buffer_agregar(b, " (declarado sin inicializar)\n");
            }
            break;
        }
            
        case AST_ASIGNACION:
            buffer_indentar(b, nivel);
            generar_expresion(nodo->hijo1, b);
            buffer_agregar(b, " ");
            buffer_agregar(b, traducir_operador(nodo->valor));
            buffer_agregar(b, " ");
            generar_expresion(nodo->hijo2, b);
            buffer_agregar(b, "\n");
            break;
            
        case AST_SENTENCIA_EXPR: {
            /* x++;  →  x += 1   (forma simple como sentencia) */
            NodoAST* expr = nodo->hijo1;
            if (expr && expr->tipo == AST_EXPR_UNARIA &&
                expr->valor &&
                (strcmp(expr->valor, "++") == 0 ||
                 strcmp(expr->valor, "--") == 0)) {
                const char* var = expr->hijo1 ? expr->hijo1->valor : "?";
                buffer_indentar(b, nivel);
                buffer_agregar(b, var);
                buffer_agregar(b, (strcmp(expr->valor, "++") == 0)
                                      ? " += 1\n" : " -= 1\n");
                break;
            }
            buffer_indentar(b, nivel);
            generar_expresion(expr, b);
            buffer_agregar(b, "\n");
            break;
        }
            
        case AST_BREAK:
            buffer_indentar(b, nivel);
            buffer_agregar(b, "break\n");
            break;
            
        case AST_CONTINUE:
            buffer_indentar(b, nivel);
            buffer_agregar(b, "continue\n");
            break;
            
        case AST_RETURN:
            /* A nivel de modulo Python no permite 'return'; se emite
             * como comentario para que el codigo generado sea valido. */
            buffer_indentar(b, nivel);
            buffer_agregar(b, "# return");
            if (nodo->hijo1) {
                buffer_agregar(b, " ");
                generar_expresion(nodo->hijo1, b);
            }
            buffer_agregar(b, "\n");
            break;
            
        default:
            buffer_indentar(b, nivel);
            buffer_agregar(b, "# (sentencia no traducible)\n");
            break;
    }
}

/**
 * generar_lista_sentencias()
 * Recorre una lista de sentencias encadenadas por 'siguiente'.
 */
static void generar_lista_sentencias(NodoAST* lista, int nivel, BufferTexto* b) {
    while (lista) {
        generar_sentencia(lista, nivel, b);
        lista = lista->siguiente;
    }
}

/* --------------------------------------------------------------------------
 *  Funcion publica: generar_codigo_python()
 * -------------------------------------------------------------------------- */
char* generar_codigo_python(NodoAST* raiz) {
    if (!raiz) return NULL;
    
    BufferTexto b;
    buffer_iniciar(&b);
    
    /* Cabecera del archivo generado */
    buffer_agregar(&b,
        "# =============================================\n"
        "# Codigo generado automaticamente por el\n"
        "# Traductor mini-C  ->  Python\n"
        "# (Analizador Lexico, Sintactico y Semantico)\n"
        "# =============================================\n\n");
    
    /* Traducir toda la sentencia raiz (que es un programa) */
    generar_sentencia(raiz, 0, &b);
    
    return b.datos;
}
