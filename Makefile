TARGET = procnanny

$(TARGET): procnanny.c
	@gcc -Wall -DMEMWATCH -DMW_STDIO procnanny.c memwatch.c -o $(TARGET)
clean:
	@-rm -f *.o
	@-rm -f $(TARGET)