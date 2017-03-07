#This is a simple API document#
*encode_frontend/main.c*
**encode_hiding** is a function that encode string passed with -s parameter in a file audio
```
int encode_hiding(int *to_hide, int tohide_lenght, lame_t gf, FILE * outf, char *inPath, char *outPath)
{



	unsigned char mp3buffer[LAME_MAXMP3BUFFER];
	int     Buffer[2][1152];
	int     iread, imp3, owrite;
	size_t  id3v2_size;

	encoder_progress_begin(gf, inPath, outPath);

	id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
	if (id3v2_size > 0) {
		unsigned char *id3v2tag = malloc(id3v2_size);
		if (id3v2tag != 0) {
			imp3 = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
			owrite = (int)fwrite(id3v2tag, 1, imp3, outf);
			free(id3v2tag);
			if (owrite != imp3) {
				encoder_progress_end(gf);
				error_printf("Error writing ID3v2 tag \n");
				return 1;
			}
		}
	}
	else {
		unsigned char* id3v2tag = getOldTag(gf);
		id3v2_size = sizeOfOldTag(gf);
		if (id3v2_size > 0) {
			size_t owrite = fwrite(id3v2tag, 1, id3v2_size, outf);
			if (owrite != id3v2_size) {
				encoder_progress_end(gf);
				error_printf("Error writing ID3v2 tag \n");
				return 1;
			}
		}
	}
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}

	int offset = 0;


	int *bit_to_hide, num_bit_to_encode_iter = 0;

	do {

		/* read in 'iread' samples */
		iread = get_audio(gf, Buffer);

		if (iread >= 0) {

			if (offset < tohide_lenght) {

				int max_bit_to_hide = iread / SAMPLES_PER_GOS;

				if ((tohide_lenght - offset) / max_bit_to_hide < 1)
					num_bit_to_encode_iter = (tohide_lenght - offset) % max_bit_to_hide;
				else
					num_bit_to_encode_iter = max_bit_to_hide;

				bit_to_hide = malloc(sizeof(int)* num_bit_to_encode_iter);

				int xx = 0, yy = 0;

				for (xx = offset; xx < offset + num_bit_to_encode_iter; xx++) {
					bit_to_hide[yy++] = to_hide[xx];
				}

				offset += num_bit_to_encode_iter;


				//printf("Offset: %d | num_bit_to_encode_iter: %d\n", offset, num_bit_to_encode_iter);
				  
				amplitude_hiding(bit_to_hide, Buffer[0], Buffer[1], iread, num_bit_to_encode_iter);

				int *xxx = extraction_data(gf, Buffer[0], Buffer[1], iread);

				int ggg = 0;
				for (; ggg < num_bit_to_encode_iter; ggg++) {
					if (bit_to_hide[ggg] != xxx[ggg])
						printf("\nErrore nella codifica del bit: %d %d-%d", ggg, bit_to_hide[ggg], xxx[ggg]);
					//else printf("\nBit codificato correttamente: %d %d-%d", ggg, bit_to_hide[ggg], xxx[ggg]);
					
				}
			}

			encoder_progress(gf);

			//printf("Offset: %d | num_bit_to_encode_iter: %d - %d \n", offset, num_bit_to_encode_iter, tohide_lenght);


			imp3 = lame_encode_buffer_int(gf, Buffer[0], Buffer[1], iread,
				mp3buffer, sizeof(mp3buffer));

			/* was our output buffer big enough? */
			if (imp3 < 0) {
				if (imp3 == -1)
					error_printf("mp3 buffer is not big enough... \n");
				else
					error_printf("mp3 internal error:  error code=%i\n", imp3);
				return 1;
			}
			owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
			if (owrite != imp3) {
				error_printf("Error writing mp3 output \n");
				return 1;
			}
		}
		if (global_writer.flush_write == 1) {
			fflush(outf);
		}
	} while (iread > 0);

	imp3 = lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */


	if (imp3 < 0) {
		if (imp3 == -1)
			error_printf("mp3 buffer is not big enough... \n");
		else
			error_printf("mp3 internal error:  error code=%i\n", imp3);
		return 1;

	}

	encoder_progress_end(gf);

	owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
	if (owrite != imp3) {
		error_printf("Error writing mp3 output \n");
		return 1;
	}
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	imp3 = write_id3v1_tag(gf, outf);
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	if (imp3) {
		return 1;
	}
	write_xing_frame(gf, outf, id3v2_size);
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	if (global_ui_config.silent <= 0) {
		print_trailing_info(gf);
	}
	fclose(outf); 
	return 0;
}
```
**amplitude_hiding** is a function that modify amplitude in according to paper for encode bit in a stream audio
```
/*
to_hide - array of int containing the bits of data to hide. Maybe we can compress it to increase amount of hidden data
lr_sound - array containing the wav samples, [0][] left channel, [1][] right channel
lr_sound_size - #samples in lr_sound per channel
*/
void
amplitude_hiding(int to_hide[], int l_sound[], int r_sound[], int lr_sound_size, int elem_to_hide) {

	int i = 0;

	int number_of_gos = lr_sound_size / SAMPLES_PER_GOS, gos_start = 0;

	//The size of section of gos could be different from each other. We use, at this moment, the same size for each one.
	int section1_size = SAMPLES_PER_GOS_SECTION,
		section2_size = SAMPLES_PER_GOS_SECTION,
		section3_size = SAMPLES_PER_GOS_SECTION;

	// Variabile usata per avanzamento GOS
	int L = section1_size + section2_size + section3_size;

	for (; i < number_of_gos && i < elem_to_hide; i++) {

		//every parameter is an array to consider both channels
		double e1[2] = { 0, 0 }, e2[2] = { 0, 0 }, e3[2] = { 0, 0 },
			e_max[2] = { 0, 0 }, e_min[2] = { 0, 0 }, e_mid[2] = { 0, 0 };//, E[2] = { 0, 0 } , *e[3] = { e1, e2, e3 };
		int index_max[2] = { 0,0 }, index_min[2] = { 0,0 }, index_mid[2] = { 0,0 }, l[3] = { section1_size, section2_size, section3_size };

		double d = 0.01;

		// Flag per il while
		int flag = 1;
		calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound, r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);

		double THRESHOLD[2] = { (e_max[LEFT] + (2 * e_mid[LEFT]) + e_min[LEFT]) * d, (e_max[R] + (2 * e_mid[R]) + e_min[R]) * d };

		//printf("Il valore del THRESHOLD prima del while è : %1.10f - %1.10f \n \n ", THRESHOLD[LEFT], THRESHOLD[R]);
		while (flag) {
			// Inizializza le variabili ad ogni ciclo
			double delta[2] = { 0,0 }, omega_up[2] = { 0,0 }, omega_down[2] = { 0,0 };
			int offset_up[2] = { gos_start,gos_start }, offset_down[2] = { gos_start, gos_start };

			calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound, r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);

			//printf("\n VALUES: \n\n\n%Lf - %Lf - %Lf - %Lf - %Lf - %Lf - %d - %d - %d - %d - %d - %d \n\n\n\n\n\n",
			//      e1[0], e2[0], e3[0], e_max[0], e_min[0], e_mid[0], section1_size, section2_size, section3_size, index_max[0], index_mid[0], index_min[0]);

			// Calcola A e B in base ai valori calcolati da calculate_e_values()
			double A[2] = { e_max[0] - e_mid[0], e_max[1] - e_mid[1] },
				B[2] = { e_mid[0] - e_min[0], e_mid[1] - e_min[1] };

			//double D[2] = { abs(A[0] - B[0]), abs(A[1] - B[1]) };

			// Calcola THRESHOLD in base ai valori di calculate_e_values()

			double subtraction[2] = { 0, 0 };
			int success[2] = { 0, 0 };
//			printf("IL bit da nascondere è : %d \n", to_hide[i]);

			if (to_hide[i]) {
				subtraction[LEFT] = A[LEFT] - B[LEFT];
				subtraction[R] = A[R] - B[R];
			}
			else {
				subtraction[LEFT] = B[LEFT] - A[LEFT];
				subtraction[R] = B[R] - A[R];
			}
			success[LEFT] = (subtraction[LEFT] >= THRESHOLD[LEFT]);
			success[R] = (subtraction[R] >= THRESHOLD[R]);

			//printf("Il valore della sottrazione è : %1.10f\n", subtraction);

			/* DEBUG ZONE */
			//printf("Valore subtraction : %1.50f Valore THRESHOLD : %1.50f \n", subtraction, THRESHOLD);

			// Controlla superamento soglia
			if (success[LEFT] && success[R]) { // se soglia superata
				flag = 0;
				break; // esci immediatamente
			}

			//Inizio modifica canale L
			if (!success[LEFT]) {
				
				delta[LEFT] = (THRESHOLD[LEFT] - subtraction[LEFT]) / 3;

				//				printf("IL valore del delta %d\n", delta);
				//CALCOLO DELTA
				if (to_hide[i]) {

					//modifico parametri e
					e_max[0] += delta[LEFT];
					e_mid[0] -= delta[LEFT];

					//	printf("Il valore di OMEGA UP è: %1.10f\n", omega_up);
					//	printf("Il valore di OMEGA DOWN è: %1.10f\n\n \n \n", omega_down);


					//calcolo variazione ampiezza
					omega_up[LEFT] = 1 + (delta[LEFT] / e_max[0]);
					omega_down[LEFT] = 1 - (delta[LEFT] / e_mid[0]);

					int i = 0;
					for (; i < index_max[0]; i++) {
						offset_up[LEFT] += l[i];
					}
					for (i = 0; i < index_mid[0]; i++) {
						offset_down[LEFT] += l[i];
					}
					modify_amplitude(l_sound, l[index_max[0]], omega_up[LEFT], LEFT, offset_up[LEFT]);
					//		printf("l_sound[%d]:%Lf - ", offset_up, l_sound[offset_up]);
					modify_amplitude(l_sound, l[index_mid[0]], omega_down[LEFT], LEFT, offset_down[LEFT]);
					//			printf("l_sound[%d]:%Lf \n ", offset_down, l_sound[offset_down]);
				}
				else { //SE DEVO NASCONDERE 0
					e_mid[0] += delta[LEFT];
					e_min[0] -= delta[LEFT];
					omega_up[LEFT] = 1 + (delta[LEFT] / e_mid[0]);
					omega_down[LEFT] = 1 - (delta[LEFT] / e_min[0]);

					//printf("Il valore diDELTA è: %1.10f\n", delta[LEFT]);

					//printf("Il valore di OMEGA UP è: %1.10f\n", omega_up[LEFT]);
					//printf("Il valore di OMEGA DOWN è: %1.10f\n \n \n \n", omega_down[LEFT]);

					int i = 0;
					for (; i < index_mid[0]; i++) {
						offset_up[LEFT] += l[i];
					}
					for (i = 0; i < index_min[0]; i++) {
						offset_down[LEFT] += l[i];
					}
					modify_amplitude(l_sound, l[index_mid[0]], omega_up[LEFT], LEFT, offset_up[LEFT]);
					modify_amplitude(l_sound, l[index_min[0]], omega_down[LEFT], LEFT, offset_down[LEFT]);

				}
				//sleep(2);
			} // end if success

			//Inizio modifica canale R
			if (!success[R]) { 

				delta[R] = (THRESHOLD[R] - subtraction[R]) / 3;

				//				printf("IL valore del delta %d\n", delta);
				//CALCOLO DELTA
				if (to_hide[i]) {

					//modifico parametri e
					e_max[R] += delta[R];
					e_mid[R] -= delta[R];

					//	printf("Il valore di OMEGA UP è: %1.10f\n", omega_up);
					//	printf("Il valore di OMEGA DOWN è: %1.10f\n\n \n \n", omega_down);


					//calcolo variazione ampiezza
					omega_up[R] = 1 + (delta[R] / e_max[R]);
					omega_down[R] = 1 - (delta[R] / e_mid[R]);

					int i = 0;
					for (; i < index_max[R]; i++) {
						offset_up[R] += l[i];
					}
					for (i = 0; i < index_mid[R]; i++) {
						offset_down[R] += l[i];
					}
					modify_amplitude(r_sound, l[index_max[R]], omega_up[R], LEFT, offset_up[R]);
					//		printf("l_sound[%d]:%Lf - ", offset_up, l_sound[offset_up]);
					modify_amplitude(r_sound, l[index_mid[R]], omega_down[R], LEFT, offset_down[R]);
					//			printf("l_sound[%d]:%Lf \n ", offset_down, l_sound[offset_down]);
				}
				else { //SE DEVO NASCONDERE 0
					e_mid[R] += delta[R];
					e_min[R] -= delta[R];
					omega_up[R] = 1 + (delta[R] / e_mid[R]);
					omega_down[R] = 1 - (delta[R] / e_min[R]);

					//printf("Il valore diDELTA è: %1.10f\n", delta[R]);

					//printf("Il valore di OMEGA UP è: %1.10f\n", omega_up[R]);
					//printf("Il valore di OMEGA DOWN è: %1.10f\n \n \n \n", omega_down[R]);

					int i = 0;
					for (; i < index_mid[R]; i++) {
						offset_up[R] += l[i];
					}
					for (i = 0; i < index_min[R]; i++) {
						offset_down[R] += l[i];
					}
					modify_amplitude(r_sound, l[index_mid[R]], omega_up[R], LEFT, offset_up[R]);
					modify_amplitude(r_sound, l[index_min[R]], omega_down[R], LEFT, offset_down[R]);

				}
				//sleep(2);
			}
		} // end while
		  
		  // A fine while, cambia il gos
		gos_start += L - 1;
	} // end for
} // end function

```
