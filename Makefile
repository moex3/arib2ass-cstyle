
#CFLAGS += -O0 -g3 -ggdb -Wall -std=gnu11 -Wno-unused-function -Wno-unused-variable -fsanitize=address -fsanitize=leak -fsanitize=undefined
#CFLAGS += -O0 -g3 -ggdb -Wall -std=gnu11 #-fsanitize=address -fsanitize=leak -fsanitize=undefined
#CFLAGS += -O0 -g3 -ggdb -Wall -std=gnu11 -Wno-unused-function -Wno-unused-variable #-fsanitize=address -fsanitize=leak -fsanitize=undefined
#CFLAGS += -O2 -Wall -std=gnu11 -Wno-unused-function -Wno-unused-variable #-fsanitize=address -fsanitize=leak -fsanitize=undefined
CFLAGS += -O2 -Wall -std=gnu11 -march=native -mtune=native

CFLAGS += -I ./subm/toml-c/ -D_GNU_SOURCE $(shell pkg-config --cflags freetype2 libavcodec libavformat libavutil)
LIBS += -lm -lstdc++ $(shell pkg-config --libs freetype2 libavcodec libavformat libavutil)

GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
CFLAGS += -Isubm/libaribcaption/include/ -Isubm/libaribcaption/build/include/

SOURCES := $(wildcard src/*.c ./subm/toml-c/toml.c)
OBJS := $(SOURCES:.c=.o)

a2ac: $(OBJS) subm/libaribcaption/build/libaribcaption.a
	${CC} $^ ${CFLAGS} -o $@ ${LIBS}

src/%.o: src/%.c subm/libaribcaption/build/include/aribcc_config.h
	${CC} $< ${CFLAGS} -c -o $@

src/opts.o: src/opts.c src/version.h

src/version.h: force
	printf "#ifndef A2AC_VERSION\n#define A2AC_VERSION \"%s\"\n#endif\n" $(GIT_VERSION) > $@

subm/libaribcaption/build/libaribcaption.a subm/libaribcaption/build/include/aribcc_config.h: subm/libaribcaption/build/Makefile
	cd subm/libaribcaption/build; \
	cmake --build . -j$(shell nproc)

subm/libaribcaption/build/Makefile:
	mkdir -p subm/libaribcaption/build; \
	cd subm/libaribcaption/build; \
	cmake .. -DCMAKE_BUILD_TYPE=Release -DARIBCC_NO_EXCEPTIONS:BOOL=ON -DARIBCC_NO_RENDERER:BOOL=ON

g: a2ac
	gdb ./a2ac

clean:
	-rm -- a2ac $(OBJS)

distclean: clean
	-rm -r -- subm/libaribcaption/build

.PHONY: clean distclean g force
