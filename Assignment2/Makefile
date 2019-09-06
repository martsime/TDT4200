
DEPS = bitmap.h
MPICC = mpicc

main : main.o bitmap.o
	$(MPICC) main.o bitmap.o -o main 

# % matches anything, $< refers to the left hand side contents and $@ refers to the LHS
#Common header as dependency to force recompilation if it is modified
%.o : %.c $(DEPS)
	$(MPICC) -c $< -o $@

phony : clean
clean: 
	rm -f *.o
	rm -f after*.bmp
