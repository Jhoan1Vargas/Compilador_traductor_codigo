# ===========================================================================
# Makefile  —  Analizador Lexico, Sintactico y Semantico (FLEX + GTK3)
# Sistema  : Windows 11 + MSYS2 MinGW64 / Linux
# Uso      : make          -> compila
#            make clean    -> elimina archivos generados
# ===========================================================================

CC      = gcc
FLEX    = flex
TARGET  = analizador.exe

# Flags de GTK3 (obtenidos automaticamente con pkg-config)
GTK_CF  = $(shell pkg-config --cflags gtk+-3.0)
GTK_LF  = $(shell pkg-config --libs   gtk+-3.0)

# -Wall    : mostrar advertencias del compilador
# -O2      : optimizacion nivel 2
# -mwindows: suprimir la ventana de consola en Windows (app solo grafica)
#            (comentar -mwindows en Linux si causa problemas)
CFLAGS  = -Wall -O2 $(GTK_CF)
LDFLAGS = $(GTK_LF)

# ---------------------------------------------------------------------------
# Regla principal
# ---------------------------------------------------------------------------
all: $(TARGET)

# Paso 1: FLEX genera lex.yy.c a partir de lexer.l
lex.yy.c: lexer.l
	$(FLEX) lexer.l

# Paso 2: compilar main.c + parser.c + semantico.c + generador_python.c
#          + lex.yy.c y enlazar con GTK3
$(TARGET): main.c parser.c parser.h semantico.c semantico.h generador_python.c generador_python.h lex.yy.c
	$(CC) $(CFLAGS) -o $@ main.c parser.c semantico.c generador_python.c lex.yy.c $(LDFLAGS)
	@echo ""
	@echo "  ==> Compilacion exitosa: $(TARGET)"
	@echo "  ==> Ejecutar con: ./$(TARGET)"
	@echo ""

# ---------------------------------------------------------------------------
# Limpiar archivos generados
# ---------------------------------------------------------------------------
clean:
	rm -f lex.yy.c $(TARGET)
	@echo "Limpieza completada."

.PHONY: all clean
