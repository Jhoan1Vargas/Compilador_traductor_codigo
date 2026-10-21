/* ==========================================================================
 * parser.c  —  Implementacion EXTENDIDA del Analizador Sintactico
 *
 * Parser Recursivo Descendente (Recursive Descent Parser)
 * Siguiendo los principios del Capitulo 4 de Aho, Sethi, Ullman
 *
 * EXTENSION: Soporta while, for, do-while, declaraciones, break, continue,
 * return, operadores logicos, relacionales, incremento/decremento, etc.
 * ========================================================================== */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 *  Estado interno del parser
 * -------------------------------------------------------------------------- */
static Token* tokens_globales = NULL;
static int num_tokens_globales = 0;
static int token_actual_idx = 0;
static char buffer_error[512];
static int contador_errores = 0;

/* --------------------------------------------------------------------------
 *  Funciones auxiliares del parser
 * -------------------------------------------------------------------------- */

static NodoAST* crear_nodo(TipoNodoAST tipo, const char* valor) {
    NodoAST* nodo = (NodoAST*)malloc(sizeof(NodoAST));
    if (!nodo) return NULL;
    
    nodo->tipo = tipo;
    nodo->valor = valor ? strdup(valor) : NULL;
    nodo->hijo1 = NULL;
    nodo->hijo2 = NULL;
    nodo->hijo3 = NULL;
    nodo->cuerpo = NULL;
    nodo->siguiente = NULL;
    nodo->linea = token_actual_idx;
    
    return nodo;
}

static const char* token_actual_tipo() {
    if (token_actual_idx >= num_tokens_globales)
        return NULL;
    return tokens_globales[token_actual_idx].tipo;
}

static const char* token_actual_lexema() {
    if (token_actual_idx >= num_tokens_globales)
        return NULL;
    return tokens_globales[token_actual_idx].lexema;
}

static void avanzar() {
    if (token_actual_idx < num_tokens_globales)
        token_actual_idx++;
}

static bool coincidir(const char* tipo_esperado) {
    const char* tipo = token_actual_tipo();
    
    if (tipo == NULL) {
        snprintf(buffer_error, sizeof(buffer_error),
                 "Error: Se esperaba '%s' pero se llego al final del codigo",
                 tipo_esperado);
        contador_errores++;
        return false;
    }
    
    if (strcmp(tipo, tipo_esperado) == 0) {
        avanzar();
        return true;
    }
    
    snprintf(buffer_error, sizeof(buffer_error),
             "Error: Se esperaba '%s' pero se encontro '%s' (%s) en token %d",
             tipo_esperado, tipo, token_actual_lexema(), token_actual_idx + 1);
    contador_errores++;
    return false;
}

static bool es_tipo(const char* tipo) {
    const char* actual = token_actual_tipo();
    return (actual != NULL && strcmp(actual, tipo) == 0);
}

static bool es_lexema(const char* lexema) {
    const char* actual = token_actual_lexema();
    return (actual != NULL && strcmp(actual, lexema) == 0);
}

/* --------------------------------------------------------------------------
 *  Declaraciones forward de funciones de parsing
 * -------------------------------------------------------------------------- */
static NodoAST* parsear_sentencia();
static NodoAST* parsear_expresion();
static NodoAST* parsear_expr_logica();

/* --------------------------------------------------------------------------
 *  Funciones de parsing de expresiones
 * -------------------------------------------------------------------------- */

/**
 * parsear_factor()
 * Factor → identificador [++|--] | numero | numero_decimal | true | false
 *        | (++|--) identificador | ! Factor | ( Expresion )
 */
static NodoAST* parsear_factor() {
    /* Operadores unarios de prefijo: !, ++, -- */
    if (es_tipo("OP_NOT") || es_tipo("OP_INCREMENTO") || es_tipo("OP_DECREMENTO")) {
        const char* operador = token_actual_lexema();
        avanzar();
        NodoAST* operando = parsear_factor();
        NodoAST* nodo = crear_nodo(AST_EXPR_UNARIA, operador);
        nodo->hijo1 = operando;
        return nodo;
    }
    
    /* Identificadores */
    if (es_tipo("IDENTIFICADOR")) {
        const char* lexema = token_actual_lexema();
        avanzar();
        NodoAST* nodo = crear_nodo(AST_IDENTIFICADOR, lexema);
        
        /* Operadores unarios de postfijo: ++, -- */
        if (es_tipo("OP_INCREMENTO") || es_tipo("OP_DECREMENTO")) {
            const char* operador = token_actual_lexema();
            avanzar();
            NodoAST* nodo_post = crear_nodo(AST_EXPR_UNARIA, operador);
            nodo_post->hijo1 = nodo;
            nodo_post->hijo2 = crear_nodo(AST_IDENTIFICADOR, "post"); /* Marca postfijo */
            return nodo_post;
        }
        
        return nodo;
    }
    
    /* Números enteros */
    if (es_tipo("NUMERO_ENTERO")) {
        const char* lexema = token_actual_lexema();
        avanzar();
        return crear_nodo(AST_NUMERO, lexema);
    }
    
    /* Números decimales */
    if (es_tipo("NUMERO_DECIMAL")) {
        const char* lexema = token_actual_lexema();
        avanzar();
        return crear_nodo(AST_NUMERO_DECIMAL, lexema);
    }
    
    /* Constantes booleanas */
    if (es_tipo("CONSTANTE_BOOLEANA")) {
        const char* lexema = token_actual_lexema();
        avanzar();
        return crear_nodo(AST_CONSTANTE_BOOL, lexema);
    }
    
    /* Expresiones entre paréntesis */
    if (es_tipo("PAREN_IZQ")) {
        avanzar();
        NodoAST* expr = parsear_expresion();
        if (!coincidir("PAREN_DER")) {
            liberar_ast(expr);
            return crear_nodo(AST_ERROR, "Falta parentesis derecho");
        }
        return expr;
    }
    
    /* Error: token inesperado */
    snprintf(buffer_error, sizeof(buffer_error),
             "Error: Expresion esperada pero se encontro '%s' en token %d",
             token_actual_lexema(), token_actual_idx + 1);
    contador_errores++;
    return crear_nodo(AST_ERROR, buffer_error);
}

/**
 * parsear_termino()
 * Termino → Factor [(*|/|%) Factor]*
 */
static NodoAST* parsear_termino() {
    NodoAST* izq = parsear_factor();
    
    while (es_tipo("OP_MULT") || es_tipo("OP_DIV") || es_tipo("OP_MODULO")) {
        const char* operador = token_actual_lexema();
        avanzar();
        NodoAST* der = parsear_factor();
        
        NodoAST* nodo_op = crear_nodo(AST_EXPR_BINARIA, operador);
        nodo_op->hijo1 = izq;
        nodo_op->hijo2 = der;
        izq = nodo_op;
    }
    
    return izq;
}

/**
 * parsear_expr_aritmetica()
 * ExprAritmetica → Termino [(+|-) Termino]*
 */
static NodoAST* parsear_expr_aritmetica() {
    NodoAST* izq = parsear_termino();
    
    while (es_tipo("OP_SUMA") || es_tipo("OP_RESTA")) {
        const char* operador = token_actual_lexema();
        avanzar();
        NodoAST* der = parsear_termino();
        
        NodoAST* nodo_op = crear_nodo(AST_EXPR_BINARIA, operador);
        nodo_op->hijo1 = izq;
        nodo_op->hijo2 = der;
        izq = nodo_op;
    }
    
    return izq;
}

/**
 * parsear_expr_comparacion()
 * ExprComparacion → ExprAritmetica [(==|!=|<|>|<=|>=) ExprAritmetica]*
 */
static NodoAST* parsear_expr_comparacion() {
    NodoAST* izq = parsear_expr_aritmetica();
    
    while (es_tipo("OP_IGUALDAD") || es_tipo("OP_DIFERENTE") ||
           es_tipo("OP_MENOR") || es_tipo("OP_MAYOR") ||
           es_tipo("OP_MENOR_IGUAL") || es_tipo("OP_MAYOR_IGUAL")) {
        const char* operador = token_actual_lexema();
        avanzar();
        NodoAST* der = parsear_expr_aritmetica();
        
        NodoAST* nodo_op = crear_nodo(AST_EXPR_BINARIA, operador);
        nodo_op->hijo1 = izq;
        nodo_op->hijo2 = der;
        izq = nodo_op;
    }
    
    return izq;
}

/**
 * parsear_expr_logica()
 * ExprLogica → ExprComparacion [(&&|||) ExprComparacion]*
 */
static NodoAST* parsear_expr_logica() {
    NodoAST* izq = parsear_expr_comparacion();
    
    while (es_tipo("OP_AND") || es_tipo("OP_OR")) {
        const char* operador = token_actual_lexema();
        avanzar();
        NodoAST* der = parsear_expr_comparacion();
        
        NodoAST* nodo_op = crear_nodo(AST_EXPR_BINARIA, operador);
        nodo_op->hijo1 = izq;
        nodo_op->hijo2 = der;
        izq = nodo_op;
    }
    
    return izq;
}

/**
 * parsear_expresion()
 * Expresion → ExprLogica
 */
static NodoAST* parsear_expresion() {
    return parsear_expr_logica();
}

/* --------------------------------------------------------------------------
 *  Funciones de parsing de sentencias
 * -------------------------------------------------------------------------- */

/**
 * parsear_bloque()
 * Bloque → { ListaSentencias }
 */
static NodoAST* parsear_bloque() {
    if (!coincidir("LLAVE_IZQ")) {
        return crear_nodo(AST_ERROR, "Se esperaba '{'");
    }
    
    NodoAST* bloque = crear_nodo(AST_BLOQUE, "bloque");
    NodoAST* lista_sentencias = NULL;
    NodoAST* ultima_sentencia = NULL;
    
    while (!es_tipo("LLAVE_DER") && token_actual_tipo() != NULL) {
        NodoAST* sent = parsear_sentencia();
        if (sent) {
            if (lista_sentencias == NULL) {
                lista_sentencias = sent;
                ultima_sentencia = sent;
            } else {
                ultima_sentencia->siguiente = sent;
                ultima_sentencia = sent;
            }
        }
    }
    
    if (!coincidir("LLAVE_DER")) {
        liberar_ast(bloque);
        liberar_ast(lista_sentencias);
        return crear_nodo(AST_ERROR, "Se esperaba '}'");
    }
    
    bloque->hijo1 = lista_sentencias;
    return bloque;
}

/**
 * parsear_sentencia_if()
 * SentenciaIf → if ( Expresion ) Sentencia [else Sentencia]
 */
static NodoAST* parsear_sentencia_if() {
    avanzar(); /* Consumir 'if' */
    
    if (!coincidir("PAREN_IZQ")) {
        return crear_nodo(AST_ERROR, "Se esperaba '(' despues de 'if'");
    }
    
    NodoAST* condicion = parsear_expresion();
    
    if (!coincidir("PAREN_DER")) {
        liberar_ast(condicion);
        return crear_nodo(AST_ERROR, "Se esperaba ')' despues de la condicion");
    }
    
    NodoAST* sentencia_then = parsear_sentencia();
    NodoAST* sentencia_else = NULL;
    
    /* Parte opcional: else */
    if (es_tipo("PALABRA_RESERVADA") && es_lexema("else")) {
        avanzar();
        sentencia_else = parsear_sentencia();
    }
    
    NodoAST* nodo_if = crear_nodo(AST_SENTENCIA_IF, "if");
    nodo_if->hijo1 = condicion;
    nodo_if->hijo2 = sentencia_then;
    nodo_if->hijo3 = sentencia_else;
    
    return nodo_if;
}

/**
 * parsear_sentencia_while()
 * SentenciaWhile → while ( Expresion ) Sentencia
 */
static NodoAST* parsear_sentencia_while() {
    avanzar(); /* Consumir 'while' */
    
    if (!coincidir("PAREN_IZQ")) {
        return crear_nodo(AST_ERROR, "Se esperaba '(' despues de 'while'");
    }
    
    NodoAST* condicion = parsear_expresion();
    
    if (!coincidir("PAREN_DER")) {
        liberar_ast(condicion);
        return crear_nodo(AST_ERROR, "Se esperaba ')' despues de la condicion");
    }
    
    NodoAST* cuerpo = parsear_sentencia();
    
    NodoAST* nodo_while = crear_nodo(AST_SENTENCIA_WHILE, "while");
    nodo_while->hijo1 = condicion;
    nodo_while->hijo2 = cuerpo;
    
    return nodo_while;
}

/**
 * parsear_sentencia_do_while()
 * SentenciaDoWhile → do Sentencia while ( Expresion ) ;
 */
static NodoAST* parsear_sentencia_do_while() {
    avanzar(); /* Consumir 'do' */
    
    NodoAST* cuerpo = parsear_sentencia();
    
    if (!es_tipo("PALABRA_RESERVADA") || !es_lexema("while")) {
        liberar_ast(cuerpo);
        return crear_nodo(AST_ERROR, "Se esperaba 'while' despues del cuerpo de 'do'");
    }
    avanzar();
    
    if (!coincidir("PAREN_IZQ")) {
        liberar_ast(cuerpo);
        return crear_nodo(AST_ERROR, "Se esperaba '(' despues de 'while'");
    }
    
    NodoAST* condicion = parsear_expresion();
    
    if (!coincidir("PAREN_DER")) {
        liberar_ast(cuerpo);
        liberar_ast(condicion);
        return crear_nodo(AST_ERROR, "Se esperaba ')' despues de la condicion");
    }
    
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(cuerpo);
        liberar_ast(condicion);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de 'do-while'");
    }
    
    NodoAST* nodo_do = crear_nodo(AST_SENTENCIA_DO_WHILE, "do-while");
    nodo_do->hijo1 = cuerpo;
    nodo_do->hijo2 = condicion;
    
    return nodo_do;
}

/**
 * parsear_asignacion_sin_separador()
 * AsignacionSinSep → identificador OpAsignacion Expresion   (sin el ';')
 * Usada en la cabecera del 'for' (inicializacion e incremento).
 * Devuelve NULL si el token actual no inicia una asignacion.
 */
static NodoAST* parsear_asignacion_sin_separador() {
    if (es_tipo("IDENTIFICADOR")) {
        int posicion_guardada = token_actual_idx;
        const char* identificador = token_actual_lexema();
        avanzar();
        
        if (es_tipo("OP_ASIGNACION") || es_tipo("OP_ASIG_SUMA") ||
            es_tipo("OP_ASIG_RESTA") || es_tipo("OP_ASIG_MULT") ||
            es_tipo("OP_ASIG_DIV")) {
            const char* operador = token_actual_lexema();
            avanzar();
            
            NodoAST* expresion = parsear_expresion();
            NodoAST* nodo_asig = crear_nodo(AST_ASIGNACION, operador);
            nodo_asig->hijo1 = crear_nodo(AST_IDENTIFICADOR, identificador);
            nodo_asig->hijo2 = expresion;
            return nodo_asig;
        }
        
        /* No era asignacion: retroceder */
        token_actual_idx = posicion_guardada;
    }
    return NULL;
}

/**
 * parsear_sentencia_for()
 * SentenciaFor → for ( Expresion ; Expresion ; Expresion ) Sentencia
 * (Admite asignaciones (x = 0) o expresiones en inicializacion e incremento;
 *  el cuerpo se guarda en el campo 'cuerpo' del nodo)
 */
static NodoAST* parsear_sentencia_for() {
    avanzar(); /* Consumir 'for' */
    
    if (!coincidir("PAREN_IZQ")) {
        return crear_nodo(AST_ERROR, "Se esperaba '(' despues de 'for'");
    }
    
    /* Inicializacion (puede ser asignacion, expresion o vacia) */
    NodoAST* inicializacion = NULL;
    if (!es_tipo("PUNTO_Y_COMA")) {
        inicializacion = parsear_asignacion_sin_separador();
        if (!inicializacion) inicializacion = parsear_expresion();
    }
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(inicializacion);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de inicializacion en 'for'");
    }
    
    /* Condicion */
    NodoAST* condicion = NULL;
    if (!es_tipo("PUNTO_Y_COMA")) {
        condicion = parsear_expresion();
    }
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(inicializacion);
        liberar_ast(condicion);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de condicion en 'for'");
    }
    
    /* Incremento (puede ser asignacion, expresion o vacia) */
    NodoAST* incremento = NULL;
    if (!es_tipo("PAREN_DER")) {
        incremento = parsear_asignacion_sin_separador();
        if (!incremento) incremento = parsear_expresion();
    }
    if (!coincidir("PAREN_DER")) {
        liberar_ast(inicializacion);
        liberar_ast(condicion);
        liberar_ast(incremento);
        return crear_nodo(AST_ERROR, "Se esperaba ')' despues de incremento en 'for'");
    }
    
    /* Cuerpo */
    NodoAST* cuerpo = parsear_sentencia();
    
    NodoAST* nodo_for = crear_nodo(AST_SENTENCIA_FOR, "for");
    nodo_for->hijo1 = inicializacion;
    nodo_for->hijo2 = condicion;
    nodo_for->hijo3 = incremento;
    nodo_for->cuerpo = cuerpo;
    
    return nodo_for;
}

/**
 * parsear_declaracion()
 * Declaracion → tipo identificador [= Expresion] ;
 */
static NodoAST* parsear_declaracion() {
    const char* tipo = token_actual_lexema();
    avanzar(); /* Consumir tipo (int, float, etc.) */
    
    if (!es_tipo("IDENTIFICADOR")) {
        return crear_nodo(AST_ERROR, "Se esperaba identificador despues del tipo");
    }
    
    const char* identificador = token_actual_lexema();
    avanzar();
    
    NodoAST* nodo_decl = crear_nodo(AST_DECLARACION, tipo);
    nodo_decl->hijo1 = crear_nodo(AST_IDENTIFICADOR, identificador);
    
    /* Inicializacion opcional */
    if (es_tipo("OP_ASIGNACION")) {
        avanzar();
        NodoAST* valor_inicial = parsear_expresion();
        nodo_decl->hijo2 = valor_inicial;
    }
    
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(nodo_decl);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de la declaracion");
    }
    
    return nodo_decl;
}

/**
 * parsear_asignacion_o_expresion()
 * Asignacion → identificador OpAsignacion Expresion ;
 * SentenciaExpr → Expresion ;
 */
static NodoAST* parsear_asignacion_o_expresion() {
    /* Guardar posicion por si necesitamos retroceder */
    int posicion_guardada = token_actual_idx;
    
    /* Intentar parsear como asignacion */
    if (es_tipo("IDENTIFICADOR")) {
        const char* identificador = token_actual_lexema();
        avanzar();
        
        /* Verificar si es asignacion (=, +=, -=, etc.) */
        if (es_tipo("OP_ASIGNACION") || es_tipo("OP_ASIG_SUMA") ||
            es_tipo("OP_ASIG_RESTA") || es_tipo("OP_ASIG_MULT") || es_tipo("OP_ASIG_DIV")) {
            const char* operador = token_actual_lexema();
            avanzar();
            
            NodoAST* expresion = parsear_expresion();
            
            if (!coincidir("PUNTO_Y_COMA")) {
                liberar_ast(expresion);
                return crear_nodo(AST_ERROR, "Se esperaba ';' despues de la asignacion");
            }
            
            NodoAST* nodo_asig = crear_nodo(AST_ASIGNACION, operador);
            nodo_asig->hijo1 = crear_nodo(AST_IDENTIFICADOR, identificador);
            nodo_asig->hijo2 = expresion;
            
            return nodo_asig;
        }
    }
    
    /* Si no es asignacion, retroceder y parsear como expresion */
    token_actual_idx = posicion_guardada;
    NodoAST* expresion = parsear_expresion();
    
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(expresion);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de la expresion");
    }
    
    NodoAST* nodo_expr = crear_nodo(AST_SENTENCIA_EXPR, "expr");
    nodo_expr->hijo1 = expresion;
    
    return nodo_expr;
}

/**
 * parsear_break()
 * Break → break ;
 */
static NodoAST* parsear_break() {
    avanzar(); /* Consumir 'break' */
    
    if (!coincidir("PUNTO_Y_COMA")) {
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de 'break'");
    }
    
    return crear_nodo(AST_BREAK, "break");
}

/**
 * parsear_continue()
 * Continue → continue ;
 */
static NodoAST* parsear_continue() {
    avanzar(); /* Consumir 'continue' */
    
    if (!coincidir("PUNTO_Y_COMA")) {
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de 'continue'");
    }
    
    return crear_nodo(AST_CONTINUE, "continue");
}

/**
 * parsear_return()
 * Return → return [Expresion] ;
 */
static NodoAST* parsear_return() {
    avanzar(); /* Consumir 'return' */
    
    NodoAST* nodo_return = crear_nodo(AST_RETURN, "return");
    
    /* Expresion opcional */
    if (!es_tipo("PUNTO_Y_COMA")) {
        NodoAST* expresion = parsear_expresion();
        nodo_return->hijo1 = expresion;
    }
    
    if (!coincidir("PUNTO_Y_COMA")) {
        liberar_ast(nodo_return);
        return crear_nodo(AST_ERROR, "Se esperaba ';' despues de 'return'");
    }
    
    return nodo_return;
}

/**
 * parsear_sentencia()
 * Sentencia → SentenciaIf | SentenciaWhile | SentenciaFor | SentenciaDoWhile
 *           | Declaracion | Asignacion | SentenciaExpr | Break | Continue
 *           | Return | Bloque
 */
static NodoAST* parsear_sentencia() {
    /* Bloque */
    if (es_tipo("LLAVE_IZQ")) {
        return parsear_bloque();
    }
    
    /* Palabras reservadas */
    if (es_tipo("PALABRA_RESERVADA")) {
        if (es_lexema("if")) return parsear_sentencia_if();
        if (es_lexema("while")) return parsear_sentencia_while();
        if (es_lexema("for")) return parsear_sentencia_for();
        if (es_lexema("do")) return parsear_sentencia_do_while();
        if (es_lexema("break")) return parsear_break();
        if (es_lexema("continue")) return parsear_continue();
        if (es_lexema("return")) return parsear_return();
    }
    
    /* Declaraciones (int, float, etc.) */
    if (es_tipo("TIPO_DATO")) {
        return parsear_declaracion();
    }
    
    /* Asignacion o expresion */
    if (es_tipo("IDENTIFICADOR") || es_tipo("NUMERO_ENTERO") || es_tipo("NUMERO_DECIMAL") ||
        es_tipo("OP_INCREMENTO") || es_tipo("OP_DECREMENTO") || es_tipo("PAREN_IZQ")) {
        return parsear_asignacion_o_expresion();
    }
    
    /* Error: sentencia no reconocida */
    snprintf(buffer_error, sizeof(buffer_error),
             "Error: Sentencia inesperada en token %d (%s)",
             token_actual_idx + 1, token_actual_lexema());
    contador_errores++;
    avanzar(); /* Intentar recuperarse */
    return crear_nodo(AST_ERROR, buffer_error);
}

/**
 * parsear_programa()
 * Programa → ListaSentencias
 */
static NodoAST* parsear_programa() {
    NodoAST* programa = crear_nodo(AST_PROGRAMA, "programa");
    NodoAST* lista_sentencias = NULL;
    NodoAST* ultima_sentencia = NULL;
    
    while (token_actual_tipo() != NULL) {
        NodoAST* sent = parsear_sentencia();
        if (sent) {
            if (lista_sentencias == NULL) {
                lista_sentencias = sent;
                ultima_sentencia = sent;
            } else {
                ultima_sentencia->siguiente = sent;
                ultima_sentencia = sent;
            }
        }
    }
    
    programa->hijo1 = lista_sentencias;
    return programa;
}

/* --------------------------------------------------------------------------
 *  Funcion publica: analizar_sintaxis()
 * -------------------------------------------------------------------------- */
ResultadoParser analizar_sintaxis(Token* tokens, int num_tokens) {
    /* Inicializar estado del parser */
    tokens_globales = tokens;
    num_tokens_globales = num_tokens;
    token_actual_idx = 0;
    contador_errores = 0;
    buffer_error[0] = '\0';
    
    /* Parsear el programa */
    NodoAST* raiz = parsear_programa();
    
    /* Crear resultado */
    ResultadoParser resultado;
    resultado.raiz = raiz;
    resultado.exito = (contador_errores == 0);
    resultado.num_errores = contador_errores;
    
    if (contador_errores > 0) {
        resultado.mensaje_error = strdup(buffer_error);
    } else {
        resultado.mensaje_error = NULL;
    }
    
    return resultado;
}

/* --------------------------------------------------------------------------
 *  Funciones de utilidad para el AST
 * -------------------------------------------------------------------------- */

void liberar_ast(NodoAST* nodo) {
    if (!nodo) return;
    
    liberar_ast(nodo->hijo1);
    liberar_ast(nodo->hijo2);
    liberar_ast(nodo->hijo3);
    liberar_ast(nodo->cuerpo);
    liberar_ast(nodo->siguiente);
    
    if (nodo->valor) free(nodo->valor);
    free(nodo);
}

const char* obtener_tipo_nodo_str(TipoNodoAST tipo) {
    switch (tipo) {
        case AST_PROGRAMA: return "Programa";
        case AST_SENTENCIA_IF: return "If";
        case AST_SENTENCIA_WHILE: return "While";
        case AST_SENTENCIA_FOR: return "For";
        case AST_SENTENCIA_DO_WHILE: return "Do-While";
        case AST_DECLARACION: return "Declaracion";
        case AST_ASIGNACION: return "Asignacion";
        case AST_SENTENCIA_EXPR: return "Expresion";
        case AST_BREAK: return "Break";
        case AST_CONTINUE: return "Continue";
        case AST_RETURN: return "Return";
        case AST_BLOQUE: return "Bloque";
        case AST_EXPR_BINARIA: return "ExprBinaria";
        case AST_EXPR_UNARIA: return "ExprUnaria";
        case AST_IDENTIFICADOR: return "Identificador";
        case AST_NUMERO: return "Numero";
        case AST_NUMERO_DECIMAL: return "NumeroDecimal";
        case AST_CONSTANTE_BOOL: return "ConstanteBool";
        case AST_LISTA_SENTENCIAS: return "ListaSentencias";
        case AST_ERROR: return "ERROR";
        default: return "Desconocido";
    }
}

void imprimir_ast(NodoAST* nodo, int profundidad) {
    if (!nodo) return;
    
    for (int i = 0; i < profundidad; i++) printf("  ");
    
    printf("|-- %s", obtener_tipo_nodo_str(nodo->tipo));
    if (nodo->valor) printf(": %s", nodo->valor);
    printf("\n");
    
    imprimir_ast(nodo->hijo1, profundidad + 1);
    imprimir_ast(nodo->hijo2, profundidad + 1);
    imprimir_ast(nodo->hijo3, profundidad + 1);
    imprimir_ast(nodo->cuerpo, profundidad + 1);
    imprimir_ast(nodo->siguiente, profundidad);
}

void generar_representacion_arbol(NodoAST* nodo, int profundidad,
                                   LineaArbolCallback callback, void* user_data) {
    if (!nodo) return;
    
    char linea[256];
    
    /* Generar indentacion */
    char indent[128] = "";
    for (int i = 0; i < profundidad; i++) {
        strcat(indent, "  ");
    }
    
    /* Generar linea del nodo */
    if (nodo->valor) {
        snprintf(linea, sizeof(linea), "%s|-- %s: %s",
                 indent, obtener_tipo_nodo_str(nodo->tipo), nodo->valor);
    } else {
        snprintf(linea, sizeof(linea), "%s|-- %s",
                 indent, obtener_tipo_nodo_str(nodo->tipo));
    }
    
    /* Invocar callback */
    if (callback) {
        callback(linea, user_data);
    }
    
    /* Recursion */
    generar_representacion_arbol(nodo->hijo1, profundidad + 1, callback, user_data);
    generar_representacion_arbol(nodo->hijo2, profundidad + 1, callback, user_data);
    generar_representacion_arbol(nodo->hijo3, profundidad + 1, callback, user_data);
    generar_representacion_arbol(nodo->cuerpo, profundidad + 1, callback, user_data);
    generar_representacion_arbol(nodo->siguiente, profundidad, callback, user_data);
}
