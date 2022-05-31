float **matrix(int nrl, int nrh, int ncl, int nch);
/* Allocates a float matrix with range [nrl..nrh][ncl..nch] */

void free_matrix(float **m, int nrl, int nrh, int ncl, int nch);
/* Frees a matrix allocated with matrix() */
