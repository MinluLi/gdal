
include .. / .. / GDALmake.opt

OBJ = cnsdtfsrs.o ascdataset.o cnsdtfdataset.o nsdtfdataset.o imageinfodataset.o

CPPFLAGS : = $(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT)

default:	$(OBJ : .o = .$(OBJ_EXT))

	clean :
	rm - f * .o

	all : $(OBJ : .o = .$(OBJ_EXT))

	install - obj : $(O_OBJ : .o = .$(OBJ_EXT))
