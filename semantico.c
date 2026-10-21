/* ==========================================================================
 * semantico.c  -  Implementacion del Analizador Semantico
 *
 * Basado en el Capitulo 6 del Libro "Compiladores" de Aho, Sethi, Ullman
 * (Analisis Semantico y Verificacion de Tipos)
 *
 * Recorre el AST y verifica:
 *   - Declaraciones: tablas de simbolos, duplicados, inicializacion
 *   - Tipos: inferencia y compatibilidad en expresiones y asignaciones
 *   - Operadores: uso correcto (aritmeticos, relacionales, logicos, ++, --)
 * ========================================================================== */

#include "semantico.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 *  Estado interno del analizador semantico
 * -------------------------------------------------------------------------- */
static TablaSimbolos tabla_global;
static char buffer_error[512];
static int contador_errores = 0;
static int linea_actual = 0;

/* --------------------------------------------------------------------------
 *  Funciones de la Tabla de Simbolos
 * -------------------------------------------------------------------------- */

static EntradaSimbolo* crear_entrada(const char* nombre, const char* tipo,
                                     bool inicializada, int linea) {
    EntradaSimbolo* entrada = (EntradaSimbolo*)malloc(sizeof(EntradaSimbolo));
    if (!entrada) return NULL;
    entrada->nombre = strdup(nombre);
    entrada->tipo = strdup(tipo);
    entrada->inicializada = inicializada;
    entrada->linea = linea;
    entrada->siguiente = NULL;
    return entrada;
}

/**
 * Busca una variable en la tabla de simbolos.
 * Devuelve la entrada si existe, NULL si no.
 */
static EntradaSimbolo* buscar_simbolo(const char* nombre) {
    EntradaSimbolo* actual = tabla_global.cabeza;
    while (actual) {
        if (strcmp(actual->nombre, nombre) == 0)
            return actual;
        actual = actual->siguiente;
    }
    return NULL;
}

/**
 * Agrega una variable a la tabla de simbolos.
 * Devuelve false si ya existe (declaracion duplicada).
 */
static bool agregar_simbolo(const char* nombre, const char* tipo,
                            bool inicializada, int linea) {
    if (buscar_simbolo(nombre)) {
        snprintf(buffer_error, sizeof(buffer_error),
                 "Error Semantico (linea %d): La variable '%s' ya fue declarada",
                 linea, nombre);
        contador_errores++;
        return false;
    }
    
    EntradaSimbolo* entrada = crear_entrada(nombre, tipo, inicializada, linea);
    if (!entrada) return false;
    
    /* Insertar al inicio de la lista */
    entrada->siguiente = tabla_global.cabeza;
    tabla_global.cabeza = entrada;
    tabla_global.num_simbolos++;
    
    return true;
}

/* --------------------------------------------------------------------------
 *  Funciones de tipos
 * -------------------------------------------------------------------------- */

/**
 * Determina si un tipo es numerico (int, float, double, char).
 */
static bool es_tipo_numerico(const char* tipo) {
    return tipo && (strcmp(tipo, "int") == 0 ||
                    strcmp(tipo, "float") == 0 ||
                    strcmp(tipo, "double") == 0 ||
                    strcmp(tipo, "char") == 0);
}

/**
 * Determina si un tipo es booleano.
 */
static bool es_tipo_bool(const char* tipo) {
    return tipo && strcmp(tipo, "bool") == 0;
}

/**
 * Infiere el tipo de un nodo del AST. Devuelve una cadena estatica
 * ("int", "float", "double", "bool", "char", "desconocido" o NULL si error).
 * Si hay un error semantico, lo registra.
 */
static const char* inferir_tipo(NodoAST* nodo);

/**
 * Verifica que dos tipos sean compatibles para una asignacion.
 * Regla: numerico con numerico (int < float < double), bool con bool.
 */
static bool tipos_compatibles(const char* t1, const char* t2) {
    if (!t1 || !t2) return false;
    if (strcmp(t1, t2) == 0) return true;
    
    /* Promocion numerica: int -> float -> double */
    if (es_tipo_numerico(t1) && es_tipo_numerico(t2))
        return true;
    
    return false;
}

/**
 * Combina dos tipos numericos usando promocion (el mas "grande" gana).
 */
static const char* combinar_tipos(const char* t1, const char* t2) {
    if (!t1 || !t2) return "desconocido";
    if (strcmp(t1, "double") == 0 || strcmp(t2, "double") == 0) return "double";
    if (strcmp(t1, "float") == 0 || strcmp(t2, "float") == 0) return "float";
    if (strcmp(t1, "int") == 0 || strcmp(t2, "int") == 0) return "int";
    return t1;
}

/* --------------------------------------------------------------------------
 *  Verificacion de expresiones
 * -------------------------------------------------------------------------- */

/**
 * Infiere el tipo de un nodo de expresion y verifica los operadores.
 */
static const char* inferir_tipo(NodoAST* nodo) {
    if (!nodo) return NULL;
    linea_actual = nodo->linea;
    
    switch (nodo->tipo) {
        case AST_NUMERO:
            return "int";
            
        case AST_NUMERO_DECIMAL:
            return "float";
            
        case AST_CONSTANTE_BOOL:
            return "bool";
            
        case AST_IDENTIFICADOR: {
            EntradaSimbolo* sim = buscar_simbolo(nodo->valor);
            if (!sim) {
                snprintf(buffer_error, sizeof(buffer_error),
                         "Error Semantico (linea %d): La variable '%s' no ha sido declarada",
                         linea_actual, nodo->valor);
                contador_errores++;
                return NULL;
            }
            return sim->tipo;
        }
        
        case AST_EXPR_UNARIA: {
            const char* op = nodo->valor;
            const char* tipo_operando = inferir_tipo(nodo->hijo1);
            
            /* Prefijo/postfijo ++ o -- */
            if (op && (strcmp(op, "++") == 0 || strcmp(op, "--") == 0)) {
                if (!tipo_operando) return NULL;
                if (!es_tipo_numerico(tipo_operando)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): El operador '%s' solo se aplica a variables numericas (se encontro %s)",
                             linea_actual, op, tipo_operando);
                    contador_errores++;
                    return NULL;
                }
                /* La variable debe estar declarada e inicializada */
                if (nodo->hijo1->tipo == AST_IDENTIFICADOR) {
                    EntradaSimbolo* sim = buscar_simbolo(nodo->hijo1->valor);
                    if (sim) sim->inicializada = true;
                }
                return tipo_operando;
            }
            
            /* Negacion logica ! */
            if (op && strcmp(op, "!") == 0) {
                if (!tipo_operando) return NULL;
                if (!es_tipo_bool(tipo_operando) && !es_tipo_numerico(tipo_operando)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): El operador '!' requiere operando booleano o numerico",
                             linea_actual);
                    contador_errores++;
                    return NULL;
                }
                return "bool";
            }
            
            return tipo_operando;
        }
        
        case AST_EXPR_BINARIA: {
            const char* op = nodo->valor;
            const char* t1 = inferir_tipo(nodo->hijo1);
            const char* t2 = inferir_tipo(nodo->hijo2);
            
            if (!t1 || !t2) return NULL; /* Errores ya registrados al inferir hijos */
            
            /* Operadores logicos: && ||  -> operands bool o numericos, resultado bool */
            if (op && (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0)) {
                if (!es_tipo_bool(t1) && !es_tipo_numerico(t1)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): Operador '%s' requiere operandos compatibles con bool (se encontro %s)",
                             linea_actual, op, t1);
                    contador_errores++;
                    return NULL;
                }
                if (!es_tipo_bool(t2) && !es_tipo_numerico(t2)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): Operador '%s' requiere operandos compatibles con bool (se encontro %s)",
                             linea_actual, op, t2);
                    contador_errores++;
                    return NULL;
                }
                return "bool";
            }
            
            /* Operadores relacionales: == != < > <= >= -> resultado bool */
            if (op && (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
                       strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                       strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0)) {
                if (!tipos_compatibles(t1, t2)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): Comparacion entre tipos incompatibles (%s y %s)",
                             linea_actual, t1, t2);
                    contador_errores++;
                    return NULL;
                }
                return "bool";
            }
            
            /* Operadores aritmeticos: + - * / % -> resultado numerico */
            if (op && (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                       strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
                       strcmp(op, "%") == 0)) {
                if (!es_tipo_numerico(t1) || !es_tipo_numerico(t2)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): El operador '%s' requiere operandos numericos (se encontro %s y %s)",
                             linea_actual, op, t1, t2);
                    contador_errores++;
                    return NULL;
                }
                /* Modulo solo en enteros */
                if (strcmp(op, "%") == 0 &&
                    (strcmp(t1, "float") == 0 || strcmp(t1, "double") == 0 ||
                     strcmp(t2, "float") == 0 || strcmp(t2, "double") == 0)) {
                    snprintf(buffer_error, sizeof(buffer_error),
                             "Error Semantico (linea %d): El operador '%%' requiere operandos enteros",
                             linea_actual);
                    contador_errores++;
                    return NULL;
                }
                return combinar_tipos(t1, t2);
            }
            
            return "desconocido";
        }
        
        default:
            return "desconocido";
    }
}

/**
 * Verifica la condicion de if/while/for/do-while.
 * La condicion debe poder evaluarse como bool/numerico (inferir_tipo
 * ya reporta los errores de incompatibilidad).
 */
static void verificar_condicion(NodoAST* condicion) {
    if (!condicion) return;
    inferir_tipo(condicion);
}

/* --------------------------------------------------------------------------
 *  Verificacion de sentencias
 * -------------------------------------------------------------------------- */

static void analizar_sentencia(NodoAST* nodo);

static void analizar_lista(NodoAST* nodo) {
    while (nodo) {
        analizar_sentencia(nodo);
        nodo = nodo->siguiente;
    }
}

static void analizar_sentencia(NodoAST* nodo) {
    if (!nodo) return;
    linea_actual = nodo->linea;
    
    switch (nodo->tipo) {
        case AST_PROGRAMA:
        case AST_BLOQUE:
            analizar_lista(nodo->hijo1);
            break;
            
        case AST_DECLARACION: {
            const char* tipo = nodo->valor;  /* int, float, ... */
            if (nodo->hijo1 && nodo->hijo1->tipo == AST_IDENTIFICADOR) {
                const char* nombre = nodo->hijo1->valor;
                bool inicializada = false;
                
                if (nodo->hijo2) {
                    const char* tipo_valor = inferir_tipo(nodo->hijo2);
                    if (!tipo_valor) return;
                    if (!tipos_compatibles(tipo, tipo_valor)) {
                        snprintf(buffer_error, sizeof(buffer_error),
                                 "Error Semantico (linea %d): No se puede inicializar '%s' (%s) con valor de tipo %s",
                                 linea_actual, nombre, tipo, tipo_valor);
                        contador_errores++;
                        return;
                    }
                    inicializada = true;
                }
                
                agregar_simbolo(nombre, tipo, inicializada, linea_actual);
            }
            break;
        }
        
        case AST_ASIGNACION: {
            if (!nodo->hijo1 || !nodo->hijo2) break;
            
            /* Lado izquierdo: debe ser un identificador declarado */
            NodoAST* destino = nodo->hijo1;
            if (destino->tipo != AST_IDENTIFICADOR) break;
            
            EntradaSimbolo* sim = buscar_simbolo(destino->valor);
            if (!sim) {
                snprintf(buffer_error, sizeof(buffer_error),
                         "Error Semantico (linea %d): La variable '%s' no ha sido declarada",
                         linea_actual, destino->valor);
                contador_errores++;
                break;
            }
            
            /* Lado derecho: inferir tipo y verificar compatibilidad */
            const char* tipo_valor = inferir_tipo(nodo->hijo2);
            if (tipo_valor && !tipos_compatibles(sim->tipo, tipo_valor)) {
                snprintf(buffer_error, sizeof(buffer_error),
                         "Error Semantico (linea %d): No se puede asignar valor de tipo %s a la variable '%s' (%s)",
                         linea_actual, tipo_valor, sim->nombre, sim->tipo);
                contador_errores++;
                break;
            }
            
            sim->inicializada = true;
            break;
        }
        
        case AST_SENTENCIA_IF:
            verificar_condicion(nodo->hijo1);
            analizar_sentencia(nodo->hijo2);
            if (nodo->hijo3) analizar_sentencia(nodo->hijo3);
            break;
            
        case AST_SENTENCIA_WHILE:
            if (nodo->hijo1) verificar_condicion(nodo->hijo1);
            if (nodo->hijo2) analizar_sentencia(nodo->hijo2);
            break;
            
        case AST_SENTENCIA_DO_WHILE:
            if (nodo->hijo1) analizar_sentencia(nodo->hijo1);
            if (nodo->hijo2) verificar_condicion(nodo->hijo2);
            break;
            
        case AST_SENTENCIA_FOR:
            if (nodo->cuerpo) analizar_sentencia(nodo->cuerpo);
            if (nodo->hijo1) inferir_tipo(nodo->hijo1);   /* Inicializacion */
            if (nodo->hijo2) verificar_condicion(nodo->hijo2);  /* Condicion */
            if (nodo->hijo3) inferir_tipo(nodo->hijo3);   /* Incremento */
            break;
            
        case AST_SENTENCIA_EXPR:
            if (nodo->hijo1) inferir_tipo(nodo->hijo1);
            break;
            
        case AST_RETURN:
            if (nodo->hijo1) inferir_tipo(nodo->hijo1);
            break;
            
        default:
            break;
    }
}

/* --------------------------------------------------------------------------
 *  Funcion publica: analizar_semantica()
 * -------------------------------------------------------------------------- */
ResultadoSemantico analizar_semantica(NodoAST* raiz) {
    /* Inicializar estado */
    tabla_global.cabeza = NULL;
    tabla_global.num_simbolos = 0;
    contador_errores = 0;
    buffer_error[0] = '\0';
    linea_actual = 0;
    
    /* Recorrer el AST verificando la semantica */
    analizar_sentencia(raiz);
    
    /* Construir resultado */
    ResultadoSemantico resultado;
    resultado.exito = (contador_errores == 0);
    resultado.num_errores = contador_errores;
    resultado.mensaje_error = (contador_errores > 0) ? strdup(buffer_error) : NULL;
    resultado.tabla = tabla_global;
    
    return resultado;
}

void liberar_tabla_simbolos(TablaSimbolos* tabla) {
    if (!tabla) return;
    EntradaSimbolo* actual = tabla->cabeza;
    while (actual) {
        EntradaSimbolo* tmp = actual;
        actual = actual->siguiente;
        if (tmp->nombre) free(tmp->nombre);
        if (tmp->tipo) free(tmp->tipo);
        free(tmp);
    }
    tabla->cabeza = NULL;
    tabla->num_simbolos = 0;
}
