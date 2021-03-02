# 交叉编译器选择(没有则注释掉)
#cross:=arm-linux-gnueabihf
#cross:=mips-linux-gnu

# 编译器配置
CC:=gcc

# 用于依赖库编译
HOST:=
ifdef cross
	HOST=$(cross)
	CC=$(cross)-gcc
endif

# 根目录获取
ROOT=$(shell pwd)

# 依赖库参数
LIBS_PATH = -L./libs/lib
LIBS_INC = -I./libs/include
LIBS = -lm -lpthread -ljpeg

# 源文件夹列表
DIR_SRC = src
DIR_OBJ = obj

# 头文件目录列表
INC = -I$(DIR_SRC)

# obj中的.o文件统计
obj += ${patsubst %.c,$(DIR_OBJ)/%.o,${notdir ${wildcard $(DIR_SRC)/*.c}}}

# 文件夹编译及.o文件转储
%.o:../$(DIR_SRC)/%.c
	@$(CC) -Wall -c $< $(INC) $(LIBS) $(LIBS_INC) $(LIBS_PATH) -o $@

# 目标编译
target: $(obj)
	@$(CC) -Wall -o app $(obj) $(INC) $(LIBS) $(LIBS_INC) $(LIBS_PATH)
clean:
	@rm ./obj/* app out.* -rf
cleanall: clean
	@rm ./libs/* -rf

# 所有依赖库
libs: libjpeg
	@echo "---------- complete !! ----------"

# 注意这里libtool是用gcc编译并运行于当前系统
libtool:
	@tar -xzf $(ROOT)/pkg/libtool-2.4.tar.gz -C $(ROOT)/libs && \
	cd $(ROOT)/libs/libtool-2.4 && \
	./configure --prefix=$(ROOT)/libs --host=$(HOST) --disable-ltdl-install && \
	make -j4 && make install && \
	cd - && \
	rm $(ROOT)/libs/libtool-2.4 -rf

# 编译libjpeg需要libtool工具
libjpeg: libtool
	@tar -xzf $(ROOT)/pkg/jpeg-6b.tar.gz -C $(ROOT)/libs && \
	cd $(ROOT)/libs/jpeg-6b && \
	./configure --prefix=$(ROOT)/libs --host=$(HOST) --enable-shared && \
	sed -i 's/CC= gcc/CC= $(CC)/' ./Makefile && \
	cp $(ROOT)/libs/bin/libtool ./ && \
	mkdir $(ROOT)/libs/man/man1 -p && \
	make -j4 && make install && \
	sed -i '/^#define JPEGLIB_H/a\#include <stddef.h>' ../include/jpeglib.h && \
	cd - && \
	rm $(ROOT)/libs/jpeg-6b -rf
