/*
* Author: Emre Erdogan (emre@emrerdogan.net)
* Date: June 4th, 2015
*/

#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

#define is_empty_cell(x) x == 0

#define MASTER 0
#define WORKER_ROW 1
#define WORKER_COL 2
#define WORKER_BOX 3

#define TAG_TASK_ASSIGNMENT_ROW		1
#define TAG_TASK_ASSIGNMENT_COL		2
#define TAG_TASK_ASSIGNMENT_BOX		3

#define TAG_TASK_COMPLETED_ROW		4
#define TAG_TASK_COMPLETED_COL		5
#define TAG_TASK_COMPLETED_BOX		6

void print_board(int matrix[9][9]);
void print_array(int array[9]);
int is_solved(int matrix[9][9]);
void get_row_cells(int matrix[9][9], int x, int row[9]);
void get_col_cells(int matrix[9][9], int y, int col[9]);
void get_box_cells(int matrix[9][9], int x, int y, int box[9]);
int get_single_intersection(int avs_row[9], int avs_col[9], int avs_box[9]);

int main(int argc, char** argv){

	int rank, world_size;
	int i,j;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);


	if(rank == MASTER){
		// create the initial puzzle
		int board[9][9] = {
			{0,5,9,0,2,0,4,6,0},
			{1,0,0,4,0,3,0,0,8},
			{3,0,0,0,7,0,0,0,2},
			{0,3,0,8,0,9,0,2,0},
			{6,0,5,0,0,0,3,0,7},
			{0,1,0,7,0,6,0,4,0},
			{2,0,0,0,1,0,0,0,4},
			{9,0,0,3,0,2,0,0,5},
			{0,7,8,0,6,0,2,3,0}
		};

		int solved = 0;
		
		int row[9];
		int col[9];
		int box[9];
		int avs_row[9]; // available values for the examined row
		int avs_col[9]; // available values for the examined col
		int avs_box[9]; // available values for the examined box
		int count_avs_row;
		int count_avs_col;
		int count_avs_box;
		int single_common_value;

		// test it
		// print_board(board);

		while(!solved){
			for(i=0; i<9 && !solved; ++i){
				for (j=0; j<9 && !solved; ++j){
					if(is_empty_cell(board[i][j])){
						// send tasks to workers
						printf("Sending tasks to workers. (x,y) = (%d,%d)\n", i, j);						

						// 1. send this row to ROW WORKER
						get_row_cells(board, i, row);
						MPI_Send(&row, 9, MPI_INT, WORKER_ROW, TAG_TASK_ASSIGNMENT_ROW, MPI_COMM_WORLD);

						// 2. send this col to COL WORKER
						get_col_cells(board, j, col);
						MPI_Send(&col, 9, MPI_INT, WORKER_COL, TAG_TASK_ASSIGNMENT_COL, MPI_COMM_WORLD);

						// 3. send this box to BOX WORKER
						get_box_cells(board, i, j, box);
						MPI_Send(&box, 9, MPI_INT, WORKER_BOX, TAG_TASK_ASSIGNMENT_BOX, MPI_COMM_WORLD);

						// receive results from the workers
						MPI_Recv(&avs_row, 9, MPI_INT, WORKER_ROW, TAG_TASK_COMPLETED_ROW, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						MPI_Recv(&avs_col, 9, MPI_INT, WORKER_COL, TAG_TASK_COMPLETED_COL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						MPI_Recv(&avs_box, 9, MPI_INT, WORKER_BOX, TAG_TASK_COMPLETED_BOX, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

						single_common_value = get_single_intersection(avs_row, avs_col, avs_box);

						if(single_common_value != 0){
							// this is the value that we've been looking for
							board[i][j] = single_common_value;
						}
						
						// check if the puzzle is solved and send the information to the workers
						solved = is_solved(board);
						MPI_Bcast(&solved, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
						MPI_Barrier(MPI_COMM_WORLD);

						if(solved){
							printf("Sudoku has been solved.\n\n");
							print_board(board);
						}
					}
				}
			}
		}

		// if we came up to this location, then the puzzle must have been solved.
		printf("Sudoku has been solved.\n\n");
		print_board(board);


	} else {
		int local_search_space[9]; // this array holds a row, a col, or a box depending on 'this' process and current state of the board
		int available_values[9]; // holds available values
		int count; // holds the number of available values found
		int val;
		int solved = 0;

		while(!solved){

			if(rank == WORKER_ROW){
				MPI_Recv(&local_search_space, 9, MPI_INT, MASTER, TAG_TASK_ASSIGNMENT_ROW, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				printf("ROW WORKER received task for row: ");
				print_array(local_search_space);
			} else if(rank == WORKER_COL){
				MPI_Recv(&local_search_space, 9, MPI_INT, MASTER, TAG_TASK_ASSIGNMENT_COL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				printf("COL WORKER received task for col: ");
				print_array(local_search_space);
			} else if(rank == WORKER_BOX) {
				MPI_Recv(&local_search_space, 9, MPI_INT, MASTER, TAG_TASK_ASSIGNMENT_BOX, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				printf("BOX WORKER received task for box: ");
				print_array(local_search_space);
			} else {
				// no more workers needed
			}						

			count = 0;
			// find available values (unused values. i.e. if the array is "4 0 5 1 9 0 0 6 7", then available values are 2, 3, and 8)
			for(val=1; val<=9; ++val){
				// by the way resetting the array. so, a zero means "ignore"
				// resetting is done in this loop, because we care about performance
				available_values[val-1] = 0;

				// seeking for val
				for(j=0; j<9; ++j){
					if(local_search_space[j] == val){
						continue; // val is already used. we need an unused value
					}
				}
				if(j == 9){
					// val is available
					available_values[count] = val;
					++count;
				}
			}

			// now we have the values (the candidates), let's send them to the master
			if(rank == WORKER_ROW){
				MPI_Send(&available_values, 9, MPI_INT, MASTER, TAG_TASK_COMPLETED_ROW, MPI_COMM_WORLD);
			} else if(rank == WORKER_COL){
				MPI_Send(&available_values, 9, MPI_INT, MASTER, TAG_TASK_COMPLETED_COL, MPI_COMM_WORLD);
			} else if(rank == WORKER_BOX) {
				MPI_Send(&available_values, 9, MPI_INT, MASTER, TAG_TASK_COMPLETED_BOX, MPI_COMM_WORLD);
			} else {
				// no more workers needed
			}

			// keep receiving tasks until the puzzle is solved
			MPI_Bcast(&solved, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}

	MPI_Finalize();

	return 0;

}

void print_board(int matrix[9][9]){
	int i, j;
	for(i=0; i<9; ++i){
		if(i%3 == 0){
			printf("-------------------------------\n");
		}
		for (j=0; j<9; ++j){
			if(j%3 == 0){
				printf("|");
			}
			printf(" %d ", matrix[i][j]);
		}

		printf("|\n");
	}
	printf("-------------------------------\n");
}

void print_array(int array[9]){
	int i;
	for(i=0; i<9; ++i){
		printf(" %d", array[i]);
	}
	printf("\n");
}

int is_solved(int matrix[9][9]){
	int i, j, solved = 1;
	for(i=0; i<9; ++i){
		for (j=0; j<9; ++j){
			if(is_empty_cell(matrix[i][j])){
				return 0;
			}
		}
	}
	return solved;
}

/**
* fills the given 9-element array with the values of the row that 
* corresponds to the cell at position x, y of the game board
*/
void get_row_cells(int matrix[9][9], int x, int row[9]){
	int j;
	
	// row is fixed
	for(j=0; j<9; ++j){
		row[j] = matrix[x][j];
	}
}

/**
* fills the given 9-element array with the values of the column that
* corresponds to the cell at position x, y of the game board
*/
void get_col_cells(int matrix[9][9], int y, int col[9]){
	int i;
	
	// col is fixed
	for(i=0; i<9; ++i){
		col[i] = matrix[i][y];
	}
}

/**
* fills the given 9-element array with the values of the box that
* corresponds to the cell at position x, y of the game board
*/
void get_box_cells(int matrix[9][9], int x, int y, int box[9]){
	int i, j, i_start, j_start, i_end, j_end, counter = 0; 
	i_start = (x/3) * 3; // means floor(x/3) * 3
	j_start = (y/3) * 3; // means floor(y/3) * 3
	i_end = i_start + 3;
	j_end = j_start + 3;


	for(i=i_start; i<i_end; ++i){
		for(j=j_start; j<j_end; ++j){
			box[counter] = matrix[i][j];
		}
	}
}

int get_single_intersection(int avs_row[9], int avs_col[9], int avs_box[9]){
	int i;
	int val = 0;

	for(i=0; i<9; ++i){
		if(avs_row[i] > 0 && avs_row[i] == avs_col[i] && avs_row[i] == avs_box[i]){
			if(val > 0){
				// an intersecting value already exists. 
				// so, it means there is no single intersecting value accross all arrays
				// thus, we return a zero, which means no-single-intersection 
				// (the length of the intersection set of the three arrays is greater than 1)
				return 0; 
			}
			val = avs_row[i];
		}
	}

	return val;
}
