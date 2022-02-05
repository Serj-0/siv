BINDIR = /usr/bin
MANDIR = /usr/share/man/man1
GW = SDL_gifwrap

CFLAGS = -Wall -c -fpic 
CPPFLAGS = -Wall main.cpp -lSDL2main -lSDL2 -lSDL2_image -lgif -LSDL_gifwrap -lgifwrap -lpthread -lboost_filesystem

build:
	./deps
	$(CC) $(CFLAGS) $(GW)/$(GW).c -o $(GW)/$(GW).o
	$(AR) rcs $(GW)/libgifwrap.a $(GW)/$(GW).o
	$(CXX) $(CPPFLAGS) -o siv

install: build
	mv siv $(BINDIR)/siv

uninstall:
	rm $(BINDIR)/siv

clean:
	rm -r $(GW)
