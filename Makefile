# Nome do arquivo de saída
TARGET = ping-pong-os.out

# Compilador
CC = gcc

# Flags de compilação
CFLAGS = -Wall

# Arquivos fonte
SRCS = ppos-core-aux.c ppos_disk.c disk.c pingpong-disco2.c
# SRCS = ppos-core-aux.c ppos_disk.c disk.c pingpong-disco1.c

# Bibliotecas
LIBS = libppos_static.a -lrt

# Diretório de saída
BUILD_DIR = build

# Comando para criar o diretório de saída, se não existir
MKDIR_P = mkdir -p

all: $(TARGET)

$(TARGET): $(SRCS)
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CFLAGS) -g -o $(BUILD_DIR)/$(TARGET) $(SRCS) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean