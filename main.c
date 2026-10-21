/* ==========================================================================
 * main.c  —  Interfaz Grafica GTK3 para el Analizador Lexico, Sintactico
 *           y Semantico
 *
 * Compilar: make
 * Requisitos: GTK3, FLEX, GCC (via MSYS2 MinGW64)
 *
 * La GUI tiene:
 *   - Panel izquierdo  : editor de codigo fuente
 *   - Panel central    : tabla con los tokens reconocidos (analisis lexico)
 *   - Panel derecho    : arbol de sintaxis abstracto (AST - analisis sintactico)
 *   - Sub-panel derecho: tabla de simbolos y resultados (analisis semantico)
 *   - Boton "Analizar" : invoca lexer + parser + semantico
 *   - Boton "Limpiar"  : borra todo
 *   - Barra de estado  : muestra el resultado del analisis
 * ========================================================================== */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "semantico.h"
#include "generador_python.h"

/* --------------------------------------------------------------------------
 *  Declaracion de la funcion del lexer (definida en lex.yy.c, generado por
 *  FLEX a partir de lexer.l).
 * -------------------------------------------------------------------------- */
void analizar_cadena(const char* entrada,
                     void (*callback)(const char* tipo,
                                      const char* lexema,
                                      void* user_data),
                     void* user_data);

/* --------------------------------------------------------------------------
 *  Columnas de la tabla de tokens
 * -------------------------------------------------------------------------- */
enum {
    COL_NUM    = 0,  /* Numero de token (#)  */
    COL_TIPO   = 1,  /* Tipo / categoria     */
    COL_LEXEMA = 2,  /* Valor del lexema     */
    N_COLS     = 3
};

/* --------------------------------------------------------------------------
 *  Estado global de la interfaz
 * -------------------------------------------------------------------------- */
static GtkListStore* g_store       = NULL;  /* Modelo de la tabla de tokens */
static GtkTextBuffer* g_ast_buffer = NULL;  /* Buffer de texto para el AST */
static GtkTextBuffer* g_sem_buffer = NULL;  /* Buffer de texto para la semantica */
static GtkTextBuffer* g_py_buffer  = NULL;  /* Buffer de texto para el codigo Python */
static GtkLabel*     g_lbl_status  = NULL;  /* Etiqueta de estado */
static int           g_n_tokens    = 0;     /* Contador de tokens */

/* Array dinamico para almacenar tokens (para pasarlos al parser) */
static Token*        g_tokens_array = NULL;
static int           g_tokens_capacity = 0;

/* --------------------------------------------------------------------------
 *  on_token_found()
 *  Callback invocado por el lexer cada vez que reconoce un token.
 *  Almacena el token en la tabla de la GUI y en el array para el parser.
 * -------------------------------------------------------------------------- */
static void on_token_found(const char* tipo, const char* lexema, void* _)
{
    /* Mostrar en la tabla de tokens */
    GtkTreeIter iter;
    char num[16];
    snprintf(num, sizeof(num), "%d", ++g_n_tokens);

    gtk_list_store_append(g_store, &iter);
    gtk_list_store_set(g_store, &iter,
        COL_NUM,    num,
        COL_TIPO,   tipo,
        COL_LEXEMA, lexema,
        -1);
    
    /* Almacenar el token en el array para pasarlo al parser */
    /* Ignorar espacios en blanco y errores lexicos para el parser */
    if (strcmp(tipo, "ERROR_LEXICO") == 0) {
        return; /* No agregar errores lexicos al parser */
    }
    
    /* Expandir el array si es necesario */
    if (g_n_tokens > g_tokens_capacity) {
        g_tokens_capacity = (g_tokens_capacity == 0) ? 32 : (g_tokens_capacity * 2);
        g_tokens_array = (Token*)realloc(g_tokens_array, 
                                         g_tokens_capacity * sizeof(Token));
    }
    
    /* Almacenar el token */
    int idx = g_n_tokens - 1;
    g_tokens_array[idx].tipo = strdup(tipo);
    g_tokens_array[idx].lexema = strdup(lexema);
}

/* --------------------------------------------------------------------------
 *  Callback para generar la representacion del arbol en el buffer de texto
 * -------------------------------------------------------------------------- */
static void agregar_linea_ast(const char* linea, void* user_data)
{
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(g_ast_buffer, &iter);
    gtk_text_buffer_insert(g_ast_buffer, &iter, linea, -1);
    gtk_text_buffer_insert(g_ast_buffer, &iter, "\n", -1);
}

/* --------------------------------------------------------------------------
 *  Funcion auxiliar: insertar texto en el buffer semantico
 * -------------------------------------------------------------------------- */
static void insertar_semantico(const char* texto)
{
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(g_sem_buffer, &iter);
    gtk_text_buffer_insert(g_sem_buffer, &iter, texto, -1);
    gtk_text_buffer_insert(g_sem_buffer, &iter, "\n", -1);
}

/* --------------------------------------------------------------------------
 *  on_btn_analizar()
 *  Sennal del boton "Analizar": invoca el lexer y luego el parser.
 * -------------------------------------------------------------------------- */
static void on_btn_analizar(GtkWidget* btn, gpointer user_data)
{
    GtkTextView*   tv  = GTK_TEXT_VIEW(user_data);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(tv);

    GtkTextIter inicio, fin;
    gtk_text_buffer_get_bounds(buf, &inicio, &fin);
    gchar* texto = gtk_text_buffer_get_text(buf, &inicio, &fin, FALSE);

    /* Limpiar resultados anteriores */
    gtk_list_store_clear(g_store);
    gtk_text_buffer_set_text(g_ast_buffer, "", -1);
    gtk_text_buffer_set_text(g_sem_buffer, "", -1);
    gtk_text_buffer_set_text(g_py_buffer, "", -1);
    
    /* Liberar tokens anteriores */
    for (int i = 0; i < g_n_tokens; i++) {
        if (g_tokens_array && i < g_tokens_capacity) {
            free(g_tokens_array[i].tipo);
            free(g_tokens_array[i].lexema);
        }
    }
    g_n_tokens = 0;

    if (strlen(texto) == 0) {
        gtk_label_set_text(g_lbl_status,
            "Advertencia: el area de codigo esta vacia.");
        g_free(texto);
        return;
    }

    /* ═══════════════════════════════════════════════════════════════════
     *  FASE 1: ANALISIS LEXICO
     * ═══════════════════════════════════════════════════════════════════ */
    analizar_cadena(texto, on_token_found, NULL);

    gchar msg[512];
    
    /* Verificar si hay errores lexicos */
    int errores_lexicos = 0;
    for (int i = 0; i < g_n_tokens; i++) {
        if (g_tokens_array && i < g_tokens_capacity) {
            if (strcmp(g_tokens_array[i].tipo, "ERROR_LEXICO") == 0) {
                errores_lexicos++;
            }
        }
    }
    
    /* Si hay errores lexicos, detener el analisis */
    if (errores_lexicos > 0) {
        g_snprintf(msg, sizeof(msg),
                   "✗ Error Lexico: %d error(es) encontrado(s). No se puede analizar sintaxis.",
                   errores_lexicos);
        
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "═══════════════════════════════════════════════════\n", -1);
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "  ✗ ERROR LEXICO\n", -1);
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "═══════════════════════════════════════════════════\n\n", -1);
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "El codigo contiene caracteres invalidos.\n", -1);
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "Por favor, revisa la tabla de tokens y corrige\n", -1);
        gtk_text_buffer_insert_at_cursor(g_ast_buffer,
            "los tokens marcados como ERROR_LEXICO.\n", -1);
        
        gtk_label_set_text(g_lbl_status, msg);
        g_free(texto);
        return;
    }
    
    g_snprintf(msg, sizeof(msg),
               "Analisis Lexico: %d token(s) encontrado(s).",
               g_n_tokens);

    /* ═══════════════════════════════════════════════════════════════════
     *  FASE 2: ANALISIS SINTACTICO
     * ═══════════════════════════════════════════════════════════════════ */
    if (g_n_tokens > 0) {
        ResultadoParser resultado = analizar_sintaxis(g_tokens_array, g_n_tokens);
        
        if (resultado.exito) {
            /* Exito: mostrar el AST */
            g_snprintf(msg, sizeof(msg),
                       "✓ Analisis Completado: %d token(s), sintaxis correcta.",
                       g_n_tokens);
            
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "═══════════════════════════════════════════════════\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  ARBOL DE SINTAXIS ABSTRACTA (AST)\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "═══════════════════════════════════════════════════\n\n", -1);
            
            generar_representacion_arbol(resultado.raiz, 0, agregar_linea_ast, NULL);
            
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "\n═══════════════════════════════════════════════════\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  ✓ El codigo es sintacticamente correcto\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "═══════════════════════════════════════════════════\n", -1);
            
            /* ═══════════════════════════════════════════════════════════
             *  FASE 3: ANALISIS SEMANTICO
             *  (solo si la sintaxis es correcta, pues requiere el AST)
             * ═══════════════════════════════════════════════════════════ */
            ResultadoSemantico sem = analizar_semantica(resultado.raiz);
            
            insertar_semantico("═══════════════════════════════════════════════════");
            insertar_semantico("  ANALISIS SEMANTICO");
            insertar_semantico("═══════════════════════════════════════════════════");
            insertar_semantico("");
            
            if (sem.exito) {
                insertar_semantico("  ✓ Sin errores semanticos");
                g_snprintf(msg, sizeof(msg),
                           "✓ Analisis Completado: %d token(s), sintaxis y semantica correctas.",
                           g_n_tokens);
            } else {
                insertar_semantico("  ✗ ERRORES SEMANTICOS");
                insertar_semantico("");
                insertar_semantico(sem.mensaje_error ? sem.mensaje_error
                                                     : "Error semantico desconocido");
                g_snprintf(msg, sizeof(msg),
                           "✗ Error Semantico: %d error(es). Revisa el panel derecho.",
                           sem.num_errores);
            }
            
            /* Tabla de simbolos */
            insertar_semantico("");
            insertar_semantico("  ── TABLA DE SIMBOLOS ──");
            insertar_semantico("");
            if (sem.tabla.num_simbolos == 0) {
                insertar_semantico("  (No se detectaron variables)");
            } else {
                char linea[256];
                g_snprintf(linea, sizeof(linea), "  %-16s %-10s %-10s %s",
                           "Variable", "Tipo", "Inicializada", "Linea");
                insertar_semantico(linea);
                insertar_semantico("  ---------------------------------------");
                
                EntradaSimbolo* e = sem.tabla.cabeza;
                while (e) {
                    char tipo[64];
                    char ini[16];
                    char num_linea[16];
                    g_snprintf(tipo, sizeof(tipo), "%s", e->tipo);
                    g_snprintf(ini, sizeof(ini), "%s", e->inicializada ? "SI" : "NO");
                    g_snprintf(num_linea, sizeof(num_linea), "%d", e->linea);
                    g_snprintf(linea, sizeof(linea), "  %-16s %-10s %-10s %s",
                               e->nombre, tipo, ini, num_linea);
                    insertar_semantico(linea);
                    e = e->siguiente;
                }
            }
            
            /* Liberar resultados semanticos */
            liberar_tabla_simbolos(&sem.tabla);
            if (sem.mensaje_error) free(sem.mensaje_error);
            
            /* ═══════════════════════════════════════════════════════════
             *  FASE 4: TRADUCCION A PYTHON
             *  (recorre el AST y genera el codigo Python equivalente)
             * ═══════════════════════════════════════════════════════════ */
            char* codigo_python = generar_codigo_python(resultado.raiz);
            if (codigo_python) {
                gtk_text_buffer_set_text(g_py_buffer, codigo_python, -1);
                free(codigo_python);
            }
        } else {
            /* Error sintactico */
            g_snprintf(msg, sizeof(msg),
                       "✗ Error Sintactico: %d error(es) encontrado(s).",
                       resultado.num_errores);
            
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "═══════════════════════════════════════════════════\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  ✗ ERROR SINTACTICO\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "═══════════════════════════════════════════════════\n\n", -1);
            
            if (resultado.mensaje_error) {
                gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                    resultado.mensaje_error, -1);
                gtk_text_buffer_insert_at_cursor(g_ast_buffer, "\n", -1);
            }
            
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "\nSugerencia: Verifica la sintaxis de tu codigo.\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "Reglas del lenguaje:\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  - Asignacion: identificador = expresion;\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  - If: if (condicion) sentencia [else sentencia]\n", -1);
            gtk_text_buffer_insert_at_cursor(g_ast_buffer,
                "  - Bloque: { sentencias }\n", -1);
        }
        
        /* Liberar memoria del AST */
        liberar_ast(resultado.raiz);
        if (resultado.mensaje_error) {
            free(resultado.mensaje_error);
        }
    }

    /* Actualizar barra de estado */
    gtk_label_set_text(g_lbl_status, msg);

    g_free(texto);
}

/* --------------------------------------------------------------------------
 *  on_btn_limpiar()
 *  Sennal del boton "Limpiar": borra el editor, la tabla y el AST.
 * -------------------------------------------------------------------------- */
static void on_btn_limpiar(GtkWidget* btn, gpointer user_data)
{
    GtkTextView* tv = GTK_TEXT_VIEW(user_data);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(tv), "", -1);
    gtk_list_store_clear(g_store);
    gtk_text_buffer_set_text(g_ast_buffer, "", -1);
    gtk_text_buffer_set_text(g_sem_buffer, "", -1);
    gtk_text_buffer_set_text(g_py_buffer, "", -1);
    
    /* Liberar tokens */
    for (int i = 0; i < g_n_tokens; i++) {
        if (g_tokens_array && i < g_tokens_capacity) {
            free(g_tokens_array[i].tipo);
            free(g_tokens_array[i].lexema);
        }
    }
    g_n_tokens = 0;
    
    gtk_label_set_text(g_lbl_status, "Listo.");
}

/* --------------------------------------------------------------------------
 *  crear_columna()
 *  Crea y devuelve una columna para el GtkTreeView.
 * -------------------------------------------------------------------------- */
static GtkTreeViewColumn* crear_columna(const char* titulo,
                                        int         col_id,
                                        gboolean    expandir,
                                        int         min_ancho)
{
    GtkCellRenderer*   r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* c = gtk_tree_view_column_new_with_attributes(
                               titulo, r, "text", col_id, NULL);
    gtk_tree_view_column_set_expand(c, expandir);
    gtk_tree_view_column_set_min_width(c, min_ancho);
    gtk_tree_view_column_set_resizable(c, TRUE);
    return c;
}

/* --------------------------------------------------------------------------
 *  aplicar_css()
 *  Aplica estilos CSS a la ventana GTK3.
 * -------------------------------------------------------------------------- */
static void aplicar_css(void)
{
    GtkCssProvider* p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p,
        /* Cabecera azul oscuro */
        ".cabecera { background-color: #2c3e50; padding: 10px; }"
        /* Boton Analizar: verde */
        "button.suggested-action {"
        "  background: #27ae60; color: white; font-weight: bold;"
        "  border-radius: 4px; }"
        "button.suggested-action:hover { background: #1e8449; }"
        /* Boton Limpiar: rojo */
        "button.destructive-action {"
        "  background: #c0392b; color: white; font-weight: bold;"
        "  border-radius: 4px; }"
        "button.destructive-action:hover { background: #a93226; }"
        /* Barra de estado */
        ".statusbar { background-color: #ecf0f1; padding: 4px 10px;"
        "  font-size: 11px; }"
        /* Tabla monoespaciada */
        "treeview { font-family: Consolas, Monospace; font-size: 10px; }"
        , -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

/* ========================================================================== */
/*  main                                                                        */
/* ========================================================================== */
int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);
    aplicar_css();

    /* ── Ventana principal ─────────────────────────────────────────────── */
    GtkWidget* ventana = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ventana), "Analizador Lexico, Sintactico y Semantico  —  FLEX + C");
    gtk_window_set_default_size(GTK_WINDOW(ventana), 1280, 720);
    gtk_window_set_position(GTK_WINDOW(ventana), GTK_WIN_POS_CENTER);
    g_signal_connect(ventana, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Layout vertical raiz */
    GtkWidget* vbox_raiz = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ventana), vbox_raiz);

    /* ── Cabecera ──────────────────────────────────────────────────────── */
    GtkWidget* caja_header = gtk_event_box_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(caja_header),
                                "cabecera");

    GtkWidget* lbl_titulo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_titulo),
        "<span foreground='white' size='16000' weight='bold'>"
        "  Analizador Lexico y Sintactico  |  FLEX + C + GTK3"
        "</span>");
    gtk_widget_set_margin_top(lbl_titulo, 4);
    gtk_widget_set_margin_bottom(lbl_titulo, 4);
    gtk_container_add(GTK_CONTAINER(caja_header), lbl_titulo);
    gtk_box_pack_start(GTK_BOX(vbox_raiz), caja_header, FALSE, FALSE, 0);

    /* ── Paneles divididos (izquierdo / centro / derecho) ──────────────── */
    /* Paned principal (izquierdo vs resto) */
    GtkWidget* paned_main = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(paned_main, 8);
    gtk_widget_set_margin_bottom(paned_main, 4);
    gtk_widget_set_margin_start(paned_main, 10);
    gtk_widget_set_margin_end(paned_main, 10);
    gtk_box_pack_start(GTK_BOX(vbox_raiz), paned_main, TRUE, TRUE, 0);
    
    /* Paned secundario (centro vs derecho) */
    GtkWidget* paned_right = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    /* ─── Panel IZQUIERDO: editor de codigo ─── */
    GtkWidget* lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_end(lbox, 8);

    GtkWidget* lbl_codigo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_codigo), "<b>Codigo fuente:</b>");
    gtk_widget_set_halign(lbl_codigo, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(lbox), lbl_codigo, FALSE, FALSE, 0);

    /* Editor de texto con scroll */
    GtkWidget* sw_editor = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw_editor),
                                        GTK_SHADOW_IN);
    gtk_widget_set_vexpand(sw_editor, TRUE);

    GtkWidget* editor = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(editor), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editor), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(editor), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(editor), 8);

    /* Codigo de ejemplo que aparece al iniciar la app */
    GtkTextBuffer* buf_editor = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    gtk_text_buffer_set_text(buf_editor,
        "if (x == 10) {\n"
        "    y = x + 5;\n"
        "} else {\n"
        "    y = 0;\n"
        "}", -1);

    gtk_container_add(GTK_CONTAINER(sw_editor), editor);
    gtk_box_pack_start(GTK_BOX(lbox), sw_editor, TRUE, TRUE, 0);

    /* Botones */
    GtkWidget* hbox_btn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(hbox_btn, 6);
    gtk_widget_set_margin_bottom(hbox_btn, 4);

    GtkWidget* btn_analizar = gtk_button_new_with_label(">> Analizar");
    GtkWidget* btn_limpiar  = gtk_button_new_with_label("Limpiar");

    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn_analizar), "suggested-action");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn_limpiar),  "destructive-action");

    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_analizar, TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(hbox_btn), btn_limpiar,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lbox), hbox_btn, FALSE, FALSE, 0);

    gtk_paned_pack1(GTK_PANED(paned_main), lbox, TRUE, FALSE);

    /* ─── Panel CENTRO: tabla de tokens ─── */
    GtkWidget* cbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(cbox, 8);
    gtk_widget_set_margin_end(cbox, 4);

    GtkWidget* lbl_tokens = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_tokens), "<b>Tokens reconocidos:</b>");
    gtk_widget_set_halign(lbl_tokens, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(cbox), lbl_tokens, FALSE, FALSE, 0);

    /* Modelo de datos para la tabla */
    g_store = gtk_list_store_new(N_COLS,
                                  G_TYPE_STRING,   /* # */
                                  G_TYPE_STRING,   /* Tipo */
                                  G_TYPE_STRING);  /* Lexema */

    /* TreeView (tabla) */
    GtkWidget* tabla = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_store));
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tabla),
                                  GTK_TREE_VIEW_GRID_LINES_BOTH);

    gtk_tree_view_append_column(GTK_TREE_VIEW(tabla),
        crear_columna("#",             COL_NUM,    FALSE, 40));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabla),
        crear_columna("Tipo de Token", COL_TIPO,   TRUE,  160));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabla),
        crear_columna("Lexema",        COL_LEXEMA, FALSE, 90));

    GtkWidget* sw_tabla = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw_tabla),
                                        GTK_SHADOW_IN);
    gtk_widget_set_vexpand(sw_tabla, TRUE);
    gtk_container_add(GTK_CONTAINER(sw_tabla), tabla);
    gtk_box_pack_start(GTK_BOX(cbox), sw_tabla, TRUE, TRUE, 0);

    /* Agregar tabla de tokens al panel central */
    gtk_paned_pack1(GTK_PANED(paned_right), cbox, TRUE, FALSE);

    /* ─── Panel DERECHO: AST (arriba) + Semantico (abajo) ─── */
    GtkWidget* paned_der = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(paned_der, 4);

    /* --- Sub-panel superior: Arbol de Sintaxis Abstracta --- */
    GtkWidget* rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* lbl_ast = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_ast), 
        "<b>Arbol de Sintaxis Abstracta (AST):</b>");
    gtk_widget_set_halign(lbl_ast, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(rbox), lbl_ast, FALSE, FALSE, 0);

    /* TextView para el AST con scroll */
    GtkWidget* sw_ast = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw_ast),
                                        GTK_SHADOW_IN);
    gtk_widget_set_vexpand(sw_ast, TRUE);

    GtkWidget* tv_ast = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv_ast), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv_ast), GTK_WRAP_NONE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv_ast), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv_ast), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv_ast), 8);
    
    /* Guardar referencia al buffer del AST */
    g_ast_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_ast));

    gtk_container_add(GTK_CONTAINER(sw_ast), tv_ast);
    gtk_box_pack_start(GTK_BOX(rbox), sw_ast, TRUE, TRUE, 0);

    /* --- Sub-panel inferior: Analisis Semantico y Tabla de Simbolos --- */
    GtkWidget* sbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* lbl_sem = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_sem),
        "<b>Analisis Semantico y Tabla de Simbolos:</b>");
    gtk_widget_set_halign(lbl_sem, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sbox), lbl_sem, FALSE, FALSE, 0);

    GtkWidget* sw_sem = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw_sem),
                                        GTK_SHADOW_IN);
    gtk_widget_set_vexpand(sw_sem, TRUE);

    GtkWidget* tv_sem = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv_sem), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv_sem), GTK_WRAP_NONE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv_sem), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv_sem), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv_sem), 8);

    /* Guardar referencia al buffer semantico */
    g_sem_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_sem));

    gtk_container_add(GTK_CONTAINER(sw_sem), tv_sem);
    gtk_box_pack_start(GTK_BOX(sbox), sw_sem, TRUE, TRUE, 0);

    /* --- Sub-panel NUEVO: Codigo Python generado (Traductor) --- */
    GtkWidget* pbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* lbl_py = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_py),
        "<b>Codigo Python generado (Traductor):</b>");
    gtk_widget_set_halign(lbl_py, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(pbox), lbl_py, FALSE, FALSE, 0);

    GtkWidget* sw_py = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw_py),
                                        GTK_SHADOW_IN);
    gtk_widget_set_vexpand(sw_py, TRUE);

    GtkWidget* tv_py = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv_py), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv_py), GTK_WRAP_NONE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv_py), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv_py), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv_py), 8);

    /* Guardar referencia al buffer Python */
    g_py_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv_py));

    gtk_container_add(GTK_CONTAINER(sw_py), tv_py);
    gtk_box_pack_start(GTK_BOX(pbox), sw_py, TRUE, TRUE, 0);

    /* Ensamblar panel derecho:
     *   paned_der   : AST (arriba)  +  paned_sem_py (abajo)
     *   paned_sem_py: Semantico (arriba)  +  Python (abajo) */
    GtkWidget* paned_sem_py = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack1(GTK_PANED(paned_sem_py), sbox, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned_sem_py), pbox, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned_sem_py), 330);

    gtk_paned_pack1(GTK_PANED(paned_der), rbox, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned_der), paned_sem_py, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned_der), 280);

    /* Agregar panel derecho (AST + Semantico) al paned de la derecha */
    gtk_paned_pack2(GTK_PANED(paned_right), paned_der, TRUE, FALSE);
    /* Agregar paned_right al paned principal */
    gtk_paned_pack2(GTK_PANED(paned_main), paned_right, TRUE, FALSE);
    
    /* Posiciones de los divisores */
    gtk_paned_set_position(GTK_PANED(paned_main), 420);
    gtk_paned_set_position(GTK_PANED(paned_right), 380);

    /* ── Barra de estado ───────────────────────────────────────────────── */
    gtk_box_pack_end(GTK_BOX(vbox_raiz),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_box),
                                "statusbar");

    g_lbl_status = GTK_LABEL(gtk_label_new(
        "Listo. Escribe o pega codigo en el editor y presiona >> Analizar para ver el analisis lexico, sintactico y semantico."));
    gtk_widget_set_margin_top(GTK_WIDGET(g_lbl_status), 4);
    gtk_widget_set_margin_bottom(GTK_WIDGET(g_lbl_status), 4);
    gtk_widget_set_halign(GTK_WIDGET(g_lbl_status), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(status_box), GTK_WIDGET(g_lbl_status),
                       FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vbox_raiz), status_box, FALSE, FALSE, 0);

    /* ── Conectar senales ──────────────────────────────────────────────── */
    g_signal_connect(btn_analizar, "clicked",
                     G_CALLBACK(on_btn_analizar), editor);
    g_signal_connect(btn_limpiar,  "clicked",
                     G_CALLBACK(on_btn_limpiar),  editor);

    /* ── Mostrar todo y entrar al loop de eventos ──────────────────────── */
    gtk_widget_show_all(ventana);
    gtk_main();

    return 0;
}
