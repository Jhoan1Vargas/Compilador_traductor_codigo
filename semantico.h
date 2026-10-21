/* ==========================================================================
 * semantico.h  —  Analizador Semantico (Tabla de Simbolos y Tipos)
 *
 * Basado en el Capitulo 6 del Libro "Compiladores" de Aho, Sethi, Ullman
 *
 * El analizador semantico recibe el AST producido por el parser y:
 *   1. Construye una tabla de simbolos (variables con su tipo y estado)
 *   2. Verifica que toda variable este declarada antes de usarse
 *   3. Detecta declaraciones duplicadas
 *   4. Verifica compatibilidad de tipos en asignaciones y expresiones
 *   5. Verifica las condiciones de if/while/for/do-while
 *   6. Valida el uso de operadores (%, ++, --, !, logicos, relacionales)
 * ========================================================================== */

#ifndef SEMANTICO_H
#define SEMANTICO_H

#include <stdbool.h>
#include "parser.h"

/* --------------------------------------------------------------------------
 *  Entrada de la Tabla de Simbolos
 * -------------------------------------------------------------------------- */
typedef struct EntradaSimbolo {
    char* nombre;               /* Nombre de la variable */
    char* tipo;                 /* Tipo: int, float, double, char, bool, void */
    bool inicializada;          /* true si fue inicializada en su declaracion */
    int linea;                  /* Linea donde fue declarada */
    struct EntradaSimbolo* siguiente;  /* Siguiente entrada en la tabla */
} EntradaSimbolo;

/* --------------------------------------------------------------------------
 *  Tabla de Simbolos (lista enlazada)
 * -------------------------------------------------------------------------- */
typedef struct {
    EntradaSimbolo* cabeza;     /* Primera entrada de la tabla */
    int num_simbolos;           /* Numero total de simbolos */
} TablaSimbolos;

/* --------------------------------------------------------------------------
 *  Resultado del analisis semantico
 * -------------------------------------------------------------------------- */
typedef struct {
    bool exito;                 /* true si no hubo errores semanticos */
    int num_errores;            /* Numero de errores encontrados */
    char* mensaje_error;        /* Primer error encontrado (o NULL) */
    TablaSimbolos tabla;        /* Tabla de simbolos construida */
} ResultadoSemantico;

/* --------------------------------------------------------------------------
 *  Funciones publicas del analizador semantico
 * -------------------------------------------------------------------------- */

/**
 * analizar_semantica()
 * 
 * Funcion principal del analizador semantico. Recibe el AST producido
 * por el parser y verifica su significado (declaraciones, tipos).
 * 
 * @param raiz  Raiz del AST (producido por analizar_sintaxis)
 * @return      Resultado con exito, errores y tabla de simbolos
 */
ResultadoSemantico analizar_semantica(NodoAST* raiz);

/**
 * liberar_tabla_simbolos()
 * 
 * Libera la memoria de una tabla de simbolos.
 * 
 * @param tabla  Tabla de simbolos a liberar
 */
void liberar_tabla_simbolos(TablaSimbolos* tabla);

#endif /* SEMANTICO_H */
