OBJECTS = nsh.o
nsh: $(OBJECTS)
  g++ $^ -o nsh
clean:
  rm -f *.o nsh
