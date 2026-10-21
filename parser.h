/* ==========================================================================
 * parser.h  —  Analizador Sintactico Recursivo Descendente
 *
 * Basado en el Capitulo 4 del Libro "Compiladores" de Aho, Sethi, Ullman
 *
 * Implementa un parser recursivo descendente que:
 *   1. Consume tokens del analizador lexico
 *   2. Verifica la gramatica del lenguaje
 *   3. Construye un Arbol de Sintaxis Abstracto (AST)
 *
 * Gramatica del lenguaje (EXTENDIDA):
 *   Programa        → ListaSentencias
 *   ListaSentencias → Sentencia ListaSentencias | ε
 *   Sentencia       → SentenciaIf | SentenciaWhile | SentenciaFor | SentenciaDoWhile
 *                   | Declaracion | Asignacion | SentenciaExpr | Break | Continue 
 *                   | Return | Bloque
 *   SentenciaIf     → if ( Expresion ) Sentencia [else Sentencia]
 *   SentenciaWhile  → while ( Expresion ) Sentencia
 *   SentenciaFor    → for ( Expresion ; Expresion ; Expresion ) Sentencia
 *   SentenciaDoWhile→ do Sentencia while ( Expresion ) ;
 *   Declaracion     → tipo identificador [= Expresion] ;
 *   Asignacion      → identificador OpAsignacion Expresion ;
 *   SentenciaExpr   → Expresion ;
 *   Break           → break ;
 *   Continue        → continue ;
 *   Return          → return [Expresion] ;
 *   Bloque          → { ListaSentencias }
 *   Expresion       → ExprLogica
 *   ExprLogica      → ExprComparacion [(&&|||) ExprComparacion]*
 *   ExprComparacion → ExprAritmetica [(==|!=|<|>|<=|>=) ExprAritmetica]*
 *   ExprAritmetica  → Termino [(+|-) Termino]*
 *   Termino         → Factor [(*|/|%) Factor]*
 *   Factor          → identificador [++|--] | numero | true | false
 *                   | (++|--) identificador | ! Factor | ( Expresion )
 * ========================================================================== */

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

/* --------------------------------------------------------------------------
 *  Tipos de nodos del Arbol de Sintaxis Abstracto (AST)
 * -------------------------------------------------------------------------- */
typedef enum {
    /* Sentencias */
    AST_PROGRAMA,
    AST_SENTENCIA_IF,
    AST_SENTENCIA_WHILE,
    AST_SENTENCIA_FOR,
    AST_SENTENCIA_DO_WHILE,
    AST_DECLARACION,
    AST_ASIGNACION,
    AST_SENTENCIA_EXPR,     /* Expresion como sentencia (ej: x++; ) */
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_BLOQUE,
    
    /* Expresiones */
    AST_EXPR_BINARIA,       /* Operadores binarios: +, -, *, /, ==, !=, <, >, &&, ||, etc. */
    AST_EXPR_UNARIA,        /* Operadores unarios: !, ++, -- (pre/post) */
    AST_IDENTIFICADOR,
    AST_NUMERO,
    AST_NUMERO_DECIMAL,
    AST_CONSTANTE_BOOL,     /* true, false */
    
    /* Otros */
    AST_LISTA_SENTENCIAS,
    AST_ERROR
} TipoNodoAST;

/* --------------------------------------------------------------------------
 *  Estructura de un nodo del AST
 * -------------------------------------------------------------------------- */
typedef struct NodoAST {
    TipoNodoAST tipo;
    char* valor;                    /* Lexema (para identificadores, numeros, operadores) */
    struct NodoAST* hijo1;          /* Hijo izquierdo / primer hijo */
    struct NodoAST* hijo2;          /* Hijo derecho / segundo hijo */
    struct NodoAST* hijo3;          /* Tercer hijo (para if-else) */
    struct NodoAST* cuerpo;         /* Cuerpo del 'for' (para no usar 'siguiente') */
    struct NodoAST* siguiente;      /* Para listas de sentencias */
    int linea;                      /* Numero de linea (para errores) */
} NodoAST;

/* --------------------------------------------------------------------------
 *  Estructura de un Token (recibido del lexer)
 * -------------------------------------------------------------------------- */
typedef struct {
    char* tipo;
    char* lexema;
} Token;

/* --------------------------------------------------------------------------
 *  Resultado del analisis sintactico
 * -------------------------------------------------------------------------- */
typedef struct {
    NodoAST* raiz;               /* Raiz del AST */
    bool exito;                  /* true si el analisis fue exitoso */
    char* mensaje_error;         /* Mensaje de error si fallo */
    int num_errores;             /* Numero de errores encontrados */
} ResultadoParser;

/* --------------------------------------------------------------------------
 *  Funciones publicas del parser
 * -------------------------------------------------------------------------- */

/**
 * analizar_sintaxis()
 * 
 * Funcion principal del parser. Recibe un arreglo de tokens del lexer
 * y devuelve el resultado del analisis sintactico (AST + errores).
 * 
 * @param tokens     Arreglo de tokens del lexer
 * @param num_tokens Numero de tokens en el arreglo
 * @return           Resultado del analisis (AST, exito, errores)
 */
ResultadoParser analizar_sintaxis(Token* tokens, int num_tokens);

/**
 * liberar_ast()
 * 
 * Libera recursivamente toda la memoria del AST.
 * 
 * @param nodo  Nodo raiz del AST a liberar
 */
void liberar_ast(NodoAST* nodo);

/**
 * imprimir_ast()
 * 
 * Imprime el AST en formato de arbol (para depuracion).
 * 
 * @param nodo       Nodo raiz del AST
 * @param profundidad Nivel de indentacion
 */
void imprimir_ast(NodoAST* nodo, int profundidad);

/**
 * obtener_tipo_nodo_str()
 * 
 * Convierte un TipoNodoAST a cadena legible.
 * 
 * @param tipo  Tipo de nodo
 * @return      Cadena con el nombre del tipo
 */
const char* obtener_tipo_nodo_str(TipoNodoAST tipo);

/**
 * generar_representacion_arbol()
 * 
 * Genera una representacion en texto del arbol para mostrar en la GUI.
 * Callback invocado por cada nodo visitado.
 * 
 * @param nodo       Nodo actual del AST
 * @param profundidad Nivel de profundidad
 * @param callback   Funcion callback para cada linea generada
 * @param user_data  Datos de usuario para el callback
 */
typedef void (*LineaArbolCallback)(const char* linea, void* user_data);
void generar_representacion_arbol(NodoAST* nodo, int profundidad, 
                                   LineaArbolCallback callback, void* user_data);

#endif /* PARSER_H */
