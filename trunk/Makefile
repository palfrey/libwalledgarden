CC=gcc
CFLAGS=-Wall -g -Werror -O0 `pkg-config --cflags glib-2.0`
LDFLAGS=-lcurl -lraptor -lpcreposix `pkg-config --libs glib-2.0`
OBJ=browser.o common.o cookies.o okcupid.o
DEPDIR = .deps
df = $(DEPDIR)/$(*F)
MAKEDEPEND = gcc -M $(CPPFLAGS) -o $*.d $<

all:lj

lj: $(OBJ) lj.o
	$(CC) -o lj $^ $(LDFLAGS)


%.o : %.c
	  $(COMPILE.c) -MD -o $@ $< -MF $(DEPDIR)/$(patsubst %.c,%.P,$<)

-include $(patsubst %.o,$(DEPDIR)/%.P,$(OBJ))

