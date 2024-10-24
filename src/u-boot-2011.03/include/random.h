/* concatenation of following two 16-bit multiply with carry generators */
/* x(n)=a*x(n-1)+carry mod 2^16 and y(n)=b*y(n-1)+carry mod 2^16, */
/* number and carry packed within the same 32 bit integer.        */
/******************************************************************/

unsigned int rand( void );           /* returns a random 32-bit integer */
void  rand_seed( unsigned int, unsigned int );      /* seed the generator */
