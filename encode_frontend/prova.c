#include <stdio.h>
#include <stdlib.h>
#include <math.h>

char* bit_to_string(int *bits) {
  
  int i = 0, j =0;
  int length = sizeof(bits) * sizeof(bits[0]);


  char *string;

  int temp = 0;

  string = malloc(sizeof(char) * length);

  for (; i < length / 8; i++) {

    for (j=0;j <8; j++) {
    	temp = (temp <<1)|bits [i*8+j];
      printf("Temp vale %d \n",temp );
    }
   
    string[i] = (char)temp;
    temp =0;
  }
  return string;
}


int main(void)
{

	int data[32] = {0,1,1,0,0,0,1,1,0,1,1,0,1,0,0,1,0,1,1,0,0,0,0,1,0,1,1,0,1,1,1,1};
    printf("%s\n",bit_to_string(data));
}