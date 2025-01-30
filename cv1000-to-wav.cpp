#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <math.h>
#include "mpeg_audio.cpp"

	struct ymz_channel
	{
		uint16_t phrase;
		uint8_t pan;
		uint8_t pan_delay;
		uint8_t pan1;
		uint8_t pan1_delay;
		int32_t volume;
		uint8_t volume_target;
		uint8_t volume_delay;
		uint8_t volume2;
		uint8_t loop;

		bool is_playing, last_block, is_paused;

		mpeg_audio *decoder;

		int16_t output_data[0x1000];
		int output_remaining;
		int output_ptr;
		int atbl;
		int pptr;
	};
	
	typedef struct wav_header {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    uint32_t wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
	char wave_header[4]; // Contains "WAVE"
    
    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    uint32_t fmt_chunk_size; // Should be 16 for PCM
    uint16_t audio_format; // Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    uint16_t sample_alignment; // num_channels * Bytes Per Sample
    uint16_t bit_depth; // Number of bits per sample
    
    // Data
    char data_header[4]; // Contains "data"
    uint32_t data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
    } wav_header;
	
void decode_amm(unsigned char* buf, unsigned long int start_offset, unsigned long int entry_length, unsigned long int table_index) {
     
  int sample_rate, channel_count;
  bool result;
  ymz_channel channel;
  wav_header wavheader;
  FILE *fout2;
  char filename[50];
  
  strncpy(wavheader.riff_header, "RIFF", 4);
  strncpy(wavheader.wave_header, "WAVE", 4);
  strncpy(wavheader.fmt_header, "fmt ", 4);
  strncpy(wavheader.data_header, "data", 4);
  wavheader.fmt_chunk_size = 16;
  wavheader.audio_format = 1;
  wavheader.bit_depth = 16;  
      
  channel.decoder = new mpeg_audio(&buf[start_offset], mpeg_audio::AMM, false, 0);      
  channel.pptr = 0;
  
  sprintf(filename, "sample%03d.wav", table_index); 
  fout2 = fopen(filename, "wb");
  
  printf("Writing %s: Offset %u Length %u\n", filename, start_offset, entry_length);
  
  fseek(fout2, 44, SEEK_SET);
  
  do {
  
  result = channel.decoder->decode_buffer(channel.pptr, entry_length*8, channel.output_data, channel.output_remaining, sample_rate, channel_count);  
  
  wavheader.num_channels = channel_count;
  wavheader.sample_alignment = ((wavheader.bit_depth) / 8) * channel_count;
  wavheader.sample_rate = sample_rate;
  wavheader.byte_rate = sample_rate * wavheader.sample_alignment;
  
    // printf("return %u pptr: %u output_remaining: %u sample_rate: %u channel_count: %u\n", result, channel.pptr / 8, channel.output_remaining, sample_rate, channel_count);

	fwrite(&channel.output_data[0], 2, channel.output_remaining, fout2);
  
  	channel.last_block = channel.output_remaining < 1152;
	channel.output_remaining--;
	channel.output_ptr = 1;
  
  } while (channel.last_block != 1);

  wavheader.wav_size = ftell(fout2) - 8;  
  wavheader.data_bytes = ftell(fout2) - 44;
  fseek(fout2, 0, SEEK_SET);
  fwrite(&wavheader, 1, 44, fout2);
  fseek(fout2, 0, SEEK_END);
  
  fclose(fout2);

}


int main(int argc, char* argv[]) {

  unsigned long int i, j, bufsize;
  unsigned char *buf;
  const char *audio1 = "u23";
  const char *audio2 = "u24";
  const char *audio3 = "audio.bin";
  const long int table_entries = 0x300;
  unsigned long offset_table[table_entries];

  FILE *fin1, *fin2, *fout;
  
    printf ("cv1000-to-wav by alamone.  Uses MAME MPEG audio code by Olivier Galibert.\n\n");
	printf ("Input: \"u23\", \"u24\"\n");
	printf ("Output: sample000...sample255.wav\n\n");
	
	fin1 = fopen(audio1, "rb");
	if (!fin1) {
  	  printf("%s missing!\n", audio1);
	  return(1);
	} else {
      // ok
	}

	fin2 = fopen(audio2, "rb");
	if (!fin1) {
  	  printf("%s missing!\n", audio2);
	  return(1);
	} else {
      // ok
	}


  bufsize = 1024 * 1024 * 8;
  fseek(fin1, 0, SEEK_SET);
  fseek(fin2, 0, SEEK_SET);
  buf = (unsigned char *)malloc(bufsize);

  for(i=0; i < (bufsize / 2); i++) {
    fread(&buf[(i*2) + 1], 1, 1, fin1);
    fread(&buf[(i*2) + 0], 1, 1, fin1);
  }
  for(i=0; i < (bufsize / 2); i++) {
    fread((bufsize / 2) + &buf[(i*2) + 1], 1, 1, fin2);
    fread((bufsize / 2) + &buf[(i*2) + 0], 1, 1, fin2);
  }


	fout = fopen(audio3, "wb");
	if (!fout) {
	  printf("%s error.\n", audio3);
	  return(1);
	} else {
	  printf("creating %s file.\n", audio3);
	}
	  
  fwrite(&buf[0], 1, bufsize, fout);
  fclose(fout);

  
  // processing... 

  for(i=0; i < table_entries; i++) {
	j = // (buf[(i*4)+0] << 24) + 
	    (buf[(i*4)+1] << 16) + 
		(buf[(i*4)+2] << 8) + 
		(buf[(i*4)+3]);
    // printf ("index %u offset %u\n", i, j);	
	offset_table[i] = j;
  }

  unsigned long int start_offset;
  unsigned long int entry_length;

  for(i=0; i < 0xFF; i++) {
	if (offset_table[i] < 8388608) {
	  for (j=i+1; j < table_entries - 1; j++) {
		if ((offset_table[j] > offset_table[i]) && (offset_table[j] < 8388608)) { break; }
	  }
	  if ((offset_table[j] - offset_table[i]) > 0) {
		
		start_offset = offset_table[i];
		entry_length = offset_table[j] - offset_table[i];
		
		if (entry_length < 8388608) {
		  // decode
		  decode_amm(buf, start_offset, entry_length, i);
		}
		
	  }
	}	
  }  
   
  free(buf);
  fclose(fout);
  fclose(fin2);
  fclose(fin1);
  
}

