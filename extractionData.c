







/*
gf -> for getaudio
l_sound r_sound -> left and right channel 
lr_soundsize -> size of song 
*/


int*
data_extraction(lame_t gf, int l_sound[], int r_sound[], int lr_sound_size)
{
	int i = 0;

	int number_of_gos = lr_sound_size / SAMPLES_PER_GOS;

	int array_of_byte[] = malloc(number_of_gos * sizeof(int));
	//The size of section of gos could be different from each otnd[]her. We use, at this moment, the same size for each one.

	int section1_size = SAMPLES_PER_GOS_SECTION,
		section2_size = SAMPLES_PER_GOS_SECTION,
		section3_size = SAMPLES_PER_GOS_SECTION;


	// Finish when all gos are analized.
	for (; i < number_of_gos; i++) {


		//every parameter is an array to consider both channels
		double e1[2] = { 0, 0 }, e2[2] = { 0, 0 }, e3[2] = { 0, 0 },
			e_max[2] = { 0, 0 }, e_min[2] = { 0, 0 }, e_mid[2] = { 0, 0 };//, E[2] = { 0, 0 } , *e[3] = { e1, e2, e3 };
		int index_max[2] = { 0,0 }, index_min[2] = { 0,0 }, index_mid[2] = { 0,0 }, l[3] = { section1_size, section2_size, section3_size };

		calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound,r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);
		int A[2]={0,0},B[2]={0,0};
		
		double A[2] = { e_max[0] - e_mid[0], e_max[1] - e_mid[1] },
				B[2] = { e_mid[0] - e_min[0], e_mid[1] - e_min[1] };

		// Add channel 2
		if(A[0] >= B[0]){

			array_of_byte[i] = 1;
			//printf("1\n");
		}
		else if(B[0] > A[0]){
			array_of_byte[i] = 1;
			//printf("0\n");
		}
	}
	return array_of_byte;

}