CC=gcc
CFLAGS=`gdal-config --cflags`
LIBS=`gdal-config --libs`

program:
	$(CC) $(CFLAGS) -o gdal_program gdal_program.c $(LIBS)
	$(CC) $(CFLAGS) -o gdal_program_locality_old gdal_program_locality_old.c $(LIBS)
	$(CC) $(CFLAGS) -o gdal_program_locality gdal_program_locality.c $(LIBS)

compressed:
	# $(CC) $(CFLAGS) -o gdal_program_locality_compression gdal_program_locality_compression.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm
	$(CC) $(CFLAGS) -o gdal_program_compression gdal_program_compression.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm

compressed_write:
	$(CC) $(CFLAGS) -o gdal_program_compression_write gdal_program_compression_write.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm

compressed_read_flat:
	$(CC) $(CFLAGS) -o gdal_program_compression_read_flat gdal_program_compression_read_flat.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm
compressed_read_flat2:
	$(CC) $(CFLAGS) -o gdal_program_compression_read_flat2 hmm3/gdal_program_compression_read_flat.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm


quadtree:
	$(CC) $(CFLAGS) -o gdal_program_quadtree hmm/main.c hmm/quadtree.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm

grid:
	$(CC) $(CFLAGS) -o gdal_program_grid hmm2/main.c $(LIBS) -I./TurboPFor/include -L./TurboPFor -lic -lm

clean:
	rm -f gdal_program
