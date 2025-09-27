# File Manager Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g `pkg-config --cflags gtk4 libadwaita-1 gio-2.0`
LIBS = `pkg-config --libs gtk4 libadwaita-1 gio-2.0`

TARGET = casper
SRCDIR = .
SOURCES = main.c utils.c navigation.c directory.c views.c actions.c sidebar.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: $(SRCDIR)/%.c filemanager.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Development targets
run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the file manager"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  uninstall- Remove from /usr/local/bin"
	@echo "  run      - Build and run Casper"
	@echo "  debug    - Build with debug flags"
	@echo "  release  - Build optimized release version"
	@echo "  help     - Show this help message"
