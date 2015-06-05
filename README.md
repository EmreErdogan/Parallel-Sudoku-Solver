# Parallel-Sudoku-Solver

This code may not solve every sudoku puzzle. 
I coded this just for improving my parallel programming skills. 

Compile
--------
`mpicc parallel_sudoku_solver.c -o solver`

Run
---
This solver needs 4 processes. So execute the following command:

`mpirun -np 4 solver`
